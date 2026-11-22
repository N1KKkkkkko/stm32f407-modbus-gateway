/***********************************************************************
文件名称：slave_machine_form.h
功    能：设备类型配置表、功能表、设备实例表
编写时间：2026-11-15
编 写 人：N1ghtoo
注    意：
***********************************************************************/

#ifndef SLAVE_MACHINE_FORM_H
#define SLAVE_MACHINE_FORM_H

#include <stdint.h>

#include "app_message.h"
#include "app_protocal.h"
#include "dev_acrel_dtsd1352.h"

#define RS485_ARRAY_COUNT(array) \
    (sizeof(array) / sizeof((array)[0]))

/* ================= 1.功能表 2.设备解析函数 3.设备类型具体配置 4.设备实例 模板  ================= */

/* 设备数据解析函数指针模板 */
typedef void (*RS485_Read_SlaveUnpackData_t)(
    const uint16_t *registers,
    uint16_t register_count,
    DeviceData_Rx_t *device_data
);   

/* 功能模板 */
//包含解析函数指针
typedef struct
{
    ModbusFunction_t function;                  //功能码
    uint16_t start_addr;                        //寄存器起始地址
    uint16_t quantity;                          //寄存器数量
    RS485_Read_SlaveUnpackData_t pack_data;     //每个功能对应一个解析函数
} RS485_FuncTemplate_t;

/* 设备类型具体配置模板 */
// 设备类型配置表，包含 功能表 ，供设备实例使用
typedef struct
{
    DeviceType_t device_type;                   //设备类型值                   
    const char *type_name;                      //设备类型名称
    uint16_t max_regs_per_func;                 //每个功能码最大寄存器数量，避免一次性请求过多寄存器
    uint8_t func_count;                         //功能码数量
    const RS485_FuncTemplate_t *func_table;     //一类设备对应功能表
} DeviceTypeProfile_t;

/* 具体从站设备模板 */
typedef struct
{
    uint8_t enabled;                          //轮询使能开关
    uint8_t slave_address;
    uint32_t poll_interval_ms;
    const DeviceTypeProfile_t *profile;
} RS485_SlaveMachine_t;

/* ================= Acrel DTSD1352 电表 功能表  ================= */

static const RS485_FuncTemplate_t Acrel_DTSD1352_Funcs[] =
{
    {
        .function = MODBUS_FUNCTION_READ_HOLDING,
        .start_addr = 0U,
        .quantity = 2U,
        .pack_data = Acrel_DTSD1352_Pack_EnergyTotal
    },
    {
        .function = MODBUS_FUNCTION_READ_HOLDING,
        .start_addr = 2U,
        .quantity = 6U,
        .pack_data = Acrel_DTSD1352_Pack_EnergyABC
    },
    {
        .function = MODBUS_FUNCTION_READ_HOLDING,
        .start_addr = 50U,
        .quantity = 6U,
        .pack_data = Acrel_DTSD1352_Pack_VI
    },
    {
        .function = MODBUS_FUNCTION_READ_HOLDING,
        .start_addr = 64U,
        .quantity = 3U,
        .pack_data = Acrel_DTSD1352_Pack_Power
    },
    {
        .function = MODBUS_FUNCTION_READ_HOLDING,
        .start_addr = 70U,
        .quantity = 3U,
        .pack_data = Acrel_DTSD1352_Pack_PF_Freq
    }
};

static const DeviceTypeProfile_t Acrel_DTSD1352_Profile =
{
    .device_type = DEV_TYPE_ACREL_METER,
    .type_name = "Acrel DTSD1352",
    .max_regs_per_func = 6U,
    .func_count = (uint8_t)RS485_ARRAY_COUNT(Acrel_DTSD1352_Funcs),
    .func_table = Acrel_DTSD1352_Funcs
};


/* =============================================== 从站设备表（设备轮询表） ========================================================= */

static const RS485_SlaveMachine_t RS485_SlaveMachines[] =
{
    {
        .enabled = 1U,
        .slave_address = 1U,
        .poll_interval_ms = 500U,
        .profile = &Acrel_DTSD1352_Profile
    }
};

#define RS485_SLAVE_MACHINE_COUNT RS485_ARRAY_COUNT(RS485_SlaveMachines)

#endif /* SLAVE_MACHINE_FORM_H */
