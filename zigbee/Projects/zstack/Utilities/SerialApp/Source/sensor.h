/**************************************************************************************************
  Filename:       sensor.h
  Revised:        $Date: 2025-10-30 $

  Description:    通用传感器管理层 头文件。
                  (V3.8 - 移除强制错误检查)
                  (V4.2 - 修正: #define 替代 enum 以修复 #if 比较 Bug)
**************************************************************************************************/

#ifndef SENSOR_H
#define SENSOR_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"
#include "sensor_config.h" // (V3.8) 包含您的配置文件

/*********************************************************************
 * 常量
 */

// --- 传感器周期读取事件 (值 0x0004) ---
#define SENSOR_READ_EVT              0x0004
// --- 传感器读取周期 (5000ms) ---
#define SENSOR_READ_PERIOD           5000

// --- V3.1 规范: 传感器设备类型 ---
// (V4.2 修正) 必须使用 #define 替代 enum, 
// 否则预处理器 (#if) 在 sensor.c 中无法正确比较
#define DEVICE_TYPE_SHT30   0x01 // V3.1 规范: SHT30 温湿度
#define DEVICE_TYPE_BMP280  0x02 // V3.1 规范: BMP280 气压
#define DEVICE_TYPE_MQ2     0x20 // V3.1 规范: MQ2 烟雾
#define DEVICE_TYPE_MQ5     0x21 // V3.1 规范: MQ5 可燃气
// (V3.4) DEVICE_TYPE_INTERNAL_TEMP (已移除)

// (V4.2) 将枚举类型定义为一个普通的 uint8
typedef uint8 DeviceType_t;


// --- (V3.8) 移除了强制 #error 检查 ---


/*********************************************************************
 * 函数
 */

/**
 * @brief   初始化传感器管理层和底层的驱动。
 * @param   task_id - OSAL 任务 ID (SerialApp_TaskID)，用于发送事件。
 * @return  无
 */
void Sensor_Init(uint8 task_id);

/**
 * @brief   启动周期性的传感器读取定时器。
 * @param   无
 * @return  无
 */
void Sensor_StartPeriodic(void);

/**
 * @brief   (V3.2 新增) 启动一次传感器测量 (非阻塞)。
 * @param   无
 * @return  无
 */
void Sensor_StartMeasurement(void);

/**
 * @brief   传感器事件处理器 (保留)。
 * @param   events - 需要处理的事件位。
 * @return  未被传感器层处理的事件。
 */
uint16 Sensor_ProcessEvent(uint16 events);

/**
 * @brief   获取本节点的设备类型 (DeviceType)。
 * @param   无
 * @return  本节点的 DeviceType_t (由编译时宏决定)。
 */
DeviceType_t Sensor_GetDeviceType(void);

/**
 * @brief   (V3.2 修改) 将传感器数据读取到一个缓冲区中。
 * @param   pBuf - 指向缓冲区的指针，数据将被写入这里。
 * @return  写入缓冲的数据长度 (字节)。
 */
uint8 Sensor_ReadData(uint8 *pBuf);


#ifdef __cplusplus
}
#endif

#endif /* SENSOR_H */

