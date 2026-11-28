/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"

#include "app_tasks.h"
#include "app_message.h"
#include "lwip.h"
#include "lwip/errno.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#define JSON_PROTO_VER        1         //JSON协议版本
#define JSON_GW_ID            1         //网关ID
#define JSON_BUF_SIZE         2048      //JSON缓冲区大小(看情况可改)
#define TCP_SERVER_IP         "192.168.100.100" //服务器IP，按现场配置修改
#define TCP_SERVER_PORT       8080      //服务器TCP端口
#define TCP_CONNECT_TIMEOUT_MS 3000U    //短连接建立超时
#define TCP_SEND_TIMEOUT_MS   3000U     //JSON发送超时
#define TCP_RETRY_DELAY_MS    1000U     //网络失败后的重试间隔
#define BATCH_TIMEOUT_POLL_MS 100U      //零散批次超时检查周期

extern struct netif gnetif;

/*---------------------------------------------------------JSON序列化化------------------------------------------------------------- */

static uint32_t s_seq = 0;

/*平时阻塞,等待队列,如果超时(6s,使用任务软件计时器)则主动读取队列.*/

/*JSON格式模板
{
  "p": 1,                                   //JSON协议版本
  "gw": 1,                                  //网关ID
  "seq": 1024,                              //包序号，Task_Net 维护，每包 +1，用于丢包检测
  "ts": 1732435200,                         //上云时间,读TimeStamp这个全局变量,Core\Src\main.c 111 TimeStamp为首次初始化,后续由定时器控制自增
  "cnt": 5,                                 //包内具体数据条数量
  "items": [                                //以下均为包内具体数据(该任务不能修改数据里的值,数据本身从队列Queue_DeviceData_Rx里来)
    {
      "ts": 1732435199,                     
      "t": 1,                               //type,type值(纯数字)对应App\Inc\app_message.h 25-33枚举值,不同type有不同的JSON填充模板
      "id": 3,
      "len": 32,
      "d": {
        "ep": 12345678,
        "ua": 2205, "ub": 2211, "uc": 2198,
        "ia": 1234, "ib": 1201, "ic": 1198,
        "pt": 1, "ct": 40
      }
    },
    {
      "ts": 1732435199,
      "t": 5,
      "id": 0,
      "len": 4,
      "d": { "temp": 365 }
    }
  ]
}*/

typedef struct
{
    char *buffer;           //JSON输出缓冲区
    size_t capacity;        //总容量
    size_t used;            //一些字节数
    uint8_t ok;             //写入状态
} JSON_Writer_t;

static uint8_t JSON_Write(JSON_Writer_t *writer, const char *format, ...)
{
    va_list args;
    int written;
    size_t remaining;

    if ((writer == NULL) || (format == NULL) || (writer->ok == 0U))
    {
        return 0U;
    }

    remaining = writer->capacity - writer->used;
    if (remaining == 0U)
    {
        writer->ok = 0U;
        return 0U;
    }

    va_start(args, format);
    written = vsnprintf(
        &writer->buffer[writer->used],
        remaining,
        format,
        args
    );
    va_end(args);

    if ((written < 0) || ((size_t)written >= remaining))
    {
        writer->ok = 0U;
        return 0U;
    }

    writer->used += (size_t)written;
    return 1U;
}

static long JSON_Scale(float value, float scale)
{
    float scaled = value * scale;
    return (long)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
}

