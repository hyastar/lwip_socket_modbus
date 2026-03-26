/*********************************************************************
 * INCLUDES
 */
#include <stdio.h>    // 用于 sprintf
#include <string.h>   // 用于 strlen
#include <ioCC2530.h> 

#include "AF.h"
#include "OnBoard.h"
#include "OSAL.h"
#include "OSAL_Memory.h" // 用于 osal_mem_alloc
#include "OSAL_Tasks.h"
#include "SerialApp.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "hal_drivers.h"
#include "hal_led.h"
#include "hal_uart.h"   // (V4.0) 所有设备都需要

// --- 传感器管理层头文件 ---
#include "sensor.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#if !defined( SERIAL_APP_PORT )
#define SERIAL_APP_PORT  0  // (V4.7) 我们将只使用 UART 0
#endif

// #define HAL_UART_PORT_BRIDGE   HAL_UART_PORT_1

// V3.1 规范波特率 (V4.9 统一使用此波特率)
#define BRIDGE_BAUD            HAL_UART_BR_115200 

// (V4.7) 移除旧的 9600 波特率定义
// #if !defined( SERIAL_APP_BAUD )
// #define SERIAL_APP_BAUD  HAL_UART_BR_9600 
// #endif

// --- V3.1 更新: 集群列表 ---
const cId_t SerialApp_ClusterList[SERIALAPP_MAX_CLUSTERS] =
{
  SENSOR_DATA_CLUSTER,
  CONTROL_CMD_CLUSTER,
  SERIALAPP_OTA_CLUSTER
};

// --- V3.1 更新: 简单描述符 (集群数量已在 .h 中改为 3) ---
const SimpleDescriptionFormat_t SerialApp_SimpleDesc =
{
  SERIALAPP_ENDPOINT,              //  int   Endpoint;
  SERIALAPP_PROFID,                //  uint16 AppProfId[2];
  SERIALAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  SERIALAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  SERIALAPP_FLAGS,                 //  int   AppFlags:4;
  SERIALAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)SerialApp_ClusterList,  //  byte *pAppInClusterList;
  SERIALAPP_MAX_CLUSTERS,          //  byte  AppNumOutClusters; (与 InClusters 保持一致)
  (cId_t *)SerialApp_ClusterList   //  byte *pAppOutClusterList;
};

// --- V3.1 更新: 端点描述符 ---
endPointDesc_t SerialApp_epDesc =
{
  SERIALAPP_ENDPOINT,
 &SerialApp_TaskID,
  (SimpleDescriptionFormat_t *)&SerialApp_SimpleDesc,
  noLatencyReqs
};

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
devStates_t SampleApp_NwkState;   
uint8 SerialApp_TaskID;           // 任务 ID.

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// --- 节点: 传感器数据发送的目标地址 (协调器) ---
#if !defined(ZDO_COORDINATOR)
static afAddrType_t SerialApp_CoordAddr =
{
  .addrMode = (afAddrMode_t)Addr16Bit,
  .endPoint = SERIALAPP_ENDPOINT, // 目标端点 11
  .addr.shortAddr = 0x0000        // 0x0000 永远是协调器的短地址
};

// --- 节点: 传感器数据包的事务 ID (防止重复) ---
static uint8 SerialApp_TransID = 0;
#endif // !defined(ZDO_COORDINATOR)


/*********************************************************************
 * LOCAL FUNCTIONS
 */

// (节点) V3.1 辅助函数: 打包 TLV 并发送数据
#if !defined(ZDO_COORDINATOR)
static void SerialApp_SendTLVData(uint8 *payloadData, uint8 payloadLen);
#endif

// (协调器) V3.1 消息处理: 解析 TLV 并打印
#if defined(ZDO_COORDINATOR)
static void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );
#endif // ZDO_COORDINATOR


// (协调器) V4.7: V3.1 规范 链路 B 函数 (运行在 UART 0)
#if defined(ZDO_COORDINATOR)

/**
 * @brief (V4.7) UART 接收回调函数 (占位符, 专用于 UART 0)
 */
static void SerialApp_UART_CB_BRIDGE(uint8 port, uint8 event);

