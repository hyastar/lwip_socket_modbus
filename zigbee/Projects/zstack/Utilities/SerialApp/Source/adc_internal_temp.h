/**************************************************************************************************
  Filename:       adc_internal_temp.h
  Revised:        $Date: 2025-10-30 $

  Description:    CC2530 内部温度传感器驱动 头文件。
                  (V3.2 版: 支持非阻塞 OSAL 事件)
**************************************************************************************************/

#ifndef ADC_INTERNAL_TEMP_H
#define ADC_INTERNAL_TEMP_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"

/*********************************************************************
 * 函数
 */

/**
 * @brief   初始化 ADC 硬件以读取内部温度。
 *
 * @param   task_id - OSAL 任务 ID (SerialApp_TaskID), 
 * 用于在测量完成后发送 SHT30_DATA_READY_EVT 事件。
 * @return  无
 */
void adc_internal_temp_Init(uint8 task_id);

/**
 * @brief   (V3.2 新增) 启动一次内部温度测量 (非阻塞)。
 *
 * @details 此函数立即读取 ADC 值, 
 * 然后立即调用 osal_set_event() 来触发 SHT30_DATA_READY_EVT 事件。
 *
 * @param   无
 * @return  无
 */
void adc_internal_temp_StartMeasurement(void);

/**
 * @brief   (V3.2 新增) 读取内部温度并将其打包。
 *
 * @param   pBuf - 指向缓冲区的指针 (数据将被写入这里)。
 * @return  写入缓冲的数据长度 (字节)。
 */
uint8 adc_internal_temp_ReadData(uint8 *pBuf);


#ifdef __cplusplus
}
#endif

#endif /* ADC_INTERNAL_TEMP_H */
