/***********************************************************************
文件名称： Task_Edge.c
功    能： 边缘计算任务
编写时间：2026-11-17
编 写 人：N1ghtoo
注    意：包含 时间戳获取函数 和 ADC换算函数（时间戳获取函数在多个任务都用到，不是通过一个全局变量引用）
        
        Time3_Start                         启动TIM3
        Timestamp_Init                       将RTC转换成32位Unix时间戳
        HAL_TIM_PeriodElapsedCallback       TIM3 update中断回调函数，时间戳自增
        Temp_Start                          启动ADC-DMA采样，将ADC搬运到s_adc_dma_buffer[2] 2 意味两通道
        HAL_ADC_ConvCpltCallback            ADC 转换完成中断回调函数,adc采样完成后s_adc_sample_ready置1
        HAL_ADC_ErrorCallback               ADC 转化错误中断回调函数，s_adc_sample_ready置0，s_adc_error_count自增
        Get_Temp                            获取s_adc_dma_buffer[],并计算，填补temp_data 这个队列Item格式数据
        Task_Edge_Run                       任务运行函数，包含入队操作
***********************************************************************/
#include "cmsis_os.h"

#include "app_tasks.h"
#include "app_message.h"
#include "adc.h"
#include "main.h"
#include "rtc.h"
#include "stm32f4xx_ll_adc.h"
#include "tim.h"

#define RTC_EPOCH_2000                  946684800UL
#define APP_TIMEZONE_UTC_OFFSET_SECONDS (8UL * 60UL * 60UL)
#define ADC_FRAME_CHANNEL_COUNT         2U
#define ADC_TEMP_SENSOR_INDEX           0U
#define ADC_VREFINT_INDEX               1U

static volatile uint16_t s_adc_dma_buffer[ADC_FRAME_CHANNEL_COUNT];
static volatile uint8_t s_adc_sample_ready = 0U;
static volatile uint32_t s_adc_error_count = 0U;

/* 时间戳获取函数 */

uint8_t Time3_Start(void)//启动TIM3定时器，独立维护TimeStamp，返回1表示成功，0表示失败
{
    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
    {
        return 0U;
    }

    return 1U;
}

static const uint8_t MonthDays[13] =
{
    0U, 31U, 28U, 31U, 30U, 31U, 30U,
    31U, 31U, 30U, 31U, 30U, 31U
};

static uint8_t IsLeapYear(uint16_t year)
{
    return ((year % 4U == 0U) &&
            ((year % 100U != 0U) || (year % 400U == 0U)));
}

uint32_t Timestamp_Init(void)//返回32位时间戳，单位为秒，UTC（始:1970-01-01 00:00:00）
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    uint32_t days_since_2000 = 0U;
    uint32_t local_seconds;
    uint16_t year;
    uint8_t month,day,hour,min,sec;
    uint8_t days_in_month;

    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return 0U;
    }

    /* GetTime 后读取日期，避免跨午夜时日期与时间不一致。 */
    if (HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return 0U;
    }

    year  = 2000 + sDate.Year;
    month = sDate.Month;
    day   = sDate.Date;
    hour  = sTime.Hours;
    min   = sTime.Minutes;
    sec   = sTime.Seconds;

    if ((month < 1U) || (month > 12U))
    {
        return 0U;
    }

    days_in_month = MonthDays[month];
    if ((month == 2U) && IsLeapYear(year))
    {
        days_in_month += 1U;
    }

    if ((day < 1U) || (day > days_in_month))
    {
        return 0U;
    }

    /* RTC 年份范围为 2000 至 2099。 */
    for(uint16_t y = 2000; y < year; y++)
    {
        days_since_2000 += IsLeapYear(y) ? 366UL : 365UL;
    }

    for(uint8_t m = 1; m < month; m++)
    {
        days_since_2000 += MonthDays[m];
        if ((m == 2U) && IsLeapYear(year))
        {
            days_since_2000 += 1U;
        }
    }

    days_since_2000 += (uint32_t)(day - 1U);
    local_seconds = (days_since_2000 * 86400UL) +
                    ((uint32_t)hour * 3600UL) +
                    ((uint32_t)min * 60UL) +
                    (uint32_t)sec;

    if (local_seconds < APP_TIMEZONE_UTC_OFFSET_SECONDS)
    {
        return 0U;
    }

    /* RTC 保存北京时间，转换为 Unix UTC 时间戳。 */
    return RTC_EPOCH_2000 +
           local_seconds -
           APP_TIMEZONE_UTC_OFFSET_SECONDS;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) //weak函数可以在这里重写，TIM3定时器每秒中断一次，时间戳自增
{
    if (htim->Instance == TIM3)
    {
        TimeStamp++;
    }

}


