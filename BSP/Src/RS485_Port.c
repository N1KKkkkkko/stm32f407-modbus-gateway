/***********************************************************************
 * Layer: Hardware Link / Modbus Transport
 *
 * This file owns the physical USART2/RS485 path:
 *   USART2 DMA -> DMA/IDLE ISR -> software ring buffer -> parser task
 *
 * It does not parse Modbus frames. It only provides a reliable byte stream
 * to nanoMODBUS and controls the RS485 transceiver direction pin.
 ***********************************************************************/

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "main.h"
#include "RS485_Port.h"
#include "usart.h"

#define RS485_DMA_BUFFER_SIZE 258U
#define RS485_RING_BUFFER_SIZE 512U
#define RS485_RX_READY_FLAG (1UL << 0U)

static uint8_t RS485_dma_buffer[RS485_DMA_BUFFER_SIZE];
static uint8_t RS485_ring_buffer[RS485_RING_BUFFER_SIZE];
static volatile uint16_t RS485_ring_head = 0U;
static volatile uint16_t RS485_ring_tail = 0U;
static volatile uint8_t RS485_uart_error = 0U;
static osThreadId_t RS485_parser_task = NULL;

/* Link-layer helpers -------------------------------------------------------*/

/**
 * @brief 在中断上下文把DMA新增字节追加到软件环形缓冲。
 * @note  仅生产数据，不解析Modbus；缓冲满时置链路错误。
 */
static void RS485_RingPushFromISR(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    uint16_t next_head;

    for (index = 0U; index < length; index++)
    {
        next_head = (uint16_t)((RS485_ring_head + 1U) %
                               RS485_RING_BUFFER_SIZE);

        if (next_head == RS485_ring_tail)       
        {
            RS485_uart_error = 1U;
            break;
        }

        RS485_ring_buffer[RS485_ring_head] = data[index];
        RS485_ring_head = next_head;
    }
}

/**
 * @brief 在任务上下文从软件环形缓冲取出一个字节。
 * @return 1表示取到字节，0表示当前无数据。
 */
static uint8_t RS485_RingPop(uint8_t *data)
{
    if (RS485_ring_tail == RS485_ring_head)
    {
        return 0U;
    }

    *data = RS485_ring_buffer[RS485_ring_tail];
    __DMB();
    RS485_ring_tail = (uint16_t)((RS485_ring_tail + 1U) %
                                 RS485_RING_BUFFER_SIZE);
    return 1U;
}

/**
 * @brief 清除Task_RS485积累的接收任务通知。
 * @note  用于链路恢复或开始新Modbus事务前清理旧事件。
 */
static void RS485_DrainRxSignal(void)
{
    (void)osThreadFlagsClear(RS485_RX_READY_FLAG);
}

/**
 * @brief 在中断上下文通知Task_RS485有新的链路字节可消费。
 * @note  只发送一个任务通知，不携带字节内容。
 */
static void RS485_SignalRxFromISR(void)
{
    if (RS485_parser_task != NULL)
    {
        (void)osThreadFlagsSet(
            RS485_parser_task,
            RS485_RX_READY_FLAG
        );
    }
}

/**
 * @brief 启动USART2 DMA+IDLE接收。
 * @return 1表示启动成功，0表示启动失败。
 */
static uint8_t RS485_StartReceive(void)
{
    return (HAL_UARTEx_ReceiveToIdle_DMA(
                &huart2,
                RS485_dma_buffer,
                RS485_DMA_BUFFER_SIZE
            ) == HAL_OK) ? 1U : 0U;
}

/* UART/DMA ISR callbacks ---------------------------------------------------*/

/**
 * @brief DMA 半满 HT、DMA 全满 TC、UART IDLE 这三类中断，统一收敛到 UARTEx 这个事件回调统一处理
 * @note  根据DMA增量位置复制字节到环形缓冲，并通知解析任务；
 *        不区分事件类型，Modbus帧解析由任务上下文完成。
 */
