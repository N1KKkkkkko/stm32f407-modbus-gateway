#ifndef DEV_ACREL_DTSD1352_H
#define DEV_ACREL_DTSD1352_H

#include <stddef.h>
#include <stdint.h>

#include "app_message.h"

/*初始地址映射*/
#define TOTAL_ENERGY_ADDRESS                                (0x000A)
#define ABC_VOLATGE_CURRENT_ADDRESS                         (0x0061)
#define PT_CT_ADDRESS                                       (0x008D)

/* DTSD1352 数据荷载结构体 */
typedef struct Dev_Acrel_DTSD1352_t
{
    float total_energy_kwh;    // 正向总有功电能，单位 kWh
    float Ua_v;                // A相电压 V
    float Ub_v;                // B相电压 V
    float Uc_v;                // C相电压 V
    float Ia_a;                // A相电流 A
    float Ib_a;                // B相电流 A
    float Ic_a;                // C相电流 A
    uint8_t pt;                // 电压变比
    uint8_t ct;                // 电流变比
};


void Acrel_DTSD1352_Pack_EnergyTotal(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

void Acrel_DTSD1352_Pack_Current_Voltage_ABC(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data);

#endif /* DEV_ACREL_DTSD1352_H */
