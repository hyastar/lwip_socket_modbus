/**************************************************************************************************
  Filename:       i2c_sht30.c
  Revised:        $Date: 2025-10-30 $

  Description:    SHT30 温湿度传感器模块驱动 实现文件。
                  (V3.2 版: 支持非阻塞 OSAL 定时器)
                  (V4.2 版: 修复了 I2C 失败导致定时器停止的 Bug)
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "i2c_sht30.h"
#include "bsp_i2c.h"
#include "OSAL.h"
#include "OSAL_Memory.h"
#include "OSAL_Timers.h" // (V3.2) 必须包含 OSAL 定时器
#include "SerialApp.h"   // (V3.2) 需要 SHT30_DATA_READY_EVT 事件定义

/*********************************************************************
 * CONSTANTS
 */

// SHT30 I2C 地址 (7位)
#define SHT30_ADDR 0x44

// SHT30 命令: 单次测量, 高精度, 时钟拉伸禁用
#define SHT30_CMD_MEAS_HIGH 0x2C06

// (V3.2) SHT30 测量所需的时间 (根据您的要求, 设置为 50ms)
#define SHT30_MEAS_WAIT_MS 50

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 sht30_TaskID; // (V3.2) 保存应用层任务 ID

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/**
 * @brief   向 SHT30 写入一个 16 位命令。
 * @param   cmd - 16 位命令
 * @return  1=成功, 0=失败 (NACK)
 */
static uint8 SHT30_WriteCmd(uint16 cmd)
{
    bsp_I2C_Start();
    
    // 1. 发送地址 + 写(0)
    bsp_I2C_SendByte(SHT30_ADDR << 1 | 0);
    if(bsp_I2C_GetAck())
    {
        bsp_I2C_Stop();
        return 0; // 无应答
    }
    
    // 2. 发送命令高字节
    bsp_I2C_SendByte(cmd >> 8);
    if(bsp_I2C_GetAck())
    {
        bsp_I2C_Stop();
        return 0; // 无应答
    }
    
    // 3. 发送命令低字节
    bsp_I2C_SendByte(cmd & 0xFF);
    if(bsp_I2C_GetAck())
    {
        bsp_I2C_Stop();
        return 0; // 无应答
    }
    
    bsp_I2C_Stop();
    return 1;
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief   初始化 SHT30 传感器。
 * @param   task_id - OSAL 任务 ID, 用于发送事件。
 */
void i2c_sht30_Init(uint8 task_id)
{
    // (V3.2) 保存 Task ID
    sht30_TaskID = task_id;
    
    // 初始化 I2C 总线引脚 (P0.5, P0.6)
    bsp_I2C_Init();
    
    // (可选) 可以在此发送一个 SHT30 软复位命令
    // SHT30_WriteCmd(0x30A2);
}

/**
 * @brief   (V3.2) 启动一次 SHT30 测量 (非阻塞)。
 * @return  1=成功启动, 0=I2C命令发送失败
 */
uint8 i2c_sht30_StartMeasurement(void)
{
    uint8 i2c_success = 1; // (V4.2) 假设成功
    
    // 1. 发送测量命令
    if(!SHT30_WriteCmd(SHT30_CMD_MEAS_HIGH))
    {
        i2c_success = 0; // (V4.2) 发送命令失败, 标记一下
    }
    
    // 2. (V4.2 修正) 无论 I2C 是否成功, 都要启动定时器
    //    否则 SHT30_DATA_READY_EVT 事件将永远不会触发,
    //    导致 SerialApp 中的 5 秒循环停止
    osal_start_timerEx(sht30_TaskID, SHT30_DATA_READY_EVT, SHT30_MEAS_WAIT_MS);
    
    return i2c_success;
}

/**
 * @brief   (V3.2) 读取 SHT30 传感器的温湿度数据。
 * @return  V3.1 规范的 Payload (8 字节) 或 0 (失败)
 */
uint8 i2c_sht30_ReadData(uint8 *pBuf)
{
    uint8 data_buffer[6];
    uint16 raw_temp, raw_rh;
    float temp, rh;

    // (V3.2) 此时定时器已到期, 传感器数据已准备好
    
    // 1. 启动 I2C 读
    bsp_I2C_Start();
    bsp_I2C_SendByte(SHT30_ADDR << 1 | 1); // 7位地址 + 读(1)
    if(bsp_I2C_GetAck())
    {
        bsp_I2C_Stop();
        return 0; // 无应答
    }
    
    // 2. 读取 6 字节数据 (T_MSB, T_LSB, T_CRC, RH_MSB, RH_LSB, RH_CRC)
    data_buffer[0] = bsp_I2C_ReadByte(); // T_MSB
    bsp_I2C_SendAck(0); // 发送ACK
    data_buffer[1] = bsp_I2C_ReadByte(); // T_LSB
    bsp_I2C_SendAck(0); // 发送ACK
    data_buffer[2] = bsp_I2C_ReadByte(); // T_CRC (忽略)
    bsp_I2C_SendAck(0); // 发送ACK
    data_buffer[3] = bsp_I2C_ReadByte(); // RH_MSB
    bsp_I2C_SendAck(0); // 发送ACK
    data_buffer[4] = bsp_I2C_ReadByte(); // RH_LSB
    bsp_I2C_SendAck(0); // 发送ACK
    data_buffer[5] = bsp_I2C_ReadByte(); // RH_CRC (忽略)
    bsp_I2C_SendAck(1); // 发送NACK (结束读取)
    
    bsp_I2C_Stop();
    
    // 3. (可选) 校验CRC... 为简单起见, 此处省略
    
    // 4. 转换数据
    raw_temp = (data_buffer[0] << 8) | data_buffer[1];
    raw_rh   = (data_buffer[3] << 8) | data_buffer[4];
    
    // 5. 计算实际温湿度 (根据SHT30手册公式)
    temp = -45.0f + 175.0f * (raw_temp / 65535.0f);
    rh   = 100.0f * (raw_rh / 65535.0f);
    
    // 6. 按照 V3.1 规范打包数据到缓冲区
    // pBuf[0..3] = float temp
    // pBuf[4..7] = float rh
    osal_memcpy(pBuf, &temp, 4);
    osal_memcpy(pBuf + 4, &rh, 4);
    
    // 7. 返回 Payload 长度
    return 8;
}

