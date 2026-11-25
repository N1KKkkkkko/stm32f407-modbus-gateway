/***********************************************************************
文件名称：app_tasks.h
功    能：任务相关函数声明
编写时间：2026-11-17
编 写 人：N1ghtoo
注    意：
***********************************************************************/

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>

#include "cmsis_os.h"
#include "app_message.h"
#include "app_protocal.h"

// CubeMX generated task handle in freertos.c
extern osThreadId_t Task_PollinHandle;
extern osMessageQueueId_t Queue_DeviceData_RxHandle;

/* Queue_Modbus_Request: Task_Pollin -> Task_RS485 (one RS485_Request_t) */
extern osMessageQueueId_t Queue_Modbus_RequestHandle;

/* Queue_Modbus_Respond: Task_RS485 -> Task_Pollin (one RS485_Response_t) */
extern osMessageQueueId_t Queue_Modbus_RespondHandle;

// Task_Edge.c
extern volatile uint32_t TimeStamp;
uint32_t Get_Timestamp(void);
uint8_t Time3_Start(void);
uint8_t Temp_Start(void);
uint8_t Get_Temp(DeviceData_Rx_t *temp_data);
void Task_Edge_Run(void *argument);

// Task_RS485.c
void Task_RS485_Run(void *argument);

// Task_Pollin.c
void Task_Pollin_Run(void *argument);

#endif /* APP_TASKS_H */
