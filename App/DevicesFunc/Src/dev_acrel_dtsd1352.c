#include "dev_acrel_dtsd1352.h"
#include "app_message.h"

/* 注意：目前一类电表使用统一PT,CT */
/* 运行期变比按从站地址分别保存，读取前默认值为1。 */
#define ACREL_DTSD1352_SLAVE_ADDRESS_COUNT 248U
static uint8_t Acrel_PT_By_Slave[ACREL_DTSD1352_SLAVE_ADDRESS_COUNT];//电压变比默认值
static uint8_t Acrel_CT_By_Slave[ACREL_DTSD1352_SLAVE_ADDRESS_COUNT];//电流变比默认值
static uint8_t Acrel_Ratio_Valid[ACREL_DTSD1352_SLAVE_ADDRESS_COUNT];

/* 从站完成PT/CT读取前使用变比1，读取后按从站地址保存。 */
static uint8_t Acrel_GetPT(uint8_t slave_address)
{
    if ((slave_address == 0U) ||
        (slave_address >= ACREL_DTSD1352_SLAVE_ADDRESS_COUNT) ||
        (Acrel_Ratio_Valid[slave_address] == 0U))
    {
        return 1U;
    }

    return Acrel_PT_By_Slave[slave_address];
}

static uint8_t Acrel_GetCT(uint8_t slave_address)
{
    if ((slave_address == 0U) ||
        (slave_address >= ACREL_DTSD1352_SLAVE_ADDRESS_COUNT) ||
        (Acrel_Ratio_Valid[slave_address] == 0U))
    {
        return 1U;
    }

    return Acrel_CT_By_Slave[slave_address];
}

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

    device_data->data_len = (uint8_t)sizeof(Dev_Acrel_DTSD1352_t);
    device_data->payload.Acrel.pt = (uint8_t)registers[0];
    device_data->payload.Acrel.ct = (uint8_t)registers[1];
    if ((device_data->dev_id != 0U) &&
        (device_data->dev_id < ACREL_DTSD1352_SLAVE_ADDRESS_COUNT))
    {
        Acrel_PT_By_Slave[device_data->dev_id] = (uint8_t)registers[0];
        Acrel_CT_By_Slave[device_data->dev_id] = (uint8_t)registers[1];
        Acrel_Ratio_Valid[device_data->dev_id] = 1U;
    }
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

    device_data->data_len = (uint8_t)sizeof(Dev_Acrel_DTSD1352_t);
    // 高字在前，拼接32位原始值
    uint32_t raw_energy = ((uint32_t)registers[0] << 16) | (uint32_t)registers[1];
    // 缩放系数 0.01 kWh
    device_data->payload.Acrel.total_energy_kwh = (float)raw_energy *
        Acrel_GetPT(device_data->dev_id) *
        Acrel_GetCT(device_data->dev_id) * 0.01f;
}

/***********************************************************************
 * 2. 三相电压电流解析
 * 寄存器：地址0x0061，数量6 → Ua、Ub、Uc、Ia、Ib、Ic
 ***********************************************************************/
void Acrel_DTSD1352_Pack_Current_Voltage_ABC(const uint16_t *registers, uint16_t register_count, DeviceData_Rx_t *device_data)
{
    if (registers == NULL || device_data == NULL || register_count < ACREL_DTSD1352_ABC_VOLATGE_CURRENT_COUNT)
    {
        return;
    }

    device_data->data_len = (uint8_t)sizeof(Dev_Acrel_DTSD1352_t);
    // 电压：系数 0.1 V
    device_data->payload.Acrel.Ua_v = (float)registers[0] *
        Acrel_GetPT(device_data->dev_id) * 0.1f;
    device_data->payload.Acrel.Ub_v = (float)registers[1] *
        Acrel_GetPT(device_data->dev_id) * 0.1f;
    device_data->payload.Acrel.Uc_v = (float)registers[2] *
        Acrel_GetPT(device_data->dev_id) * 0.1f;

    // 电流：系数 0.001 A（官方标准）
    device_data->payload.Acrel.Ia_a = (float)registers[3] *
        Acrel_GetCT(device_data->dev_id) * 0.001f;
    device_data->payload.Acrel.Ib_a = (float)registers[4] *
        Acrel_GetCT(device_data->dev_id) * 0.001f;
    device_data->payload.Acrel.Ic_a = (float)registers[5] *
        Acrel_GetCT(device_data->dev_id) * 0.001f;
}

