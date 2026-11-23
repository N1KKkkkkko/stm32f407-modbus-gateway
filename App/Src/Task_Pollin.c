/***********************************************************************
文件名称： Task_Pollin.c
功    能： 轮询任务
编写时间：2026-11-20
编 写 人：N1ghtoo
注    意:RS485_PackRawRegisters
         RS485_PollDeviceOnce                  
         RS485_FindDueDevice            遍历线性数组,并判断是否符合最小轮询周期,符合就进入进入轮询,不符合则跳过
         RS485_PublishDeviceData        将轮询结果发布到Queue_DeviceData_Rx          
         Task_Pollin_Run                freertos任务入口函数
***********************************************************************/

#include "cmsis_os.h"

#include "app_tasks.h"
#include "nanomodbus.h"
#include "RS485_Port.h"
#include "SlaveMachine_form.h"

#define RS485_POLL_MAX_RETRIES 3U       //最大重试次数
#define RS485_POLL_RETRY_DELAY_MS 100U
#define RS485_POLL_IDLE_DELAY_MS 10U

/* 轮询状态结构体 */
typedef struct
{                           
    uint32_t next_poll_tick;            // 下次轮询最小时间戳,大于这个时间戳就可以轮询了
    uint8_t function_index;             // 当前设备模板中的功能索引
    uint8_t retry_count;                // 轮询失败重试次数,RS485_POLL_MAX_RETRIES 最大重试次数 超过就放弃本次轮询
    uint8_t online;                     // 设备在线状态,1:在线,0:离线       
    int32_t last_error;                 // 保存上次返回错误结果状态,1 ：通信失败 , -1 ：配置/参数不合法
    union RS485_DeviceRuntime_t device_runtime;       //运行态参数(解析函数计算时会用到)
} RS485_SlaveMachineState_t;            

static RS485_SlaveMachineState_t
    RS485_SlaveMachineStates[RS485_SLAVE_MACHINE_COUNT];

static uint32_t RS485_PollCursor = 0U;

/* 当前设备只使用专用payload和专用解析函数，通用原始负载路径不参与编译。 */
#if 0
/**
 * @brief Default register packer.
 * @note  Stores host-order uint16 registers as big-endian bytes in payload.
 *        Device-specific scaling/float order should use pack_data callback.
 */
static void RS485_PackRawRegisters(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data)
{
    uint16_t index;

    if ((registers == NULL) ||
        (device_data == NULL) ||
        ((uint32_t)register_count * 2U > MAX_PAYLOAD_LEN))
    {
        return;
    }

    for (index = 0U; index < register_count; index++)
    {
        device_data->payload.Byte_val[index * 2U] =
            (uint8_t)(registers[index] >> 8U);
        device_data->payload.Byte_val[index * 2U + 1U] =
            (uint8_t)(registers[index] & 0xFFU);
    }

    device_data->data_len = (uint8_t)(register_count * 2U);
}
#endif

/**
 * @brief Dispatch one polling request according to the configured Modbus
 *        function code.
 * @note  This function selects the operation only. Task_RS485 owns request
 *        framing, RS485 transmission, response timeout and CRC checking.
 * @return RS485_STATUS_OK on success. The detailed failure code is stored
 *         in response->error_code.
 */