/* ADC换算函数 */

uint8_t Temp_Start(void) //启动ADC-DMA采样，返回1表示成功，0表示失败
{
    s_adc_sample_ready = 0U; //adc采样完成标志位初始化
    s_adc_error_count = 0U; //adc采样错误计数初始化

    if (HAL_ADC_Start_DMA(
            &hadc1,
            (uint32_t *)(uintptr_t)s_adc_dma_buffer,
            ADC_FRAME_CHANNEL_COUNT
        ) != HAL_OK)
    {
        return 0U;
    }

    return 1U;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) //weak函数可以在这里重写，ADC采样完成后会调用这个函数
{
    if (hadc->Instance == ADC1)
    {
        __DMB();
        s_adc_sample_ready = 1U;
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)  //weak函数可以在这里重写，ADC采样出错后会调用这个函数
{
    if (hadc->Instance == ADC1)
    {
        s_adc_sample_ready = 0U;
        s_adc_error_count++;
    }
}

uint8_t Get_Temp(DeviceData_Rx_Data_t *temp_data) //传指针可以给实际数据赋值
{
    uint16_t temp_raw;
    uint16_t vrefint_raw;
    uint32_t vrefint_mv;
    int32_t temp_celsius;

    if (temp_data == NULL)//检查指针是否为空
    {
        return 0U;
    }

    *temp_data = (DeviceData_Rx_Data_t){0};

    __DMB();//确保读取s_adc_sample_ready前，DMA传输已完成
    if (s_adc_sample_ready == 0U)
    {
        return 0U;
    }
    s_adc_sample_ready = 0U; //重置采样完成标志位，准备下一次采样

    temp_raw = s_adc_dma_buffer[ADC_TEMP_SENSOR_INDEX];//读取温度传感器ADC原始值 1通道
    vrefint_raw = s_adc_dma_buffer[ADC_VREFINT_INDEX];//读取VREFINT ADC原始值 2通道

    if ((temp_raw == 0U) || (vrefint_raw == 0U))
    {
        return 0U;
    }

    vrefint_mv = __LL_ADC_CALC_VREFANALOG_VOLTAGE(
        vrefint_raw,
        LL_ADC_RESOLUTION_12B
    );//计算VREFINT的实际电压值，单位为毫伏
    temp_celsius = __LL_ADC_CALC_TEMPERATURE(
        vrefint_mv,
        temp_raw,
        LL_ADC_RESOLUTION_12B
    );//计算温度传感器的实际温度值，单位为摄氏度

    temp_data->timestamp = TimeStamp;
    temp_data->dev_type = DEV_TYPE_GATEWAY_SELF;
    temp_data->dev_id = 0x00U;
    temp_data->data_len = sizeof(Dev_Gateway_Temp_t);
    temp_data->payload.GatewaySelf.temperature_c = (float)temp_celsius;

    return 1U;
}

void Task_Edge_Run(void *argument)
{
  // 单条设备数据，由共享批次函数打包后进入静态队列。
  DeviceData_Rx_Data_t temp_data; //用于存储温湿度传感器数据的队列Item结构体

  while (Temp_Start() == 0U)//等待温湿度传感器初始化完
  {
    osDelay(600);   
  }

  /* Infinite loop */
  for(;;)
  {
    osDelay(600);   //每600ms执行一次Task

    if (Get_Temp(&temp_data) != 0U)
    {
      DeviceData_QueueSample(&temp_data);
    }
  }
}
