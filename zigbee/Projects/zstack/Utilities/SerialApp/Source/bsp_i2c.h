/**************************************************************************************************
  Filename:       bsp_i2c.h
  Revised:        $Date: 2025-10-30 $

  Description:    CC2530 软件模拟 I2C 驱动层 (BSP) 头文件。
                  (SDA: P0.5, SCL: P0.6)
**************************************************************************************************/

#ifndef BSP_I2C_H
#define BSP_I2C_H

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
 * @brief   初始化软件 I2C 总线的 GPIO 引脚 (P0.5, P0.6)。
 * @param   无
 * @return  无
 */
void bsp_I2C_Init(void);

/**
 * @brief   发送 I2C 起始信号。
 * @param   无
 * @return  无
 */
void bsp_I2C_Start(void);

/**
 * @brief   发送 I2C 停止信号。
 * @param   无
 * @return  无
 */
void bsp_I2C_Stop(void);

/**
 * @brief   等待 I2C 应答信号。
 * @param   无
 * @return  0=收到应答 (ACK), 1=未收到应答 (NACK)
 */
uint8 bsp_I2C_GetAck(void);

/**
 * @brief   发送 I2C 应答信号。
 * @param   ack - 0=发送ACK, 1=发送NACK
 * @return  无
 */
void bsp_I2C_SendAck(uint8 ack);

/**
 * @brief   通过 I2C 发送一个字节。
 * @param   dat - 要发送的数据 (1 字节)
 * @return  无
 */
void bsp_I2C_SendByte(uint8 dat);

/**
 * @brief   从 I2C 读取一个字节。
 * @param   无
 * @return  读取到的数据 (1 字节)
 */
uint8 bsp_I2C_ReadByte(void);


#ifdef __cplusplus
}
#endif

#endif /* BSP_I2C_H */
