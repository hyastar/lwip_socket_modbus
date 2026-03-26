/**************************************************************************************************
  Filename:       adc_mq2.h
  Revised:        $Date: 2025-10-30 $

  Description:    MQ2 烟雾传感器模块驱动 头文件。
                  (V3.2 版: 支持非阻塞 OSAL 事件)
                  (依赖: hal_adc.h, OSAL.h)
***************************************************************************************************/

#ifndef ADC_MQ2_H
#define ADC_MQ2_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h" // 包含 Z-Stack 基本类型 (uint8, uint16)

/*********************************************************************
 * 函数
 */

/**
 * @brief   初始化 MQ2 传感器的 ADC 引脚 (P0.6 / AIN6)。
 *
 * @param   task_id - OSAL 任务 ID (SerialApp_TaskID), 
 * 用于在测量完成后发送 SHT30_DATA_READY_EVT 事件。
 * @return  无
 */
void adc_mq2_Init(uint8 task_id);

/**
 * @brief   (V3.2 新增) 启动一次 MQ2 测量 (非阻塞)。
 *
 * @details 此函数立即读取 ADC 值, 
 * 然后立即调用 osal_set_event() 来触发 SHT30_DATA_READY_EVT 事件。
 *
 * @param   无
 * @return  无
 */
void adc_mq2_StartMeasurement(void);

/**
 * @brief   (V3.2 新增) 读取 MQ2 传感器的 ADC 原始值。
 *
 * @details 此函数应在 SHT30_DATA_READY_EVT 事件中被调用。
 *
 * @param   pBuf - 指向缓冲区的指针。
 * @return  写入缓冲的数据长度 (字节)。
 *
 * @note    根据 V3.1 规范 (DeviceType 0x20):
 * - pBuf[0..1] = 2 字节 uint16 (ADC 原始值, 小端序)
 * - 返回值为 2。
 */
uint8 adc_mq2_ReadData(uint8 *pBuf);


#ifdef __cplusplus
}
#endif

#endif /* ADC_MQ2_H */
