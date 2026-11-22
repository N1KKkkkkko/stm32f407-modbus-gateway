#include "dev_acrel_dtsd1352.h"

static const uint8_t Acrel_PT = 1;//电压变比默认值
static const uint8_t Acrel_CT = 1;//电流变比默认值

/***********************************************************************
 * 0. PT,CT
 * 寄存器：地址0x008D,数量2 → PT、CT
 ***********************************************************************/
void Acrel_DTSD1352_Pack_PTCT(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_PT_CT_COUNT)
    {
        return;
    }

    device_data->Acrel.pt = (uint8_t)registers[0];
    device_data->Acrel.pt = (uint8_t)registers[1];
}

/***********************************************************************
 * 1. 当前正向总有功电能解析
 * 寄存器：地址0x000A，数量2 → 当前正向总有功电能_H、当前正向总有功电能_L
 ***********************************************************************/
void Acrel_DTSD1352_Pack_EnergyTotal(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_TOTAL_ENERGY_COUNT)
    {
        return;
    }

    // 高字在前，拼接32位原始值
    uint32_t raw_energy = ((uint32_t)registers[0] << 16) | (uint32_t)registers[1];
    // 缩放系数 0.01 kWh
    device_data->Acrel.total_energy_kwh = (float)raw_energy * Acrel_PT * Acrel_CT * 0.01f;
}

/***********************************************************************
 * 2. 三相电压电流解析
 * 寄存器：地址0x0061，数量6 → Ua、Ub、Uc、Ia、Ib、Ic
 ***********************************************************************/
void Acrel_DTSD1352_Pack_Current_Voltage_ABC(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_ACREL_DTSD1352_ABC_VOLATGE_CURRENT_COUNT)
    {
        return;
    }

    // 电压：系数 0.1 V
    device_data->Acrel.Ua_v = (float)registers[0] * Acrel_PT * 0.1f;
    device_data->Acrel.Ub_v = (float)registers[1] * Acrel_PT * 0.1f;
    device_data->acrel.Uc_v = (float)registers[2] * Acrel_PT * 0.1f;

    // 电流：系数 0.001 A（官方标准）
    device_data->Acrel.Ia_a = (float)registers[3] * Acrel_CT * 0.001f;
    device_data->Acrel.Ib_a = (float)registers[4] * Acrel_CT * 0.001f;
    device_data->Acrel.Ic_a = (float)registers[5] * Acrel_CT * 0.001f;
}

