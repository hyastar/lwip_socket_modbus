/**************************************************************************************************
  Filename:       adc_internal_temp.c
  Revised:        $Date: 2025-10-30 $

  Description:    CC2530 内部温度传感器驱动 实现文件。
                  (V3.2 版: 支持非阻塞 OSAL 事件)
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "adc_internal_temp.h"
#include "hal_drivers.h" // 包含 Z-Stack 硬件抽象层
#include "hal_adc.h"     // 包含 ADC 驱动
#include "OSAL.h"        // (V3.2) 需要 osal_set_event
#include "SerialApp.h"   // (V3.2) 需要 SHT30_DATA_READY_EVT 事件定义

/*********************************************************************
 * MACROS
 */

// (12位精度, 1.25V 参考电压)
#define HAL_ADC_CELSIUS(v)   ((int16)((((int32)v * 1250) / 4095) - 278))

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 internal_TaskID;   // (V3.2) 保存应用层任务 ID
static int8  internal_TempC;    // (V3.2) 保存 StartMeasurement 时的读数

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief   初始化 ADC 硬件以读取内部温度。
 */
void adc_internal_temp_Init(uint8 task_id)
{
  // (V3.2) 保存 Task ID
  internal_TaskID = task_id;
  
#if (HAL_ADC == TRUE)
  HalAdcInit();
#endif
}

/**
 * @brief   (V3.2) 启动一次内部温度测量 (非阻塞)。
 */
void adc_internal_temp_StartMeasurement(void)
{
#if (HAL_ADC == TRUE)
  uint16 adcValue;

  // 1. 读取 ADC 温度通道 (12位精度)
  adcValue = HalAdcRead(HAL_ADC_CHANNEL_TEMP, HAL_ADC_RESOLUTION_12);
  
  // 2. 将 ADC 原始值转换为摄氏度
  internal_TempC = (int8)HAL_ADC_CELSIUS(adcValue);
  
#else
  // ADC 功能未开启
  internal_TempC = 0;
#endif

  // 3. (V3.2) 立即设置 OSAL 事件, 通知应用层数据已就绪
  osal_set_event(internal_TaskID, SHT30_DATA_READY_EVT);
}


/**
 * @brief   (V3.2) 读取内部温度并将其打包。
 */
uint8 adc_internal_temp_ReadData(uint8 *pBuf)
{
  // 1. 将 1 字节的摄氏度整数值写入缓冲区
  pBuf[0] = (uint8)internal_TempC;
  
  // 2. 返回数据长度
  return 1; // 长度为 1 字节
}
