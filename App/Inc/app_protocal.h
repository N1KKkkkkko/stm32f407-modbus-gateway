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
#define RS485_REQUEST_QUEUE_ITEM_SIZE  16U
#define RS485_RESPONSE_QUEUE_ITEM_SIZE 76U

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

/*Task_Pollin发送请求 RS485 的Modbus请求结构体*/
typedef struct
{
    uint8_t slave_address;
    ModbusFunction_t function;
    uint16_t register_address;
    uint16_t register_count;
    uint16_t write_value;
} RS485_Request_t;

/*Task_Pollin接收响应 RS485 的Modbus数据结构体*/
typedef struct
{
    RS485_Status_t status;
    int32_t error_code;
    uint16_t register_count;
    uint16_t registers[RS485_MAX_REGISTERS];
} RS485_Response_t;

/*检查队列项大小是否符合标准 */
typedef char RS485RequestQueueItemSizeCheck[
    (sizeof(RS485_Request_t) == RS485_REQUEST_QUEUE_ITEM_SIZE) ? 1 : -1
];

typedef char RS485ResponseQueueItemSizeCheck[
    (sizeof(RS485_Response_t) == RS485_RESPONSE_QUEUE_ITEM_SIZE) ? 1 : -1
];

#endif /* APP_PROTOCAL_H */
