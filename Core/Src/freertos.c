/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_tasks.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticQueue_t osStaticMessageQDef_t;
typedef StaticSemaphore_t osStaticMutexDef_t;
typedef StaticSemaphore_t osStaticSemaphoreDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Task_Net */
osThreadId_t Task_NetHandle;
uint32_t Task_NetBuffer[ 512 ];
osStaticThreadDef_t Task_NetControlBlock;
const osThreadAttr_t Task_Net_attributes = {
  .name = "Task_Net",
  .cb_mem = &Task_NetControlBlock,
  .cb_size = sizeof(Task_NetControlBlock),
  .stack_mem = &Task_NetBuffer[0],
  .stack_size = sizeof(Task_NetBuffer),
  .priority = (osPriority_t) osPriorityHigh7,
};
/* Definitions for Task_RS232 */
osThreadId_t Task_RS232Handle;
uint32_t Task_RS232Buffer[ 128 ];
osStaticThreadDef_t Task_RS232ControlBlock;
const osThreadAttr_t Task_RS232_attributes = {
  .name = "Task_RS232",
  .cb_mem = &Task_RS232ControlBlock,
  .cb_size = sizeof(Task_RS232ControlBlock),
  .stack_mem = &Task_RS232Buffer[0],
  .stack_size = sizeof(Task_RS232Buffer),
  .priority = (osPriority_t) osPriorityAboveNormal7,
};
/* Definitions for Task_RS485 */
osThreadId_t Task_RS485Handle;
uint32_t Task_RS485Buffer[ 128 ];
osStaticThreadDef_t Task_RS485ControlBlock;
const osThreadAttr_t Task_RS485_attributes = {
  .name = "Task_RS485",
  .cb_mem = &Task_RS485ControlBlock,
  .cb_size = sizeof(Task_RS485ControlBlock),
  .stack_mem = &Task_RS485Buffer[0],
  .stack_size = sizeof(Task_RS485Buffer),
  .priority = (osPriority_t) osPriorityRealtime6,
};
/* Definitions for Task_FStorage */
osThreadId_t Task_FStorageHandle;
uint32_t Task_FStorageBuffer[ 128 ];
osStaticThreadDef_t Task_FStorageControlBlock;
const osThreadAttr_t Task_FStorage_attributes = {
  .name = "Task_FStorage",
  .cb_mem = &Task_FStorageControlBlock,
  .cb_size = sizeof(Task_FStorageControlBlock),
  .stack_mem = &Task_FStorageBuffer[0],
  .stack_size = sizeof(Task_FStorageBuffer),
  .priority = (osPriority_t) osPriorityNormal6,
};
/* Definitions for Task_Edge */
osThreadId_t Task_EdgeHandle;
uint32_t Task_EdgeBuffer[ 128 ];
osStaticThreadDef_t Task_EdgeControlBlock;
const osThreadAttr_t Task_Edge_attributes = {
  .name = "Task_Edge",
  .cb_mem = &Task_EdgeControlBlock,
  .cb_size = sizeof(Task_EdgeControlBlock),
  .stack_mem = &Task_EdgeBuffer[0],
  .stack_size = sizeof(Task_EdgeBuffer),
  .priority = (osPriority_t) osPriorityNormal3,
};
/* Definitions for Task_SStorage */
osThreadId_t Task_SStorageHandle;
uint32_t Task_SStorageBuffer[ 128 ];
osStaticThreadDef_t Task_SStorageControlBlock;
const osThreadAttr_t Task_SStorage_attributes = {
  .name = "Task_SStorage",
  .cb_mem = &Task_SStorageControlBlock,
  .cb_size = sizeof(Task_SStorageControlBlock),
  .stack_mem = &Task_SStorageBuffer[0],
  .stack_size = sizeof(Task_SStorageBuffer),
  .priority = (osPriority_t) osPriorityNormal2,
};
/* Definitions for Task_Shutdown */
osThreadId_t Task_ShutdownHandle;
uint32_t Task_ShutdownBuffer[ 128 ];
osStaticThreadDef_t Task_ShutdownControlBlock;
const osThreadAttr_t Task_Shutdown_attributes = {
  .name = "Task_Shutdown",
  .cb_mem = &Task_ShutdownControlBlock,
  .cb_size = sizeof(Task_ShutdownControlBlock),
  .stack_mem = &Task_ShutdownBuffer[0],
  .stack_size = sizeof(Task_ShutdownBuffer),
  .priority = (osPriority_t) osPriorityRealtime7,
};
/* Definitions for Task_Pollin */
osThreadId_t Task_PollinHandle;
uint32_t Task_PollinBuffer[ 128 ];
osStaticThreadDef_t Task_PollinControlBlock;
const osThreadAttr_t Task_Pollin_attributes = {
  .name = "Task_Pollin",
  .cb_mem = &Task_PollinControlBlock,
  .cb_size = sizeof(Task_PollinControlBlock),
  .stack_mem = &Task_PollinBuffer[0],
  .stack_size = sizeof(Task_PollinBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Queue_DeviceData_Rx */
osMessageQueueId_t Queue_DeviceData_RxHandle;
uint8_t Queue_DeviceData_RxBuffer[ 16 * 48 ];
osStaticMessageQDef_t Queue_DeviceData_RxControlBlock;
const osMessageQueueAttr_t Queue_DeviceData_Rx_attributes = {
  .name = "Queue_DeviceData_Rx",
  .cb_mem = &Queue_DeviceData_RxControlBlock,
  .cb_size = sizeof(Queue_DeviceData_RxControlBlock),
  .mq_mem = &Queue_DeviceData_RxBuffer,
  .mq_size = sizeof(Queue_DeviceData_RxBuffer)
};
/* Definitions for Queue_FlashData */
osMessageQueueId_t Queue_FlashDataHandle;
uint8_t Queue_FlashDataBuffer[ 30 * 4 ];
osStaticMessageQDef_t Queue_FlashDataControlBlock;
const osMessageQueueAttr_t Queue_FlashData_attributes = {
  .name = "Queue_FlashData",
  .cb_mem = &Queue_FlashDataControlBlock,
  .cb_size = sizeof(Queue_FlashDataControlBlock),
  .mq_mem = &Queue_FlashDataBuffer,
  .mq_size = sizeof(Queue_FlashDataBuffer)
};
/* Definitions for Queue_LogData */
osMessageQueueId_t Queue_LogDataHandle;
uint8_t Queue_LogDataBuffer[ 20 * 12 ];
osStaticMessageQDef_t Queue_LogDataControlBlock;
const osMessageQueueAttr_t Queue_LogData_attributes = {
  .name = "Queue_LogData",
  .cb_mem = &Queue_LogDataControlBlock,
  .cb_size = sizeof(Queue_LogDataControlBlock),
  .mq_mem = &Queue_LogDataBuffer,
  .mq_size = sizeof(Queue_LogDataBuffer)
};
/* Definitions for Queue_Pollin_Tx */
osMessageQueueId_t Queue_Pollin_TxHandle;
uint8_t Queue_Pollin_TxBuffer[ 1 * 16 ];
osStaticMessageQDef_t Queue_Pollin_TxControlBlock;
const osMessageQueueAttr_t Queue_Pollin_Tx_attributes = {
  .name = "Queue_Pollin_Tx",
  .cb_mem = &Queue_Pollin_TxControlBlock,
  .cb_size = sizeof(Queue_Pollin_TxControlBlock),
  .mq_mem = &Queue_Pollin_TxBuffer,
  .mq_size = sizeof(Queue_Pollin_TxBuffer)
};
/* Definitions for Queue_RS485_Tx */
osMessageQueueId_t Queue_RS485_TxHandle;
uint8_t Queue_RS485_TxBuffer[ 1 * 76 ];
osStaticMessageQDef_t Queue_RS485_TxControlBlock;
const osMessageQueueAttr_t Queue_RS485_Tx_attributes = {
  .name = "Queue_RS485_Tx",
  .cb_mem = &Queue_RS485_TxControlBlock,
  .cb_size = sizeof(Queue_RS485_TxControlBlock),
  .mq_mem = &Queue_RS485_TxBuffer,
  .mq_size = sizeof(Queue_RS485_TxBuffer)
};
/* Definitions for Mutex_P_R */
osMutexId_t Mutex_P_RHandle;
osStaticMutexDef_t Mutex_P_RControlBlock;
const osMutexAttr_t Mutex_P_R_attributes = {
  .name = "Mutex_P_R",
  .cb_mem = &Mutex_P_RControlBlock,
  .cb_size = sizeof(Mutex_P_RControlBlock),
};
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void Task_Net_Entry(void *argument);
void Task_RS232_Entry(void *argument);
void Task_RS485_Entry(void *argument);
void Task_FStorage_Entry(void *argument);
void Task_Edge_Entry(void *argument);
void Task_SStorage_Entry(void *argument);
void Task_Shutdown_Entry(void *argument);
void Task_Pollin_Entry(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of Mutex_P_R */
  Mutex_P_RHandle = osMutexNew(&Mutex_P_R_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphore(s) */
  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of Queue_DeviceData_Rx */
  Queue_DeviceData_RxHandle = osMessageQueueNew (16, 48, &Queue_DeviceData_Rx_attributes);

  /* creation of Queue_FlashData */
  Queue_FlashDataHandle = osMessageQueueNew (30, 4, &Queue_FlashData_attributes);

  /* creation of Queue_LogData */
  Queue_LogDataHandle = osMessageQueueNew (20, 12, &Queue_LogData_attributes);

  /* creation of Queue_Pollin_Tx */
  Queue_Pollin_TxHandle = osMessageQueueNew (1, 16, &Queue_Pollin_Tx_attributes);

  /* creation of Queue_RS485_Tx */
  Queue_RS485_TxHandle = osMessageQueueNew (1, 76, &Queue_RS485_Tx_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of Task_Net */
  Task_NetHandle = osThreadNew(Task_Net_Entry, NULL, &Task_Net_attributes);

  /* creation of Task_RS232 */
  Task_RS232Handle = osThreadNew(Task_RS232_Entry, NULL, &Task_RS232_attributes);

  /* creation of Task_RS485 */
  Task_RS485Handle = osThreadNew(Task_RS485_Entry, NULL, &Task_RS485_attributes);

  /* creation of Task_FStorage */
  Task_FStorageHandle = osThreadNew(Task_FStorage_Entry, NULL, &Task_FStorage_attributes);

  /* creation of Task_Edge */
  Task_EdgeHandle = osThreadNew(Task_Edge_Entry, NULL, &Task_Edge_attributes);

  /* creation of Task_SStorage */
  Task_SStorageHandle = osThreadNew(Task_SStorage_Entry, NULL, &Task_SStorage_attributes);

  /* creation of Task_Shutdown */
  Task_ShutdownHandle = osThreadNew(Task_Shutdown_Entry, NULL, &Task_Shutdown_attributes);

  /* creation of Task_Pollin */
  Task_PollinHandle = osThreadNew(Task_Pollin_Entry, NULL, &Task_Pollin_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_Task_Net_Entry */
/**
* @brief Function implementing the Task_Net thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_Net_Entry */
void Task_Net_Entry(void *argument)
{
  /* USER CODE BEGIN Task_Net_Entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Task_Net_Entry */
}

/* USER CODE BEGIN Header_Task_RS232_Entry */
/**
* @brief Function implementing the Task_RS232 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_RS232_Entry */
void Task_RS232_Entry(void *argument)
{
  /* USER CODE BEGIN Task_RS232_Entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Task_RS232_Entry */
}

/* USER CODE BEGIN Header_Task_RS485_Entry */
/**
* @brief Function implementing the Task_RS485 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_RS485_Entry */
void Task_RS485_Entry(void *argument)
{
  /* USER CODE BEGIN Task_RS485_Entry */
  Task_RS485_Run(argument);
  /* USER CODE END Task_RS485_Entry */
}

/* USER CODE BEGIN Header_Task_FStorage_Entry */
/**
* @brief Function implementing the Task_FStorage thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_FStorage_Entry */
void Task_FStorage_Entry(void *argument)
{
  /* USER CODE BEGIN Task_FStorage_Entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Task_FStorage_Entry */
}

/* USER CODE BEGIN Header_Task_Edge_Entry */
/**
* @brief Function implementing the Task_Edge thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_Edge_Entry */
void Task_Edge_Entry(void *argument)
{
  /* USER CODE BEGIN Task_Edge_Entry */
  Task_Edge_Run(argument);
  /* USER CODE END Task_Edge_Entry */
}

/* USER CODE BEGIN Header_Task_SStorage_Entry */
/**
* @brief Function implementing the Task_SStorage thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_SStorage_Entry */
void Task_SStorage_Entry(void *argument)
{
  /* USER CODE BEGIN Task_SStorage_Entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Task_SStorage_Entry */
}

/* USER CODE BEGIN Header_Task_Shutdown_Entry */
/**
* @brief Function implementing the Task_Shutdown thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_Shutdown_Entry */
void Task_Shutdown_Entry(void *argument)
{
  /* USER CODE BEGIN Task_Shutdown_Entry */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END Task_Shutdown_Entry */
}

/* USER CODE BEGIN Header_Task_Pollin_Entry */
/**
* @brief Function implementing the Task_Pollin thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Task_Pollin_Entry */
void Task_Pollin_Entry(void *argument)
{
  /* USER CODE BEGIN Task_Pollin_Entry */
  Task_Pollin_Run(argument);
  /* USER CODE END Task_Pollin_Entry */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