/* 原循环DMA说明保留：
 * 新数据末尾位置，因为是环形缓冲，所以用 % 做到取新数据末尾位置
 * 没绕圈
 * 绕了一圈
 * 更新last_position
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) //Size 从 DMA 接收启动开始，到这次事件触发时，一共收到的总字节数
{
    uint16_t length;

    if (huart->Instance != USART2)
    {
        return;
    }

    /* Normal DMA在IDLE或TC时提交完整接收段，半满HT不提前消费。 */
    if (huart->RxEventType == HAL_UART_RXEVENT_HT)
    {
        return;
    }

    length = Size; //Size表示本次DMA接收段的有效字节数

    if (length > RS485_DMA_BUFFER_SIZE)
    {
        RS485_uart_error = 1U;
        RS485_SignalRxFromISR();
        return;
    }

    if (length > 0U)
    {
        RS485_RingPushFromISR(RS485_dma_buffer, length);
        RS485_SignalRxFromISR(); //ISR中断中任务通知给Task_RS485.
    }

    /* IDLE或TC后重新启动下一次Normal DMA接收。 */
    if (RS485_StartReceive() == 0U)
    {
        RS485_uart_error = 1U;
        RS485_SignalRxFromISR();
    }
}

/**
 * @brief 记录USART2链路错误并通知解析任务。
 * @note  实际Abort和DMA重启在Task_RS485上下文中执行。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        RS485_uart_error = 1U;
        RS485_SignalRxFromISR();
    }
}

/* Public link-layer API ----------------------------------------------------*/

/**
 * @brief 注册消费环形缓冲的Task_RS485任务句柄。
 * @note  ISR使用该句柄发送数据到达任务通知。
 */
void RS485_PortSetParserTask(osThreadId_t parser_task)
{
    RS485_parser_task = parser_task;
}

/**
 * @brief 初始化RS485链路层缓冲并启动USART2 DMA接收。
 * @return 1表示成功，0表示失败。
 */
uint8_t RS485_PortInit(void)
{
    taskENTER_CRITICAL();
    RS485_ring_head = 0U;
    RS485_ring_tail = 0U;
    RS485_uart_error = 0U;
    taskEXIT_CRITICAL();

    RS485_DrainRxSignal();
    return RS485_StartReceive();
}

/**
 * @brief 在任务上下文恢复USART2接收链路。
 * @note  清除链路错误和旧缓冲后重新启动DMA。
 */
uint8_t RS485_PortRecover(void)
{
    if (RS485_uart_error == 0U)
    {
        return 1U;
    }

    (void)HAL_UART_AbortReceive(&huart2);

    taskENTER_CRITICAL();
    RS485_ring_head = 0U;
    RS485_ring_tail = 0U;
    RS485_uart_error = 0U;
    taskEXIT_CRITICAL();

    RS485_DrainRxSignal();
    return RS485_StartReceive();
}

/**
 * @brief 丢弃环形缓冲中的旧字节和接收任务通知。
 * @note  供nanoMODBUS开始新请求前清理线路残留数据。
 */
void RS485_PortFlush(void)
{
    taskENTER_CRITICAL();
    RS485_ring_head = 0U;       
    RS485_ring_tail = 0U;
    taskEXIT_CRITICAL();

    RS485_DrainRxSignal();
}

/**
 * @brief 计算Modbus RTU两帧之间所需的最小静默时间。
 * @note  波特率高于19200时使用固定1.75ms，其余情况使用3.5个字符时间。
 * @return 转换为FreeRTOS tick的静默时间，最小为1 tick。
 */
