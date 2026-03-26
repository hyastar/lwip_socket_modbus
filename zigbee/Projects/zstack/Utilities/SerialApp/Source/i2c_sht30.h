/**************************************************************************************************
  Filename:       i2c_sht30.h
  Revised:        $Date: 2025-10-30 $

  Description:    SHT30 温湿度传感器模块驱动 头文件。
                  (V3.2 版: 支持非阻塞 OSAL 定时器)
                  (依赖: bsp_i2c.h, OSAL_Timers.h)
***************************************************************************************************/

#ifndef I2C_SHT30_H
#define I2C_SHT30_H

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
 * @brief   初始化 SHT30 传感器。
 *
 * @param   task_id - OSAL 任务 ID (SerialApp_TaskID), 
 * 用于在测量完成后发送 SHT30_DATA_READY_EVT 事件。
 * @return  无
 */
void i2c_sht30_Init(uint8 task_id);

/**
 * @brief   (V3.2 新增) 启动一次 SHT30 测量 (非阻塞)。
 *
 * @details 此函数向 SHT30 发送 I2C "开始测量" 命令, 
 * 然后启动一个 50ms 的 OSAL 定时器。
 * 定时器触发后, OSAL 会向 task_id 发送 SHT30_DATA_READY_EVT 事件。
 *
 * @param   无
 * @return  1=成功启动, 0=I2C命令发送失败
 */
uint8 i2c_sht30_StartMeasurement(void);

/**
 * @brief   (V3.2 新增) 读取 SHT30 传感器的温湿度数据。
 *
 * @details 此函数应在 SHT30_DATA_READY_EVT 事件中被调用。
 * (此时数据已准备好, 无需延时)
 *
 * @param   pBuf - 指向缓冲区的指针。
 * @return  写入缓冲的数据长度 (字节)。
 *
 * @note    根据 V3.1 规范 (DeviceType 0x01):
 * - pBuf[0..3] = 4 字节 float (温度)
 * - pBuf[4..7] = 4 字节 float (湿度)
 * - 返回值为 8。
 * - 如果读取失败，返回值为 0。
 */
uint8 i2c_sht30_ReadData(uint8 *pBuf);


#ifdef __cplusplus
}
#endif

#endif /* I2C_SHT30_H */