static RS485_Status_t RS485_PollDeviceOnce(
    const RS485_SlaveMachine_t *device,
    const RS485_FuncTemplate_t *function,
    RS485_Response_t *response)
{
    RS485_Request_t request;

    if ((device == NULL) ||
        (device->profile == NULL) ||
        (function == NULL) ||
        (response == NULL))
    {
        return RS485_STATUS_ERROR;
    }

    if ((Queue_Pollin_TxHandle == NULL) ||
        (Queue_RS485_TxHandle == NULL))
    {
        response->status = RS485_STATUS_ERROR;
        response->error_code = NMBS_ERROR_TRANSPORT;
        response->register_count = 0U;
        return RS485_STATUS_ERROR;
    }

    if ((function->function != MODBUS_FUNCTION_READ_HOLDING) &&
        (function->function != MODBUS_FUNCTION_READ_INPUT))
    {
        response->status = RS485_STATUS_ERROR;
        response->error_code = NMBS_ERROR_INVALID_ARGUMENT;
        response->register_count = 0U;
        return RS485_STATUS_ERROR;
    }

    request = (RS485_Request_t){
        .slave_address = device->slave_address,
        .function = function->function,
        .register_address = function->start_addr,
        .register_count = function->quantity,
        .write_value = 0U
    };

    if (osMessageQueuePut(
            Queue_Pollin_TxHandle,
            &request,
            0U,
            osWaitForever
        ) != osOK)
    {
        response->status = RS485_STATUS_ERROR;
        response->error_code = NMBS_ERROR_TRANSPORT;
        response->register_count = 0U;
        return RS485_STATUS_ERROR;
    }

    if (osMessageQueueGet(
            Queue_RS485_TxHandle,
            response,
            NULL,
            osWaitForever
        ) != osOK)
    {
        response->status = RS485_STATUS_ERROR;
        response->error_code = NMBS_ERROR_TRANSPORT;
        response->register_count = 0U;
        return RS485_STATUS_ERROR;
    }

    return response->status;
}

/**
 * @brief Find the first enabled device whose polling deadline has expired.
 * @return Device index, or -1 when no device is due.
 */
static int32_t RS485_FindDueDevice(uint32_t start_tick)
{
    uint32_t index;
    uint32_t offset;

    for (offset = 0U; offset < RS485_SLAVE_MACHINE_COUNT; offset++)
    {
        index = (RS485_PollCursor + offset) % RS485_SLAVE_MACHINE_COUNT;

        if ((RS485_SlaveMachines[index].enabled != 0U) &&
            ((int32_t)(start_tick -
                       RS485_SlaveMachineStates[index].next_poll_tick) >= 0))
        {
            RS485_PollCursor =
                (index + 1U) % RS485_SLAVE_MACHINE_COUNT;
            return (int32_t)index;
        }
    }

    return -1;
}

/**
 * @brief Publish one successful poll result to Queue_DeviceData_Rx.
 */
static void RS485_PublishDeviceData(
    const RS485_SlaveMachine_t *device,
    const RS485_FuncTemplate_t *function,
    const RS485_Response_t *response,
    union RS485_DeviceRuntime_t *device_runtime)
{
    DeviceData_Rx_t device_data = {0};

    if ((device == NULL) ||
        (function == NULL) ||
        (function->pack_data == NULL) ||
        (response == NULL) ||
        (device_runtime == NULL) ||
        (response->status != RS485_STATUS_OK) ||
        (response->register_count == 0U))
    {
        return;
    }

    device_data.timestamp = TimeStamp;
    device_data.dev_type = device->profile->device_type;
    device_data.dev_id = device->slave_address;

    function->pack_data(
        response->registers,
        response->register_count,
        &device_data,
        device_runtime
    );

    if (device_data.data_len > 0U)
    {
        (void)osMessageQueuePut(
            Queue_DeviceData_RxHandle,
            &device_data,
            0U,
            0U
        );
    }
}

/* 保证相邻功能请求之间满足Modbus RTU最小静默时间。 */
static void RS485_WaitMinimumSilentTime(
    uint32_t finish_func_time,
    uint8_t has_previous_frame,
    uint32_t *star_func_time)
{
    uint32_t now_tick;
    uint32_t elapsed_tick;
    uint32_t silent_tick;

    if ((has_previous_frame != 0U) && (star_func_time != NULL))
    {
        silent_tick = RS485_PortGetSilentTimeTicks();
        now_tick = osKernelGetTickCount();
        elapsed_tick = now_tick - finish_func_time;

        if (elapsed_tick < silent_tick)
        {
            osDelay(silent_tick - elapsed_tick);
        }
    }

    if (star_func_time != NULL)
    {
        *star_func_time = osKernelGetTickCount();
    }
}

/**
 * @brief Polling scheduler task.
 * @note  No TIM interrupt is used. After each response/failure the next
 *        due device is selected by elapsed time.
 */