/**
 * @brief (V4.3) 解析来自 STM32 的下行帧 (占位符)
 */
static void ParseDownlinkFrame(uint8 *frameData, uint8 frameLen);

/**
 * @brief (V4.3) 向 STM32 发送上行帧 (封装 链路 B)
 */
static void SendUplinkFrame(uint16 srcAddr, uint8 *payload, uint8 payloadLen);

/**
 * @brief (V4.3) 向 STM32 发送心跳帧 (封装 链路 B)
 */
static void SendHeartbeat(void);

/**
 * @brief (V4.3) V3.1 规范: CRC-16/MODBUS 计算
 */
static uint16 crc16_modbus(const uint8 *data, uint16 len);

// V3.1 规范: CRC-16 查表 [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 3.6]
// (V4.8) 修复这个被截断的表
static const uint16 crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

#endif // ZDO_COORDINATOR


/*********************************************************************
 * @fn      SerialApp_Init
 *
 * @brief   应用任务初始化函数。
 *
 * @param   task_id - OSAL 分配的任务 ID.
 *
 * @return  无
 */
void SerialApp_Init( uint8 task_id )
{
  SerialApp_TaskID = task_id;
  SampleApp_NwkState = DEV_INIT;       
  
  // 注册 AF 端点
  afRegister( (endPointDesc_t *)&SerialApp_epDesc );

  // (V4.7) --- 协调器: 配置 单串口 (UART 0) ---
#if defined(ZDO_COORDINATOR)
  {
    halUARTCfg_t uartConfig;
    
    // --- 0. 配置外设引脚映射 (UART 0 @ 备用位置 1) ---
    // (U0CFG = 0, U1CFG = 0)
    PERCFG &= ~0x03; // 确保 UART0 和 UART1 都在备用位置 1
    
    // --- 1. 配置 桥接数据端口 (UART 0 @ P0.2/P0.3, 115200 波特率) ---
    P0SEL |= 0x0C;   // 0b00001100: P0.2 和 P0.3 设置为外设功能
    P0DIR &= ~0x04;  // P0.2 (RXD) 设置为输入
    P0DIR |= 0x08;   // P0.3 (TXD) 设置为输出
    
    // (V4.7) 确保 P0 优先于 P2
    P2DIR &= ~0XC0;
  
    uartConfig.configured           = TRUE;
    uartConfig.baudRate             = BRIDGE_BAUD; // (V4.7) 关键: 修改为 115200
    uartConfig.flowControl          = FALSE;
    uartConfig.flowControlThreshold = 0;
    uartConfig.rx.maxBufSize        = 128;   // (V4.7) 关键: 开启接收
    uartConfig.tx.maxBufSize        = 128;
    uartConfig.idleTimeout          = 6;
    uartConfig.intEnable            = TRUE;  // (V4.7) 关键: 开启中断
    uartConfig.callBackFunc         = SerialApp_UART_CB_BRIDGE; // (V4.7) 关键: 设置回调
    HalUARTOpen (SERIAL_APP_PORT, &uartConfig); // 打开 UART 0
    
    
    // (V4.3) 启动 V3.1 规范心跳定时器
    osal_start_timerEx(SerialApp_TaskID, HEARTBEAT_EVT, 5000);
  }
  
  // (V4.9) --- 终端/路由: 配置 调试 UART (UART 0) @ 115200 ---
#else 
  {
    halUARTCfg_t uartConfig;
  
    // 路由/终端 统一使用 115200 波特率
    uartConfig.configured           = TRUE;
    uartConfig.baudRate             = BRIDGE_BAUD; 
    uartConfig.flowControl          = FALSE;
    uartConfig.flowControlThreshold = 0;
    uartConfig.rx.maxBufSize        = 0; // 节点默认不接收串口数据
    uartConfig.tx.maxBufSize        = 128;
    uartConfig.idleTimeout          = 6;
    uartConfig.intEnable            = FALSE;
    uartConfig.callBackFunc         = NULL; 
    HalUARTOpen (SERIAL_APP_PORT, &uartConfig);
  }
#endif // ZDO_COORDINATOR

  
  //  --- 移除所有协调器的启动打印信息 ---
  #if defined(ZDO_COORDINATOR)
    // HalUARTWrite(SERIAL_APP_PORT, (uint8*)"Coordinator Ready (V4.7 Single UART)\r\n", 39);
  #elif defined(ZDO_ENDDEVICE)
    //  路由/终端的打印信息是保留的
    HalUARTWrite(SERIAL_APP_PORT, (uint8*)"EndDevice Ready (V4.9 UART@115200)\r\n", 36);
  #else
    HalUARTWrite(SERIAL_APP_PORT, (uint8*)"Router Ready (V4.9 UART@115200)\r\n", 33);
  #endif

  // --- 初始化传感器管理层 ---
  Sensor_Init( task_id );
}

