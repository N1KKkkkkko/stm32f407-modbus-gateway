/***********************************************************************
文件名称： Task_RS485.c
功    能： Modbus RTU主站服务任务
编写时间：2026-11-18
编 写 人：N1ghtoo
注    意：通过Queue_Modbus_Request接收请求，通过Queue_Modbus_Respond返回响应
***********************************************************************/

#include "cmsis_os.h"

#include "app_tasks.h"
#include "nanomodbus.h"
#include "RS485_Port.h"

#define RS485_DEFAULT_RESPONSE_TIMEOUT_MS 1000
#define RS485_DEFAULT_BYTE_TIMEOUT_MS 2

static nmbs_t RS485_nmbs;
static uint32_t RS485_last_finish_tick = 0U;
static uint8_t RS485_has_last_frame = 0U;

static void RS485_TransportFlush(nmbs_t *nmbs, void *argument);
static void RS485_ExecuteRequest(
    const RS485_Request_t *request,
    RS485_Response_t *response
);
static void RS485_SetResponseError(
    RS485_Response_t *response,
    int32_t error_code)
{
    if (response == NULL)
    {
        return;
    }

    response->status = RS485_STATUS_ERROR;
    response->error_code = error_code;
    response->register_count = 0U;
}

/**
 * @brief nanoMODBUS flush 适配函数。
 * @note  应用/Modbus层调用，丢弃链路层环形缓冲中的旧字节。
 */
static void RS485_TransportFlush(nmbs_t *nmbs, void *argument)
{
    (void)nmbs;
    (void)argument;
    RS485_PortFlush();
}

/* Application/Modbus request execution ------------------------------------*/

/**
 * @brief Execute one request in the Task_RS485 task context.
 * @note  nanoMODBUS stores validated register data in response->registers.
 *        The raw response frame remains inside RS485_nmbs.msg.buf.
 */
static void RS485_ExecuteRequest(
    const RS485_Request_t *request,
    RS485_Response_t *response)
{
    nmbs_error error;

    if ((request == NULL) || (response == NULL) ||
        (request->register_count > RS485_MAX_REGISTERS))
    {
        RS485_SetResponseError(response, NMBS_ERROR_INVALID_ARGUMENT);
        return;
    }//错误判断

    response->status = RS485_STATUS_ERROR;
    response->error_code = NMBS_ERROR_INVALID_ARGUMENT;
    response->register_count = 0U;

    nmbs_set_destination_rtu_address(
        &RS485_nmbs,
        request->slave_address
    );

    switch (request->function)
    {
        case MODBUS_FUNCTION_READ_HOLDING:
            error = nmbs_read_holding_registers(
                &RS485_nmbs,
                request->register_address,
                request->register_count,
                response->registers
            );
            break;

        case MODBUS_FUNCTION_READ_INPUT:
            error = nmbs_read_input_registers(
                &RS485_nmbs,
                request->register_address,
                request->register_count,
                response->registers
            );
            break;

        case MODBUS_FUNCTION_WRITE_SINGLE_REG:
            error = nmbs_write_single_register(
                &RS485_nmbs,
                request->register_address,
                request->write_value
            );
            break;

        default:
            error = NMBS_ERROR_INVALID_ARGUMENT;
            break;
    }

    response->error_code = (int32_t)error;

    if (error == NMBS_ERROR_NONE)
    {
        response->status = RS485_STATUS_OK;
        response->register_count =
            (request->function == MODBUS_FUNCTION_READ_HOLDING) ||
            (request->function == MODBUS_FUNCTION_READ_INPUT)
                ? request->register_count
                : 0U;
    }
    else
    {
        RS485_SetResponseError(response, (int32_t)error);
    }
}

/* Task/service initialization ---------------------------------------------*/

/**
 * @brief 初始化nanoMODBUS Client、任务队列和RS485硬件链路。
 * @return 1表示成功，0表示失败并允许任务重试。
 */
static uint8_t RS485_ModbusServiceInit(void)
{
    nmbs_platform_conf platform;

    if ((Queue_Modbus_RequestHandle == NULL) ||
        (Queue_Modbus_RespondHandle == NULL))
    {
        return 0U;
    }

    nmbs_platform_conf_create(&platform);
    platform.transport = NMBS_TRANSPORT_RTU;    //使用的协议类型
    platform.read = RS485_PortRead;             //读函数需要对应硬件驱动的读函数
    platform.write = RS485_PortWrite;           //写函数需要对应硬件驱动的写函数
    platform.flush = RS485_TransportFlush;      //丢弃链路层环形缓冲中的旧字节
    platform.arg = NULL;

    if (nmbs_client_create(&RS485_nmbs, &platform) != NMBS_ERROR_NONE)
    {
        return 0U;
    }

    nmbs_set_read_timeout(
        &RS485_nmbs,
        RS485_DEFAULT_RESPONSE_TIMEOUT_MS
    );
    nmbs_set_byte_timeout(
        &RS485_nmbs,
        RS485_DEFAULT_BYTE_TIMEOUT_MS
    );

    if (RS485_PortInit() == 0U)
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief Task_RS485主循环。
 * @note  独占RS485总线，串行处理请求；没有请求时阻塞在Queue_Modbus_Request。
 */
void Task_RS485_Run(void *argument)
{
    RS485_Request_t request;
    RS485_Response_t response;

    (void)argument;

    RS485_PortSetParserTask(osThreadGetId());

    while (RS485_ModbusServiceInit() == 0U)     //协议栈初始化循环，成功则退出循环
    {
        osDelay(1000U);
    }

    for (;;)
    {
        /*
         * Task_Pollin -> Queue_Modbus_Request -> Task_RS485.平时阻塞等待Pollin请求(Queue_Modbus_Request消息队列)
         */
        if (osMessageQueueGet(
                Queue_Modbus_RequestHandle,
                &request,
                NULL,
                osWaitForever
            ) != osOK)
        {
            continue;
        }

        /* 接到请求 */
        response = (RS485_Response_t){
            .status = RS485_STATUS_ERROR
        };

        if (RS485_PortRecover() == 0U)      //启动DMA失败
        {
            RS485_SetResponseError(
                &response,
                NMBS_ERROR_TRANSPORT
            );
        }
        else                                //启动成功
        {
            /* RS485总线唯一执行者统一保证相邻Modbus帧的最小静默时间。 */
            if (RS485_has_last_frame != 0U)
            {
                uint32_t now_tick = osKernelGetTickCount();
                uint32_t silent_tick = RS485_PortGetSilentTimeTicks();
                uint32_t elapsed_tick = now_tick - RS485_last_finish_tick;

                if (elapsed_tick < silent_tick)
                {
                    osDelay(silent_tick - elapsed_tick);
                }
            }

            RS485_ExecuteRequest(
                &request,
                &response
            );
            RS485_last_finish_tick = osKernelGetTickCount();
            RS485_has_last_frame = 1U;
        }

        /*
         * Task_RS485 -> Queue_Modbus_Respond -> Task_Pollin.
         * osMessageQueuePut copies the complete response into queue storage.
         */
        (void)osMessageQueuePut(
            Queue_Modbus_RespondHandle,
            &response,
            0U,
            osWaitForever
        );
    }
}
