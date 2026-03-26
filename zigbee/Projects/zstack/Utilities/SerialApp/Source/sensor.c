/**************************************************************************************************
  Filename:       sensor.c
  Revised:        $Date: 2025-10-30 $

  Description:    通用传感器管理层 实现文件。
                  (V3.2 版: 支持非阻塞启动和读取)
                  (V3.4 版: 彻底移除 Internal Temp 支持)
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "sensor.h"
#include "OSAL.h"
#include "OSAL_Timers.h" // 包含了 osal_start_timerEx
#include "SerialApp.h"   // (V3.2) 需要 SHT30_DATA_READY_EVT

// --- 包含所有可能的具体传感器驱动 ---
// #include "adc_internal_temp.h" // (已移除)
#include "i2c_sht30.h"        
#include "adc_mq2.h"
#include "adc_mq5.h"
// #include "i2c_bmp280.h" 

/*********************************************************************
 * LOCAL VARIABLES
 */
#if !defined(ZDO_COORDINATOR)
// 保存应用层的任务 ID，用于发送事件
static uint8 sensor_TaskID;
#endif

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/**
 * @brief   初始化传感器管理层和底层的驱动。
 */
void Sensor_Init(uint8 task_id)
{
  // 协调器不需要初始化任何传感器
#if !defined(ZDO_COORDINATOR)

  sensor_TaskID = task_id;
  
  // --- 根据编译时宏，调用特定驱动的初始化函数 ---
  // (V3.2 更新: 所有 Init 函数现在都需要 task_id)
  
  #if (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_SHT30)
    i2c_sht30_Init(task_id);
    
  #elif (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_MQ2)
    adc_mq2_Init(task_id);
    
  #elif (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_MQ5)
    adc_mq5_Init(task_id);
    
  #endif
  
#else
  (void)task_id; 
#endif // !defined(ZDO_COORDINATOR)
}

/**
 * @brief   启动周期性的传感器读取定时器。
 */
void Sensor_StartPeriodic(void)
{
#if !defined(ZDO_COORDINATOR)
  osal_start_timerEx(sensor_TaskID, SENSOR_READ_EVT, SENSOR_READ_PERIOD);
#endif
}

/**
 * @brief   (V3.2 新增) 启动一次传感器测量 (非阻塞)。
 */
void Sensor_StartMeasurement(void)
{
  // 协调器不执行测量
#if !defined(ZDO_COORDINATOR)

  // --- 根据编译时宏，调用特定驱动的启动函数 ---
    
  #if (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_SHT30)
    i2c_sht30_StartMeasurement();
    
  #elif (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_MQ2)
    adc_mq2_StartMeasurement();
    
  #elif (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_MQ5)
    adc_mq5_StartMeasurement();

  #endif

#endif // !defined(ZDO_COORDINATOR)
}

/**
 * @brief   传感器事件处理器 (保留)。
 */
uint16 Sensor_ProcessEvent(uint16 events)
{
  (void)events;
  return events; 
}

/**
 * @brief   获取本节点的设备类型 (DeviceType)。
 */
DeviceType_t Sensor_GetDeviceType(void)
{
#if !defined(ZDO_COORDINATOR)
  // V3.9 修正: 显式转换为 (DeviceType_t) 以消除 Pe188 警告
  return (DeviceType_t)ACTIVE_SENSOR_TYPE;
#else
  return (DeviceType_t)0; 
#endif
}

/**
 * @brief   (V3.2 修改) 将传感器数据读取到一个缓冲区中。
 */
uint8 Sensor_ReadData(uint8 *pBuf)
{
  // 协调器不读取数据
#if !defined(ZDO_COORDINATOR)

  // --- 根据编译时宏，调用特定驱动的读取函数 ---
  // (V3.2 更新: 全部调用 _ReadData)
    
  #if (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_SHT30)
    return i2c_sht30_ReadData(pBuf);
    
  #elif (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_MQ2)
    return adc_mq2_ReadData(pBuf);
    
  #elif (ACTIVE_SENSOR_TYPE == DEVICE_TYPE_MQ5)
    return adc_mq5_ReadData(pBuf);

  #else
    return 0;
  #endif

#else
  return 0;
#endif // !defined(ZDO_COORDINATOR)
}