/*********************************************************************
 * @fn      SerialApp_ProcessEvent
 *
 * @brief   应用任务事件处理器。
 *
 * @param   task_id  - 任务 ID.
 * @param   events   - 事件位图.
 *
 * @return  未处理的事件.
 */
UINT16 SerialApp_ProcessEvent( uint8 task_id, UINT16 events )
{
  (void)task_id;  // 未使用的参数
  
  if ( events & SYS_EVENT_MSG )
  {
    afIncomingMSGPacket_t *MSGpkt;

    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SerialApp_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
      case AF_INCOMING_MSG_CMD:
        // --- (V4.5) 协调器: 处理传入的 Zigbee 消息 (双口转发) ---
#if defined(ZDO_COORDINATOR)
        SerialApp_ProcessMSGCmd( MSGpkt );
#endif
        break;
        
      case ZDO_STATE_CHANGE:
        SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
        
        // (V4.0) --- 打印网络状态变化 (调试信息) ---
        {
          // (V4.7) 协调器不再打印状态, 只让 路由/终端 打印
#if !defined(ZDO_COORDINATOR)
          char printBuf[40]; // 局部缓冲区
          
          if ( (SampleApp_NwkState == DEV_ZB_COORD)      // 0x09
              || (SampleApp_NwkState == DEV_ROUTER)        // 0x07
              || (SampleApp_NwkState == DEV_END_DEVICE) ) // 0x06
          {
              // 设备已入网
              sprintf(printBuf, "Network: Joined/Formed (State: 0x%02X)\r\n", SampleApp_NwkState);
              HalUARTWrite(SERIAL_APP_PORT, (uint8*)printBuf, strlen(printBuf)); 
              
              HalLedSet(HAL_LED_1, HAL_LED_MODE_ON);
              
              // --- 节点: 启动传感器周期性读取 ---
              Sensor_StartPeriodic();
          }
          else
          {
              sprintf(printBuf, "Network: State changed (State: 0x%02X)\r\n", SampleApp_NwkState);
              HalUARTWrite(SERIAL_APP_PORT, (uint8*)printBuf, strlen(printBuf)); 
              HalLedSet(HAL_LED_1, HAL_LED_MODE_OFF); 
          }
#else
          // (V4.7) 协调器入网后, 只亮灯, 不打印
          if ( (SampleApp_NwkState == DEV_ZB_COORD) )
          {
              HalLedSet(HAL_LED_1, HAL_LED_MODE_ON);
          }
          else
          {
              HalLedSet(HAL_LED_1, HAL_LED_MODE_OFF);
          }
#endif // !defined(ZDO_COORDINATOR)
        }
        break;

      default:
        break;
      }

      // 释放消息
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    return ( events ^ SYS_EVENT_MSG );
  }