static uint8_t JSON_WriteItem(
    JSON_Writer_t *writer,
    const DeviceData_Rx_Data_t *item,
    uint8_t first)
{
    if (JSON_Write(
            writer,
            "%s{\"ts\":%lu,\"t\":%lu,\"id\":%u,\"len\":%u,\"d\":",
            (first != 0U) ? "" : ",",
            (unsigned long)item->timestamp,
            (unsigned long)item->dev_type,
            (unsigned int)item->dev_id,
            (unsigned int)item->data_len
        ) == 0U)
    {
        return 0U;
    }

    if (item->dev_type == DEV_TYPE_ACREL_METER)
    {
        return JSON_Write(
            writer,
            "{\"ep\":%ld,\"ua\":%ld,\"ub\":%ld,\"uc\":%ld,"
            "\"ia\":%ld,\"ib\":%ld,\"ic\":%ld,\"pt\":%u,\"ct\":%u}}",
            JSON_Scale(item->payload.Acrel.total_energy_kwh, 100.0f),
            JSON_Scale(item->payload.Acrel.Ua_v, 10.0f),
            JSON_Scale(item->payload.Acrel.Ub_v, 10.0f),
            JSON_Scale(item->payload.Acrel.Uc_v, 10.0f),
            JSON_Scale(item->payload.Acrel.Ia_a, 1000.0f),
            JSON_Scale(item->payload.Acrel.Ib_a, 1000.0f),
            JSON_Scale(item->payload.Acrel.Ic_a, 1000.0f),
            (unsigned int)item->payload.Acrel.pt,
            (unsigned int)item->payload.Acrel.ct
        );
    }

    if (item->dev_type == DEV_TYPE_GATEWAY_SELF)
    {
        return JSON_Write(
            writer,
            "{\"temp\":%ld}}",
            JSON_Scale(item->payload.GatewaySelf.temperature_c, 10.0f)
        );
    }

    return JSON_Write(writer, "{}");
}

int json_fill_block(char *buf, int size,
                    const DeviceData_Rx_t *blk,
                    uint32_t seq)
{
    JSON_Writer_t writer;
    uint32_t index;
    uint32_t count;

    if ((buf == NULL) || (blk == NULL) || (size <= 0))
    {
        return 0;
    }

    count = blk->data_count;
    if (count > ITEM_PER_BLOCK)
    {
        count = ITEM_PER_BLOCK;
    }

    writer.buffer = buf;
    writer.capacity = (size_t)size;
    writer.used = 0U;
    writer.ok = 1U;

    if (JSON_Write(
            &writer,
            "{\"p\":%u,\"gw\":%u,\"seq\":%lu,\"ts\":%lu,\"cnt\":%lu,\"items\":[",
            (unsigned int)JSON_PROTO_VER,
            (unsigned int)JSON_GW_ID,
            (unsigned long)seq,
            (unsigned long)TimeStamp,
            (unsigned long)count
        ) == 0U)
    {
        return 0;
    }

    for (index = 0U; index < count; index++)
    {
        if (JSON_WriteItem(&writer, &blk->data[index], (index == 0U)) == 0U)
        {
            return 0;
        }
    }

    if (JSON_Write(&writer, "]}") == 0U)
    {
        return 0;
    }

    return (int)writer.used;
}




/*---------------------------------------------------------TCP Client------------------------------------------------------------- */
/* TCP_Client初始化函数 */
uint8_t TCP_Client_Init()
{
    /*短连接不保存Socket，只确认LwIP队列接口可用。 */
    return ((Queue_DeviceData_RxHandle != NULL) &&
            (gnetif.name[0] != 0U)) ? 1U : 0U;
}

/* TCP_Client 网络就绪函数 */
uint8_t  Net_is_Ready()
{
    return ((netif_is_up(&gnetif) != 0U) &&
            (netif_is_link_up(&gnetif) != 0U)) ? 1U : 0U;
}         

