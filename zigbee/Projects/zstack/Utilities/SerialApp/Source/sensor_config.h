/**************************************************************************************************
  Filename:       my_sensor_config.h
  Revised:        $Date: 2025-10-30 $

  Description:    自定义传感器编译配置。
                  (V3.7 - 根据 IAR 设备角色 (ZDO_COORDINATOR, ZDO_ENDDEVICE) 自动选择)
**************************************************************************************************/

#ifndef MY_SENSOR_CONFIG_H
#define MY_SENSOR_CONFIG_H

// --- 包含 Z-Stack 宏定义，以便我们可以检查 ZDO_COORDINATOR 等 ---
// (注意: 此文件必须在 ZComDef.h 或 ZGlobals.c 之后被包含)
// (更新: sensor.h 会先包含 ZComDef.h, 所以这里是安全的)

/* ================== 自动传感器配置 (V3.7) ================== */
// 本文件根据 IAR 项目中定义的设备角色宏来自动配置传感器。
// IAR 项目选项中不再需要定义 ACTIVE_SENSOR_TYPE。

// 1. 如果是协调器 (IAR 项目中定义了 ZDO_COORDINATOR)
#if defined(ZDO_COORDINATOR)
  // 协调器不挂载传感器，所以不定义 ACTIVE_SENSOR_TYPE
  // sensor.c 中的 #if 逻辑将不会编译任何驱动

// 2. 如果是终端设备 (IAR 项目中定义了 ZDO_ENDDEVICE)
#elif defined(ZDO_ENDDEVICE)
  // 终端设备挂载 MQ2 或 MQ5
  // 在这里手动选择要编译的终端传感器：
  
  //(a) 编译 MQ2 终端时
  //#define ACTIVE_SENSOR_TYPE 0x20

  //(b) 编译 MQ5 终端时 
  #define ACTIVE_SENSOR_TYPE 0x21

// 3. 如果是路由 (IAR 项目中既没有定义 ZDO_COORDINATOR 也没有定义 ZDO_ENDDEVICE)
#else 
  // 默认为路由设备
  // 路由挂载 SHT30
  #define ACTIVE_SENSOR_TYPE 0x01

#endif
/* ============================================================= */


#endif /* MY_SENSOR_CONFIG_H */