// --- (V4.1) 周期性传感器读取事件 (路由/终端) ---
 if ( events & SENSOR_READ_EVT )
 {
#if !defined(ZDO_COORDINATOR)
    // --- 强制打印当前状态 ---
    char printBuf[40];
    sprintf(printBuf, "Debug: Current NwkState = 0x%02X\r\n", SampleApp_NwkState);
    HalUARTWrite(SERIAL_APP_PORT, (uint8*)printBuf, strlen(printBuf));
    
    // 如果状态不是 0x07 (Router)，则尝试继续打印状态，不读取传感器
    if (SampleApp_NwkState != DEV_ROUTER) {
         // 每 2 秒检查一次状态，直到入网
         osal_start_timerEx(SerialApp_TaskID, SENSOR_READ_EVT, 2000); 
    } else {
         Sensor_StartMeasurement();
    }
#endif 
    return ( events ^ SENSOR_READ_EVT );
}
  
  // --- (V4.1) 传感器数据就绪事件 (路由/终端) ---
  if ( events & SHT30_DATA_READY_EVT )
  {
#if !defined(ZDO_COORDINATOR)
    
    uint8 payloadData[10]; 
    uint8 payloadLen;
    
    payloadLen = Sensor_ReadData(payloadData);

    if (payloadLen > 0)
    {
      // (V4.0) --- 3. 在本地打印传感器数据 ---
      {
        char printBuf[80];
        uint8 deviceType = (uint8)Sensor_GetDeviceType();
        
        if (deviceType == DEVICE_TYPE_SHT30 && payloadLen == 8) // SHT30
        {
          float temp, humi;
          osal_memcpy(&temp, &payloadData[0], 4);
          osal_memcpy(&humi, &payloadData[4], 4);
          sprintf(printBuf, "Sensor: Local SHT30, Temp: %dC, Humi: %d%%\r\n\r\n", (int)temp, (int)humi); 
        }
        else if ((deviceType == DEVICE_TYPE_MQ2 || deviceType == DEVICE_TYPE_MQ5) && payloadLen == 2) // MQx
        {
          uint16 adcValue = (uint16)payloadData[0] | ((uint16)payloadData[1] << 8);
          sprintf(printBuf, "Sensor: Local MQx (0x%02X), ADC: %u\r\n\r\n", deviceType, adcValue); 
        }
        else
        {
          sprintf(printBuf, "Sensor: Local Unknown (0x%02X), Len: %d\r\n\r\n", deviceType, payloadLen); 
        }
        HalUARTWrite(SERIAL_APP_PORT, (uint8*)printBuf, strlen(printBuf));
      }

      // 4. (V3.1) 打包成 TLV 帧并发送数据包到协调器
      SerialApp_SendTLVData(payloadData, payloadLen);
    }
    
    // 5. (V4.2 修正) 重启 5 秒定时器
    osal_start_timerEx(SerialApp_TaskID, SENSOR_READ_EVT, SENSOR_READ_PERIOD);

#endif // !defined(ZDO_COORDINATOR)
    
    return ( events ^ SHT30_DATA_READY_EVT );
  }

  // --- (V4.3) 协调器心跳事件 (链路 B) ---
  if ( events & HEARTBEAT_EVT )
  {
#if defined(ZDO_COORDINATOR)
    // 1. 发送 V3.1 规范的心跳包到 STM32 (数据口 UART 0)
    SendHeartbeat();
    
    // (V4.7) 移除所有调试打印
    // HalUARTWrite(SERIAL_APP_PORT, (uint8*)"[Heartbeat] Sent to STM32 (UART1)\r\n", 35);
    
    // 2. 重新启动 5 秒定时器
    osal_start_timerEx(SerialApp_TaskID, HEARTBEAT_EVT, 5000);
#endif // ZDO_COORDINATOR
    
    return ( events ^ HEARTBEAT_EVT );
  }


  return ( 0 );  // 丢弃未知事件
}

/*********************************************************************
 * @fn      SerialApp_SendTLVData
 *
 * @brief   (节点) V3.1 辅助函数: 打包 TLV 并发送数据
 *
 * @param   payloadData - 指向原始传感器数据 (例如 8 字节 SHT30)
 * @param   payloadLen  - 原始传感器数据的长度 (例如 8)
 *
 * @return  无
 */
