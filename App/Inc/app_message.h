/***********************************************************************
文件名称：app_message.h
功    能：任务间通信消息队列Item定义
编写时间：2026-11-15
编 写 人：N1ghtoo
注    意：
***********************************************************************/

#ifndef APP_MESSAGE_H
#define APP_MESSAGE_H

#include <stdint.h>
#define MAX_PAYLOAD_LEN 16
#define POOL_BLOCK_COUNT 30  // 内存池 30 块，能扛短时离线积压
#define DEVICE_DATA_RX_QUEUE_ITEM_SIZE 32U
#define LOG_DATA_QUEUE_ITEM_SIZE 12U

//设备类型值
typedef enum {
    DEV_TYPE_UNKNOWN = 0x00,        // 0: 网关本身 (默认值)
    DEV_TYPE_ACREL_METER = 0x01,    // 1: 安科瑞电表 (输出Float类型的电压电流)
    DEV_TYPE_TEMP_HUMID = 0x02,     // 2: 温湿度传感器 (输出整型类型的温湿度)
    DEV_TYPE_DELTA_INVERTER = 0x03, // 3: 台达变频器
    DEV_TYPE_CANOPEN_NODE = 0x04,   // 4: CANopen 伺服电机
    DEV_TYPE_GATEWAY_SELF = 0x05,   // 5: 网关自身健康状态 (ADC电压、板载温度)
    // ... 以后现场加新设备，直接在这里加类型即可
} DeviceTypeValue_t;

// 消息中的设备类型固定为32位，避免不同编译器改变结构体布局
typedef uint32_t DeviceType_t;

// Queue_DeviceData_Rx Item
// 设备数据 （经过USART,Modbus RTU协议拆解后的设备数据 和 网关温度）
typedef struct {
    uint32_t timestamp;//时间戳
    DeviceType_t dev_type;// 设备类型
    uint8_t  dev_id; // 设备ID
    uint8_t  data_len; // 数据长度
    union {
        uint8_t  Byte_val[MAX_PAYLOAD_LEN];
        float    float_val[MAX_PAYLOAD_LEN / 4];
        int32_t  int_val[MAX_PAYLOAD_LEN / 4];
    } payload; // 数据载荷(实际数据)，支持多种类型，按设备类型解析
    uint8_t reserved[4]; // 显式填充到32字节，与静态队列Item大小一致
} DeviceData_Rx_t;


// Queue_FlashData Item
// 只传指针即可，因为长度在结构体里有了，采用解引队列指针，获取内存池数据的方式
typedef struct {
     DeviceData_Rx_t *data_ptr;
} FlashData_t;


// Queue_LogData Item 
// 日志结构体
typedef struct {
    uint32_t      timestamp;  // 4 字节：发生时间 (Unix时间戳)
    DeviceType_t dev_type;// 4字节：设备类型 
    uint8_t  dev_id; // 1字节：设备ID
    uint8_t       status;     // 1 字节：事件状态 (Active 1 /Inactive 0 /Wrong -1)
    uint16_t      reserved;   // 2 字节：预留对齐或存错误码 (如 Modbus 异常码)
} LogData_t;          // 对齐后 12 字节

typedef char DeviceData_RxQueueItemSizeCheck[
    (sizeof(DeviceData_Rx_t) == DEVICE_DATA_RX_QUEUE_ITEM_SIZE) ? 1 : -1
];

typedef char LogDataQueueItemSizeCheck[
    (sizeof(LogData_t) == LOG_DATA_QUEUE_ITEM_SIZE) ? 1 : -1
];

#endif // APP_MESSAGE_H