static uint8_t TCP_Client_Connect(int socket_fd)
{
    struct sockaddr_in server_addr;
    fd_set write_set;
    struct timeval timeout;
    int result;
    int socket_error = 0;
    socklen_t error_length = sizeof(socket_error);
    int nonblocking = 1;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(TCP_SERVER_IP);

    if (lwip_ioctl(socket_fd, FIONBIO, &nonblocking) < 0)
    {
        return 0U;
    }

    result = lwip_connect(
        socket_fd,
        (const struct sockaddr *)&server_addr,
        sizeof(server_addr)
    );

    if ((result < 0) && (errno != EINPROGRESS))
    {
        return 0U;
    }

    if (result < 0)
    {
        FD_ZERO(&write_set);
        FD_SET(socket_fd, &write_set);
        timeout.tv_sec = TCP_CONNECT_TIMEOUT_MS / 1000U;
        timeout.tv_usec =
            (TCP_CONNECT_TIMEOUT_MS % 1000U) * 1000U;

        result = lwip_select(
            socket_fd + 1,
            NULL,
            &write_set,
            NULL,
            &timeout
        );
        if ((result <= 0) || (FD_ISSET(socket_fd, &write_set) == 0))
        {
            return 0U;
        }

        if ((lwip_getsockopt(
                 socket_fd,
                 SOL_SOCKET,
                 SO_ERROR,
                 &socket_error,
                 &error_length
             ) < 0) ||
            (socket_error != 0))
        {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t TCP_Client_SendAll(
    int socket_fd,
    const char *data,
    uint16_t length)
{
    uint16_t sent = 0U;
    fd_set write_set;
    struct timeval timeout;
    int result;
    int send_result;

    while (sent < length)
    {
        FD_ZERO(&write_set);
        FD_SET(socket_fd, &write_set);
        timeout.tv_sec = TCP_SEND_TIMEOUT_MS / 1000U;
        timeout.tv_usec =
            (TCP_SEND_TIMEOUT_MS % 1000U) * 1000U;

        result = lwip_select(
            socket_fd + 1,
            NULL,
            &write_set,
            NULL,
            &timeout
        );
        if ((result <= 0) || (FD_ISSET(socket_fd, &write_set) == 0))
        {
            return 0U;
        }

        send_result = lwip_send(
            socket_fd,
            &data[sent],
            (size_t)(length - sent),
            0
        );
        if (send_result > 0)
        {
            sent = (uint16_t)(sent + (uint16_t)send_result);
        }
        else if ((send_result < 0) &&
                 ((errno == EWOULDBLOCK) || (errno == EAGAIN)))
        {
            continue;
        }
        else
        {
            return 0U;
        }
    }

    return 1U;
}

/* TCP_Client发送函数 */
uint8_t TCP_Client_Send_Data(const char *data, uint16_t length)
{
    uint8_t send_ok = 0;
    int socket_fd;

    if ((data == NULL) || (length == 0U) || (Net_is_Ready() == 0U))
    {
        return 0U;
    }

    socket_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        return 0U;
    }

    if (TCP_Client_Connect(socket_fd) != 0U)
    {
        send_ok = TCP_Client_SendAll(socket_fd, data, length);
    }

    (void)lwip_shutdown(socket_fd, SHUT_RDWR);
    (void)lwip_close(socket_fd);

    return send_ok;//成功为1.失败为0
}

/*---------------------------------------------------------Task_Net_Run------------------------------------------------------------- */
void Task_Net_Run(void *argument)
{
    static DeviceData_Rx_t blk;
    static char json_buf[JSON_BUF_SIZE];
    static int json_len;
    static uint8_t block_pending;
    osStatus_t queue_status;

    (void)argument;

    /* 等待DefaultTask完成MX_LWIP_Init。 */
    while (TCP_Client_Init() == 0U)
    {
        osDelay(500U);
    }

    for (;;)
    {
        /* 零散批次从首条数据开始计算6秒，而不是从最近整包到达时计时。 */
        (void)DeviceData_FlushBatchIfExpired(6000U);

        if (block_pending == 0U)
        {
            queue_status = osMessageQueueGet(
                Queue_DeviceData_RxHandle,
                &blk,
                NULL,
                pdMS_TO_TICKS(BATCH_TIMEOUT_POLL_MS)
            );

            if (queue_status != osOK)
            {
                continue;
            }

            block_pending = 1U;
            json_len = 0;
        }

        if (json_len <= 0)
        {
            json_len = json_fill_block(
                json_buf,
                sizeof(json_buf),
                &blk,
                s_seq
            );
            if (json_len <= 0)
            {
                /* 保留当前数据块并重试，不能静默丢弃。 */
                osDelay(TCP_RETRY_DELAY_MS);
                continue;
            }
        }

        if (TCP_Client_Send_Data(json_buf, (uint16_t)json_len) == 0U)
        {
            /* 发送失败时保留当前JSON原地重试，保持数据和seq顺序。 */
            osDelay(TCP_RETRY_DELAY_MS);
            continue;
        }

        s_seq++;
        block_pending = 0U;
        json_len = 0;
    }
}