uint32_t RS485_PortGetSilentTimeTicks(void)
{
    uint32_t baud_rate = huart2.Init.BaudRate;
    uint32_t silent_time_us;
    uint32_t tick_frequency = osKernelGetTickFreq();
    uint64_t tick_count;

    if (baud_rate == 0U)
    {
        return 1U;
    }

    if (baud_rate > 19200U)
    {
        silent_time_us = 1750U;
    }
    else
    {
        silent_time_us =                              //字符时间依赖UART硬件帧位数而不是Modbus APU
            (38500000U + baud_rate - 1U) / baud_rate; //这里默认帧是11bit,uart帧 1 起始 + 8 数据 + 1 校验 + 1 停止
    }

    tick_count =
        ((uint64_t)silent_time_us * tick_frequency + 999999ULL) /
        1000000ULL;

    if (tick_count == 0ULL)
    {
        return 1U;
    }

    return (uint32_t)tick_count;
}

/**
 * @brief 从链路层环形缓冲读取指定数量的字节。
 * @note  运行在Task_RS485上下文；无数据时按byte_timeout_ms阻塞等待任务通知。
 * @return 实际读取字节数，负数表示链路读取失败。
 */
int32_t RS485_PortRead(
    uint8_t *buffer,
    uint16_t count,
    int32_t byte_timeout_ms,
    void *argument)
{
    uint16_t received = 0U;
    uint32_t wait_ticks;
    uint32_t flags;
    uint32_t tick_frequency;

    (void)argument;

    while (received < count)
    {
        if (RS485_RingPop(&buffer[received]) != 0U)
        {
            received++;
            continue;
        }

        if (RS485_uart_error != 0U)
        {
            return -1;
        }

        if (byte_timeout_ms == 0)
        {
            break;
        }

        if (byte_timeout_ms < 0)
        {
            wait_ticks = osWaitForever;
        }
        else
        {
            tick_frequency = osKernelGetTickFreq();
            wait_ticks =
                ((uint32_t)byte_timeout_ms * tick_frequency + 999U) /
                1000U;
        }

        if ((byte_timeout_ms > 0) && (wait_ticks == 0U))
        {
            wait_ticks = 1U;
        }

        flags = osThreadFlagsWait(
            RS485_RX_READY_FLAG,
            osFlagsWaitAny,
            wait_ticks
        );

        if ((flags & RS485_RX_READY_FLAG) == 0U)
        {
            break;
        }
    }

    return (int32_t)received;
}

/**
 * @brief 控制RS485收发方向并发送一帧原始字节。
 * @note  发送前拉高DE，等待UART_FLAG_TC后拉低DE。
 * @return 实际发送字节数，负数表示发送失败。
 */
int32_t RS485_PortWrite(
    const uint8_t *buffer,
    uint16_t count,
    int32_t byte_timeout_ms,
    void *argument)
{
    uint32_t timeout_ms;
    uint32_t start_tick;
    uint8_t transmit_complete = 0U;

    (void)argument;
    (void)byte_timeout_ms;

    timeout_ms =
        ((uint32_t)count * 10U * 1000U) /
        (uint32_t)huart2.Init.BaudRate;
    timeout_ms += 50U;

    HAL_GPIO_WritePin(
        RS485_DE_NRE_GPIO_Port,
        RS485_DE_NRE_Pin,
        GPIO_PIN_SET
    );

    if (HAL_UART_Transmit(
            &huart2,
            (uint8_t *)buffer,
            count,
            timeout_ms
        ) != HAL_OK)        //传输失败
    {
        HAL_GPIO_WritePin(
            RS485_DE_NRE_GPIO_Port,
            RS485_DE_NRE_Pin,
            GPIO_PIN_RESET
        );
        return -1;
    }

    start_tick = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - start_tick) > 100U)
        {
            break;
        }
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) != RESET)
    {
        transmit_complete = 1U;
    }

    HAL_GPIO_WritePin(
        RS485_DE_NRE_GPIO_Port,
        RS485_DE_NRE_Pin,
        GPIO_PIN_RESET
    );

    return (transmit_complete != 0U) ? (int32_t)count : -1;
}
