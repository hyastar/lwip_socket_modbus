/**************************************************************************************************
  Filename:       SerialApp.h
  Revised:        $Date: 2025-10-30 $

  Description:    This file contains the Serial Application definitions.
                  (Modified for V3.1 non-blocking sensor reading and OTA)

  Copyright 2004-2007 Texas Instruments Incorporated. All rights reserved.
... (版权信息)...
**************************************************************************************************/

#ifndef SERIALAPP_H
#define SERIALAPP_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"

/*********************************************************************
 * CONSTANTS
 */

// These constants are only for example and should be changed to the
// device's needs
#define SERIALAPP_ENDPOINT           11

#define SERIALAPP_PROFID             0x0F05
#define SERIALAPP_DEVICEID           0x0001
#define SERIALAPP_DEVICE_VERSION     0
#define SERIALAPP_FLAGS              0

// --- V3.1 更新: 最大集群数量改为 3 ---
#define SERIALAPP_MAX_CLUSTERS       3

// --- V3.1 规范: 传感器数据上报 ---
// (原版 CLUSTERID 5 已移除)
#define SENSOR_DATA_CLUSTER       0x0001 // 上行数据 (V3.1 规范)

// --- V3.1 规范: 下行控制指令 ---
#define CONTROL_CMD_CLUSTER       0x0002 // 下行控制 (V3.1 规范)

// --- V3.1 规范: OTA 升级通道 ---
#define SERIALAPP_OTA_CLUSTER     0x0003 // OTA (V3.1 规范)


// --- 传感器周期读取事件 (值 0x0004) ---
#define SENSOR_READ_EVT           0x0004

// --- 协调器心跳事件 (V3.1 链路 B) ---
// (在调试阶段暂不使用, 但为第二阶段保留)
#define HEARTBEAT_EVT             0x0008

// --- (关键) SHT30 数据就绪事件 (非阻塞) ---
// (由 i2c_sht30 模块在 50ms 后触发)
#define SHT30_DATA_READY_EVT      0x0010


/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
extern uint8 SerialApp_TaskID;

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Task Initialization for the Serial Transfer Application
 */
extern void SerialApp_Init( uint8 task_id );

/*
 * Task Event Processor for the Serial Transfer Application
 */
extern UINT16 SerialApp_ProcessEvent( uint8 task_id, UINT16 events );

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* SERIALAPP_H */

