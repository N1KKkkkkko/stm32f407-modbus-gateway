#include "dev_acrel_dtsd1352.h"

/***********************************************************************
 * 1. 总电能解析
 * 寄存器：地址0，数量2 → Total_Energy_H、Total_Energy_L
 ***********************************************************************/
void Acrel_DTSD1352_Pack_EnergyTotal(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < 2U)
    {
        return;
    }

    // 高字在前，拼接32位原始值
    uint32_t raw_energy = ((uint32_t)registers[0] << 16) | (uint32_t)registers[1];
    // 缩放系数 0.01 kWh
    device_data->acrel.total_energy_kwh = (float)raw_energy * 0.01f;
}

/***********************************************************************
 * 2. A/B/C三相电能解析
 * 寄存器：地址2，数量6 → A相H/L、B相H/L、C相H/L
 ***********************************************************************/
void Acrel_DTSD1352_Pack_EnergyABC(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < 6U)
    {
        return;
    }

    uint32_t raw_a = ((uint32_t)registers[0] << 16) | (uint32_t)registers[1];
    uint32_t raw_b = ((uint32_t)registers[2] << 16) | (uint32_t)registers[3];
    uint32_t raw_c = ((uint32_t)registers[4] << 16) | (uint32_t)registers[5];

    device_data->acrel.a_energy_kwh = (float)raw_a * 0.01f;
    device_data->acrel.b_energy_kwh = (float)raw_b * 0.01f;
    device_data->acrel.c_energy_kwh = (float)raw_c * 0.01f;
}

/***********************************************************************
 * 3. 三相电压电流解析
 * 寄存器：地址50，数量6 → Ua、Ub、Uc、Ia、Ib、Ic
 ***********************************************************************/
void Acrel_DTSD1352_Pack_VI(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < 6U)
    {
        return;
    }

    // 电压：系数 0.1 V
    device_data->acrel.ua_v = (float)registers[0] * 0.1f;
    device_data->acrel.ub_v = (float)registers[1] * 0.1f;
    device_data->acrel.uc_v = (float)registers[2] * 0.1f;

    // 电流：系数 0.001 A（官方标准）
    device_data->acrel.ia_a = (float)registers[3] * 0.001f;
    device_data->acrel.ib_a = (float)registers[4] * 0.001f;
    device_data->acrel.ic_a = (float)registers[5] * 0.001f;
}

/***********************************************************************
 * 4. 功率解析
 * 寄存器：地址64，数量3 → 有功、无功、视在
 * 注意：功率为有符号int16，支持反向负功率
 ***********************************************************************/
void Acrel_DTSD1352_Pack_Power(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < 3U)
    {
        return;
    }

    // 强制转为有符号，否则负功率会解析为大正数
    device_data->acrel.p_total_w  = (float)(int16_t)registers[0];
    device_data->acrel.q_total_var = (float)(int16_t)registers[1];
    device_data->acrel.s_total_va  = (float)(int16_t)registers[2];
}

/***********************************************************************
 * 5. 功率因数 + 频率解析
 * 寄存器：地址70，数量3 → 70=PF，71=保留，72=频率
 ***********************************************************************/
void Acrel_DTSD1352_Pack_PF_Freq(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < 3U)
    {
        return;
    }

    // 功率因数：系数 0.001
    device_data->acrel.pf = (float)registers[0] * 0.001f;
    // 频率：系数 0.1 Hz，下标2对应地址72
    device_data->acrel.freq_hz = (float)registers[2] * 0.1f;
}
