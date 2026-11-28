#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "app_tasks.h"

static DeviceData_Rx_t DeviceData_Batch;
static DeviceData_Rx_t DeviceData_SubmitBuffer;
static uint32_t DeviceData_BatchFirstTick = 0U;
static volatile uint8_t DeviceData_SubmitPending = 0U;
static volatile uint8_t DeviceData_SubmitBusy = 0U;

/* 尝试提交待发送块，队列满时立即返回失败，绝不阻塞持有状态。 */
static uint8_t DeviceData_TrySubmit(void)
{
    osStatus_t status;
    uint8_t can_submit = 0U;

    if (Queue_DeviceData_RxHandle == NULL)
    {
        return 0U;
    }

    taskENTER_CRITICAL();
    if ((DeviceData_SubmitPending != 0U) &&
        (DeviceData_SubmitBusy == 0U))
    {
        DeviceData_SubmitBusy = 1U;
        can_submit = 1U;
    }
    taskEXIT_CRITICAL();

    if (can_submit == 0U)
    {
        return (DeviceData_SubmitPending == 0U) ? 1U : 0U;
    }

    status = osMessageQueuePut(
        Queue_DeviceData_RxHandle,
        &DeviceData_SubmitBuffer,
        0U,
        0U
    );

    taskENTER_CRITICAL();
    if (status == osOK)
    {
        DeviceData_SubmitPending = 0U;
    }
    DeviceData_SubmitBusy = 0U;
    taskEXIT_CRITICAL();

    return (status == osOK) ? 1U : 0U;
}

/* 把当前批次移动到静态待提交缓冲。 */
static void DeviceData_MoveBatchToSubmitBuffer(void)
{
    DeviceData_SubmitBuffer = DeviceData_Batch;
    DeviceData_Batch.data_count = 0U;
    DeviceData_BatchFirstTick = 0U;
    DeviceData_SubmitPending = 1U;
}

/* 将单条设备数据加入共享批次，满8条后由静态队列复制整块。 */
void DeviceData_QueueSample(const DeviceData_Rx_Data_t *sample)
{
    uint8_t submit_needed;

    if ((sample == NULL) || (Queue_DeviceData_RxHandle == NULL))
    {
        return;
    }

    for (;;)
    {
        submit_needed = 0U;

        taskENTER_CRITICAL();
        if (DeviceData_SubmitPending == 0U)
        {
            if (DeviceData_Batch.data_count == 0U)
            {
                DeviceData_BatchFirstTick = osKernelGetTickCount();
            }

            DeviceData_Batch.data[DeviceData_Batch.data_count] = *sample;
            DeviceData_Batch.data_count++;

            if (DeviceData_Batch.data_count >= ITEM_PER_BLOCK)
            {
                DeviceData_MoveBatchToSubmitBuffer();
            }

            submit_needed = DeviceData_SubmitPending;
        }
        else
        {
            submit_needed = 1U;
        }
        taskEXIT_CRITICAL();

        if (submit_needed == 0U)
        {
            return;
        }

        if (DeviceData_TrySubmit() != 0U)
        {
            return;
        }

        osDelay(1U);
    }
}

/* 将尚未满8条的批次立即提交，队列满时保留待提交数据。 */
void DeviceData_FlushBatch(void)
{
    uint8_t submit_needed = 0U;

    taskENTER_CRITICAL();
    if (DeviceData_SubmitPending == 0U)
    {
        if (DeviceData_Batch.data_count == 0U)
        {
            taskEXIT_CRITICAL();
            return;
        }

        DeviceData_MoveBatchToSubmitBuffer();
    }
    submit_needed = 1U;
    taskEXIT_CRITICAL();

    if (submit_needed != 0U)
    {
        (void)DeviceData_TrySubmit();
    }
}

/* 从批次首条数据开始计时，超时后尝试非阻塞提交。 */
uint8_t DeviceData_FlushBatchIfExpired(uint32_t timeout_ms)
{
    uint32_t timeout_tick;
    uint32_t elapsed_tick;
    uint8_t submit_needed = 0U;

    taskENTER_CRITICAL();
    if ((DeviceData_SubmitPending == 0U) &&
        (DeviceData_Batch.data_count > 0U))
    {
        timeout_tick =
            ((timeout_ms * osKernelGetTickFreq()) + 999U) / 1000U;
        elapsed_tick =
            osKernelGetTickCount() - DeviceData_BatchFirstTick;

        if (elapsed_tick >= timeout_tick)
        {
            DeviceData_MoveBatchToSubmitBuffer();
        }
    }
    submit_needed = DeviceData_SubmitPending;
    taskEXIT_CRITICAL();

    if (submit_needed == 0U)
    {
        return 0U;
    }

    return DeviceData_TrySubmit();
}
