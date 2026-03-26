/**************************************************************************************************
  Filename:       adc_mq5.c
  Revised:        $Date: 2025-10-30 $

  Description:    MQ5 可燃气体传感器模块驱动 实现文件。
                  (V3.2 版: 支持非阻塞 OSAL 事件)
                  使用 Z-Stack HAL ADC 驱动读取 P0.6 (AIN6)。
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "adc_mq5.h"
#include <ioCC2530.h>    // 包含寄存器定义 (用于 GPIO 配置)
#include "hal_adc.h"     // 包含 Z-Stack 官方 ADC 驱动
#include "OSAL.h"        // (V3.2) 需要 osal_set_event
#include "SerialApp.h"   // (V3.2) 需要 SHT30_DATA_READY_EVT 事件定义

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 mq5_TaskID;      // (V3.2) 保存应用层任务 ID
static uint16 mq5_AdcValue;   // (V3.2) 保存 StartMeasurement 时的 ADC 读数

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief   初始化 MQ5 传感器的 ADC 引脚 (P0.6 / AIN6)。
 */
void adc_mq5_Init(uint8 task_id)
{
  // (V3.2) 保存 Task ID
  mq5_TaskID = task_id;

#if (HAL_ADC == TRUE)
  
    // --- (V3.2 修正) ---
    // 1. 将 P0.6 (AIN6) 设置为外设功能 (P0SEL |= 0x40)
    // (保持 V3.1 .c 文件的原始设置)
    P0SEL |= 0x40;
    
    // 2. 将 P0.6 (AIN6) 设置为模拟输入 (APCFG |= 0x40)
    // (保持 V3.1 .c 文件的原始设置)
    APCFG |= 0x40;

    // 3. 初始化 Z-Stack ADC 模块
    HalAdcInit();
    
#endif
}

/**
 * @brief   (V3.2) 启动一次 MQ5 测量 (非阻塞)。
 */
void adc_mq5_StartMeasurement(void)
{
#if (HAL_ADC == TRUE)
    // 1. 读取 ADC P0.6 通道 (12位精度)
    // (HAL_ADC_CHANNEL_6 对应 AIN6, 即 P0.6)
    mq5_AdcValue = HalAdcRead(HAL_ADC_CHANNEL_6, HAL_ADC_RESOLUTION_12);
  
#else
    // ADC 功能未开启
    mq5_AdcValue = 0x0000;
#endif

    // 2. (V3.2) 立即设置 OSAL 事件, 通知应用层数据已就绪
    osal_set_event(mq5_TaskID, SHT30_DATA_READY_EVT);
}


/**
 * @brief   (V3.2) 读取 MQ5 传感器的 ADC 原始值。
 * @return  V3.1 规范的 Payload (2 字节)
 */
uint8 adc_mq5_ReadData(uint8 *pBuf)
{
  // 1. 按照 V3.1 规范 (DeviceType 0x21) 打包数据
  pBuf[0] = (uint8)(mq5_AdcValue & 0xFF);       // 低字节
  pBuf[1] = (uint8)((mq5_AdcValue >> 8) & 0xFF); // 高字节

  // 2. 返回 Payload 长度
  return 2;
}

