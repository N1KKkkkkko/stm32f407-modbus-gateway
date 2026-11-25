/***********************************************************************
文件名称：app_protocal.h
功    能：Modbus RTU协议解析相关定义
编写时间：2026-11-15
编 写 人：N1ghtoo
注    意：
***********************************************************************/

#ifndef APP_PROTOCAL_H
#define APP_PROTOCAL_H

#include <stdint.h>

#define RS485_MAX_REGISTERS 32U

/* Modbus function codes shared by device configuration and RS485 requests. */
typedef enum
{
    MODBUS_FUNCTION_READ_COILS        = 0x01,
    MODBUS_FUNCTION_READ_DISCRETE     = 0x02,
    MODBUS_FUNCTION_READ_HOLDING      = 0x03,
    MODBUS_FUNCTION_READ_INPUT        = 0x04,
    MODBUS_FUNCTION_WRITE_SINGLE_COIL = 0x05,
    MODBUS_FUNCTION_WRITE_SINGLE_REG  = 0x06,
    MODBUS_FUNCTION_WRITE_MULTI_COILS = 0x0F,
    MODBUS_FUNCTION_WRITE_MULTI_REG   = 0x10
} ModbusFunctionValue_t;

/* Fixed 32-bit storage keeps queue layout stable across compilers. */
typedef uint32_t ModbusFunction_t;

typedef enum
{
    RS485_STATUS_OK = 0,
    RS485_STATUS_ERROR = 1
} RS485_Status_t;

#endif /* APP_PROTOCAL_H */
