#ifndef DEV_ACREL_DTSD1352_H
#define DEV_ACREL_DTSD1352_H

#include <stddef.h>
#include <stdint.h>

struct DeviceData_Rx_t;
union RS485_DeviceRuntime_t;

/*初始地址映射*/
#define ACREL_DTSD1352_TOTAL_ENERGY_ADDRESS                                                 (0x000A)
#define ACREL_DTSD1352_ABC_VOLATGE_CURRENT_ADDRESS                                          (0x0061)
#define ACREL_DTSD1352_PT_CT_ADDRESS                                                        (0x008D)

/*寄存器数量映射*/
#define ACREL_DTSD1352_TOTAL_ENERGY_COUNT                                                   (2U)
#define ACREL_DTSD1352_ABC_VOLATGE_CURRENT_COUNT                                            (6U)
#define ACREL_DTSD1352_PT_CT_COUNT                                                          (2U)

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
}Dev_Acrel_DTSD1352_t;

/* DTSD1352运行期数据，按设备实例保存。 */
typedef struct
{
    uint8_t pt;
    uint8_t ct;
    uint8_t ratio_valid;
} Acrel_DTSD1352_Runtime_t;


void Acrel_DTSD1352_Pack_PTCT(
    const uint16_t *registers,
    uint16_t register_count,
    struct DeviceData_Rx_t *device_data,
    union RS485_DeviceRuntime_t *device_runtime);

void Acrel_DTSD1352_Pack_EnergyTotal(
    const uint16_t *registers,
    uint16_t register_count,
    struct DeviceData_Rx_t *device_data,
    union RS485_DeviceRuntime_t *device_runtime);

void Acrel_DTSD1352_Pack_Current_Voltage_ABC(
    const uint16_t *registers,
    uint16_t register_count,
    struct DeviceData_Rx_t *device_data,
    union RS485_DeviceRuntime_t *device_runtime);

#endif /* DEV_ACREL_DTSD1352_H */