#if !defined(ZDO_COORDINATOR)
static void SerialApp_SendTLVData(uint8 *payloadData, uint8 payloadLen)
{
  uint8 deviceType;
  uint8 *tlvBuffer; // 指向 TLV 帧的指针
  uint8 tlvLen;
  
  deviceType = (uint8)Sensor_GetDeviceType();
  tlvLen = payloadLen + 2;
  
  tlvBuffer = osal_mem_alloc(tlvLen);
  if (tlvBuffer == NULL)
  {
    return; // 内存分配失败
  }
  
  tlvBuffer[0] = deviceType; // 帧T: 设备类型
  tlvBuffer[1] = payloadLen; // 帧L: 负载长度
  osal_memcpy(&tlvBuffer[2], payloadData, payloadLen);
  
  AF_DataRequest(
    &SerialApp_CoordAddr,       // 目标地址 (协调器)
    &SerialApp_epDesc,          // 源端点描述符
    SENSOR_DATA_CLUSTER,        // (V3.1) 目标集群 ID
    tlvLen,                     // 数据长度 (TLV 总长度)
    tlvBuffer,                  // 数据缓冲区 (TLV 帧)
    &SerialApp_TransID,         // 事务 ID (自增)
    AF_DISCV_ROUTE,             // 选项: 自动发现路由
    AF_DEFAULT_RADIUS           // 默认跳数
  );
  
  osal_mem_free(tlvBuffer);
}
#endif // !defined(ZDO_COORDINATOR)


/*********************************************************************
 * @fn      SerialApp_ProcessMSGCmd
 *
 * @brief   (协调器) V4.7 Zigbee 消息处理器 (单口转发模式)。
 *
 * @param   pkt - 指向收到的 Zigbee 消息包
 *
 * @return  无
 */
#if defined(ZDO_COORDINATOR)
static void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
  uint16 srcAddr = pkt->srcAddr.addr.shortAddr;
  uint8 *payload = pkt->cmd.Data;
  uint8 payloadLen = pkt->cmd.DataLength;

  // (V4.7) 2. 将原始二进制数据转发到 数据端口 (UART 0)
  if ( (pkt->clusterId == SENSOR_DATA_CLUSTER) || 
       (pkt->clusterId == SERIALAPP_OTA_CLUSTER) ) 
  {
      SendUplinkFrame(srcAddr, payload, payloadLen); // 发送到 UART 0 (V3.1 规范)
  }
}
#endif // ZDO_COORDINATOR


// =================================================================
// --- (V4.7) 协调器 链路 B (V3.1 规范) 实现函数 (全部走 UART 0) ---
// =================================================================
#if defined(ZDO_COORDINATOR)

/**
 * @brief (V4.3) V3.1 规范: CRC-16/MODBUS 计算
 * [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 3.6]
 */
static uint16 crc16_modbus(const uint8 *data, uint16 len) 
{
    uint16 crc = 0xFFFF;
    for (uint16 i = 0; i < len; i++) {
        uint8 index = (uint8)(crc ^ data[i]);
        crc = (crc >> 8) ^ crc16_table[index];
    }
    return crc;
}

/**
 * @brief (V4.3) 向 STM32 发送上行帧 (封装 链路 B)
 * [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 4.2.1]
 */
static void SendUplinkFrame(uint16 srcAddr, uint8 *payload, uint8 payloadLen)
{
    // 帧缓冲区: SOF(1) + SRC(2) + LEN(1) + Payload(N) + CRC(2)
    uint8 frame[256];
    uint16 crc;
    uint8 frameLen;
    
    // 1. 检查长度是否溢出 (Payload 最大 256-6=250 字节)
    if (payloadLen > (256 - 6))
    {
      return; 
    }

    // 2. 构造帧头
    frame[0] = 0xAA; // SOF
    frame[1] = (srcAddr >> 8) & 0xFF; // SRC_H
    frame[2] = srcAddr & 0xFF; // SRC_L
    frame[3] = payloadLen; // LEN
    
    // 3. 拷贝 Payload (完整的 链路 A TLV 结构)
    if (payloadLen > 0)
    {
      osal_memcpy(&frame[4], payload, payloadLen);
    }
    
    // 4. 计算 CRC (从 SRC_H 到 Payload 末尾, 共 3+N 字节)
    // [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 3.2]
    crc = crc16_modbus(&frame[1], 3 + payloadLen);
    
    // 5. 填充 CRC
    frameLen = 4 + payloadLen;
    frame[frameLen++] = crc & 0xFF; // CRC_L
    frame[frameLen++] = (crc >> 8) & 0xFF; // CRC_H
    
    // 6. (V4.7) 关键: 发送到桥接数据端口 UART 0
    HalUARTWrite(SERIAL_APP_PORT, frame, frameLen);
}

