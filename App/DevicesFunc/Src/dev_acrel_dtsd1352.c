#include "dev_acrel_dtsd1352.h"
#include "app_message.h"
#include "SlaveMachine_form.h"

/* 注意：目前一类电表使用统一PT,CT */
/* 电压变比默认值 */
/* 电流变比默认值 */
/* PT/CT保存在设备实例运行态中，未读取前使用默认值1。 */
static uint8_t Acrel_GetPT(
    const union RS485_DeviceRuntime_t *device_runtime)
{
    if ((device_runtime == NULL) ||
        (device_runtime->Acrel.ratio_valid == 0U))
    {
        return 1U;
    }

    return device_runtime->Acrel.pt;
}

static uint8_t Acrel_GetCT(
    const union RS485_DeviceRuntime_t *device_runtime)
{
    if ((device_runtime == NULL) ||
        (device_runtime->Acrel.ratio_valid == 0U))
    {
        return 1U;
    }

    return device_runtime->Acrel.ct;
}

/***********************************************************************
 * 0. PT,CT
 * 寄存器：地址0x008D,数量2 → PT、CT
 ***********************************************************************/
void Acrel_DTSD1352_Pack_PTCT(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_Data_t *device_data, union RS485_DeviceRuntime_t *device_runtime)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_PT_CT_COUNT)
    {
        return;
    }

    device_data->data_len = (uint8_t)sizeof(Dev_Acrel_DTSD1352_t);
    device_data->payload.Acrel.pt = (uint8_t)registers[0];
    device_data->payload.Acrel.ct = (uint8_t)registers[1];
    if (device_runtime != NULL)
    {
        device_runtime->Acrel.pt = (uint8_t)registers[0];
        device_runtime->Acrel.ct = (uint8_t)registers[1];
        device_runtime->Acrel.ratio_valid = 1U;
    }
}

/***********************************************************************
 * 1. 当前正向总有功电能解析
 * 寄存器：地址0x000A，数量2 → 当前正向总有功电能_H、当前正向总有功电能_L
 ***********************************************************************/
void Acrel_DTSD1352_Pack_EnergyTotal(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_Data_t *device_data, union RS485_DeviceRuntime_t *device_runtime)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_TOTAL_ENERGY_COUNT)
    {
        return;
    }

    device_data->data_len = (uint8_t)sizeof(Dev_Acrel_DTSD1352_t);
    // 高字在前，拼接32位原始值
    uint32_t raw_energy = ((uint32_t)registers[0] << 16) | (uint32_t)registers[1];
    // 缩放系数 0.01 kWh
    device_data->payload.Acrel.total_energy_kwh = (float)raw_energy *
        Acrel_GetPT(device_runtime) *
        Acrel_GetCT(device_runtime) * 0.01f;
}

/***********************************************************************
 * 2. 三相电压电流解析
 * 寄存器：地址0x0061，数量6 → Ua、Ub、Uc、Ia、Ib、Ic
 ***********************************************************************/
void Acrel_DTSD1352_Pack_Current_Voltage_ABC(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_Data_t *device_data, union RS485_DeviceRuntime_t *device_runtime)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_ABC_VOLATGE_CURRENT_COUNT)
    {
        return;
    }

    device_data->data_len = (uint8_t)sizeof(Dev_Acrel_DTSD1352_t);
    // 电压：系数 0.1 V
    device_data->payload.Acrel.Ua_v = (float)registers[0] *
        Acrel_GetPT(device_runtime) * 0.1f;
    device_data->payload.Acrel.Ub_v = (float)registers[1] *
        Acrel_GetPT(device_runtime) * 0.1f;
    device_data->payload.Acrel.Uc_v = (float)registers[2] *
        Acrel_GetPT(device_runtime) * 0.1f;

    // 电流：系数 0.001 A（官方标准）
    device_data->payload.Acrel.Ia_a = (float)registers[3] *
        Acrel_GetCT(device_runtime) * 0.001f;
    device_data->payload.Acrel.Ib_a = (float)registers[4] *
        Acrel_GetCT(device_runtime) * 0.001f;
    device_data->payload.Acrel.Ic_a = (float)registers[5] *
        Acrel_GetCT(device_runtime) * 0.001f;
}