void Task_Pollin_Run(void *argument)
{
    RS485_Response_t response;
    uint32_t start_tick;
    uint32_t finish_tick;
    uint32_t star_func_time = 0U;
    uint32_t finish_func_time = 0U;
    uint8_t has_previous_frame = 0U;
    int32_t device_index;
    RS485_Status_t result;  /* 0: OK, 1: error; details in response.error_code */
    const RS485_SlaveMachine_t *device;
    const DeviceTypeProfile_t *profile;
    const RS485_FuncTemplate_t *function;
    RS485_SlaveMachineState_t *device_state;

    (void)argument;

    for (;;)
    {
        start_tick = osKernelGetTickCount();
        device_index = RS485_FindDueDevice(start_tick);

        if (device_index < 0)
        {
            osDelay(RS485_POLL_IDLE_DELAY_MS);
            continue;
        }

        device = &RS485_SlaveMachines[device_index];
        device_state = &RS485_SlaveMachineStates[device_index];
        response = (RS485_Response_t){
            .status = RS485_STATUS_ERROR
        };
        profile = device->profile;
        function = NULL;

        if ((profile == NULL) ||
            (profile->func_table == NULL) ||
            (profile->func_count == 0U) ||
            (profile->max_regs_per_func == 0U))
        {
            response.status = RS485_STATUS_ERROR;
            response.error_code = NMBS_ERROR_INVALID_ARGUMENT;
            response.register_count = 0U;
            result = RS485_STATUS_ERROR;
        }
        else
        {
            if (device_state->function_index >= profile->func_count)
            {
                device_state->function_index = 0U;
            }

            function = &profile->func_table[device_state->function_index];

            if ((function->quantity == 0U) ||
                (function->quantity > profile->max_regs_per_func) ||
                (function->quantity > RS485_MAX_REGISTERS))
            {
                response.status = RS485_STATUS_ERROR;
                response.error_code = NMBS_ERROR_INVALID_ARGUMENT;
                response.register_count = 0U;
                result = RS485_STATUS_ERROR;
            }
            else
            {
                RS485_WaitMinimumSilentTime(
                    finish_func_time,
                    has_previous_frame,
                    &star_func_time
                );
                result = RS485_PollDeviceOnce(device, function, &response);
                finish_func_time = osKernelGetTickCount();
                has_previous_frame = 1U;
            }
        }

        finish_tick = osKernelGetTickCount();

        if (result == RS485_STATUS_OK) //状态正常
        {
            device_state->online = 1U;       
            device_state->retry_count = 0U;
            RS485_PublishDeviceData(
                device,
                function,
                &response,
                &device_state->device_runtime
            );

            device_state->function_index++;
            if (device_state->function_index >= profile->func_count)
            {
                device_state->function_index = 0U;
                device_state->next_poll_tick =
                    finish_tick +
                    ((device->poll_interval_ms *
                      osKernelGetTickFreq()) / 1000U);
            }
            else
            {
                /* Continue this device's remaining function templates. */
                device_state->next_poll_tick =
                    finish_func_time + RS485_PortGetSilentTimeTicks();
            }
        }
        else //状态异常
        {
            /* 轮询失败,记录错误码,设置设备离线状态,并根据重试次数决定是否重试或跳过 */
            {
                device_state->last_error = response.error_code;
                device_state->online = 0U;

                if (response.error_code != NMBS_ERROR_INVALID_ARGUMENT 
                && device_state->retry_count < RS485_POLL_MAX_RETRIES)  //可重试
                {
                    device_state->retry_count++;
                    device_state->next_poll_tick =
                    finish_tick +
                    ((RS485_POLL_RETRY_DELAY_MS *
                      osKernelGetTickFreq()) / 1000U);
                }
                else
                {
                    device_state->retry_count = 0U;
                    device_state->function_index = 0U;
                    device_state->next_poll_tick =
                    finish_tick +
                    ((device->poll_interval_ms *
                      osKernelGetTickFreq()) / 1000U);
                }
            }
        }
    }
}