/**
 * @brief (V4.3) 向 STM32 发送心跳帧 (封装 链路 B)
 * [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 4.2.3]
 */
static void SendHeartbeat(void)
{
    uint8 frame[6];
    uint16 crc;
    
    frame[0] = 0xAA; // SOF
    frame[1] = 0x00; // SRC_H = 0x00
    frame[2] = 0x00; // SRC_L = 0x00 (协调器地址)
    frame[3] = 0x00; // LEN = 0 (无 Payload)
    
    // 计算 CRC (计算 3 字节: SRC_H, SRC_L, LEN)
    // [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 3.4]
    crc = crc16_modbus(&frame[1], 3);
    frame[4] = crc & 0xFF;
    frame[5] = (crc >> 8) & 0xFF;
    
    // (V4.7) 关键: 发送到桥接数据端口 UART 0
    HalUARTWrite(SERIAL_APP_PORT, frame, 6);
}

/**
 * @brief (V4.7) UART 接收回调函数 (占位符, 专用于 UART 0)
 * [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 4.2.2]
 */
static void SerialApp_UART_CB_BRIDGE(uint8 port, uint8 event)
{
  (void)port;
  (void)event;
  
  // (V4.7) 占位符: 
  // 此处应实现 V3.1 规范 5.2 节中的 UART 接收状态机。
  // 状态机应逐字节读取数据: HalUARTRead(SERIAL_APP_PORT, ...)
  // 并在接收到完整且 CRC 正确的帧时，调用 ParseDownlinkFrame()
  
  // 示例: (您需要实现一个真正的状态机来代替这个)
  /*
  uint8 buf[128];
  uint16 len = HalUARTRead(SERIAL_APP_PORT, buf, 128);
  if (len > 0)
  {
     // (占位符) 假设 buf 是一个完整的、CRC 正确的帧
     // ParseDownlinkFrame(buf, len);
  }
  */
}

/**
 * @brief (V4.3) 解析来自 STM32 的下行帧 (占位符)
 * [cite: STM32-Zigbee混合通信规范V3.1(OTA).md, section 4.2.2]
 */
static void ParseDownlinkFrame(uint8 *frameData, uint8 frameLen)
{
  (void)frameData;
  (void)frameLen;
  
  // (V4.3) 占位符:
  // 此函数应由 SerialApp_UART_CB_BRIDGE 在校验 CRC 成功后调用
  
  /*
  uint16 dstAddr;
  uint8 payloadLen;
  uint8 *payload;
  afAddrType_t dstAddrStruct;
  uint16 clusterId;
  
  // 1. 提取目标地址 (DST_H, DST_L)
  dstAddr = ((uint16)frameData[1] << 8) | frameData[2];
  
  // 2. 提取 Payload (链路 A TLV)
  payloadLen = frameData[3];
  payload = &frameData[4];
  
  // 3. 构造 AF 地址结构
  dstAddrStruct.addrMode = afAddr16Bit;
  dstAddrStruct.addr.shortAddr = dstAddr;
  dstAddrStruct.endPoint = SERIALAPP_ENDPOINT;
  
  // 4. 选择 ClusterID (V3.1 规范: 0x0002 或 0x0003)
  if (payload[0] >= 0xE0 && payload[0] <= 0xE3) {
      clusterId = SERIALAPP_OTA_CLUSTER; // 0x0003
  } else {
      clusterId = CONTROL_CMD_CLUSTER; // 0x0002
  }
  
  // 5. 发送到 Zigbee 节点
  AF_DataRequest(&dstAddrStruct,
      &SerialApp_epDesc,
      clusterId,
      payloadLen, payload, 
      &SerialApp_TransID, // (V4.3) 注意: 下行也需要 TransID
      AF_DISCV_ROUTE,
      AF_DEFAULT_RADIUS);
  */
}

#endif // ZDO_COORDINATOR


/*********************************************************************
*********************************************************************/

