/**
 * @file  modbus_tcp_client.h
 * @brief Modbus TCP Client — STM32F429 → Rust Server (功能码 0x10)
 *
 * 寄存器映射（与 modpoll.py / Rust Server 保持一致）：
 *   Reg 0x0000 : MQ1 ADC 原始值
 *   Reg 0x0001 : MQ2 ADC 原始值
 *   Reg 0x0002 : 温度 × 100  (如 2536 = 25.36 °C)
 *   Reg 0x0003 : 湿度 × 100  (如 6012 = 60.12 %)
 *   Reg 0x0004 : 状态标志位  (0x0001 = 数据有效)
 */

 #ifndef __MODBUS_TCP_CLIENT_H
 #define __MODBUS_TCP_CLIENT_H
 
 #include <stdint.h>
 #include <stdbool.h>
 
 /* ── 宏 ─────────────────────────────────────────────── */
 #define MODBUS_TCP_SERVER_IP   "151.242.85.89"
 #define MODBUS_TCP_SERVER_PORT 48651
 #define MODBUS_TCP_TIMEOUT_MS  3000
 
 /* Modbus TCP 帧各字段偏移（MBAP 头 + PDU） */
 #define MBAP_TRANS_ID_OFF   0   /* 事务 ID            2 B */
 #define MBAP_PROT_ID_OFF     2   /* 协议 ID (= 0)      2 B */
 #define MBAP_LEN_OFF        4    /* 长度字段            2 B */
 #define MBAP_UNIT_ID_OFF     6   /* 单元 ID             1 B */
 #define PDU_FUNC_OFF         7   /* 功能码              1 B */
 #define PDU_START_ADDR_OFF   8   /* 起始地址            2 B */
 #define PDU_REG_COUNT_OFF   10   /* 寄存器数量          2 B */
 #define PDU_BYTE_CNT_OFF    12   /* 字节数              1 B */
 #define PDU_DATA_OFF        13   /* 数据起始偏移        N B */
 
 /* 功能码 */
 #define FUNC_WRITE_MULTI_REG  0x10
 
 /* 最小帧长：MBAP(7) + 功能码(1) + 起始地址(2) + 数量(2) + 字节数(1) */
 #define MIN_FRAME_LEN 13
 
 /* 寄存器数量（固定 5 个：MQ1/MQ2/温度/湿度/状态） */
 #define REG_COUNT       5
 /* 数据字节数 = 5 寄存器 × 2 字节 */
 #define REG_DATA_BYTES  (REG_COUNT * 2)
 /* 完整发送帧长 = MBAP(7) + 固定 PDU 头(6) + 字节数(1) + 数据(10) = 24 */
 #define SEND_FRAME_LEN  (7 + 6 + 1 + REG_DATA_BYTES)
 /* 回复帧长 = MBAP(7) + 功能码(1) + 起始地址(2) + 数量(2) = 12 */
 #define RECV_FRAME_LEN  12
 
 /* 发送/接收缓冲区大小 */
 #define MB_TCP_BUF_SIZE  64
 
 /* ── 类型 ────────────────────────────────────────────── */
 
 /** @brief Modbus TCP 句柄 */
 typedef struct {
     int  sockfd;
     uint16_t trans_id;
     uint8_t  tx_buf[MB_TCP_BUF_SIZE];
     uint8_t  rx_buf[MB_TCP_BUF_SIZE];
 } ModbusTCP_HandleTypeDef;
 
 /* ── 函数声明 ────────────────────────────────────────── */
 
 /**
  * @brief  初始化 Modbus TCP 连接
  * @param  ip   服务器 IP（点分十进制字符串）
  * @param  port 服务器端口
  * @retval true 连接成功，false 失败
  */
 bool ModbusTCP_Init(const char *ip, uint16_t port);
 
 /**
  * @brief  关闭 Modbus TCP 连接
  */
 void ModbusTCP_DeInit(void);
 
 /**
  * @brief  发送一次 Write Multiple Registers (功能码 0x10)
  *
  * @param  regs  寄存器值数组，长度必须为 REG_COUNT
  *              regs[0] = MQ1 ADC
  *              regs[1] = MQ2 ADC
  *              regs[2] = 温度 × 100
  *              regs[3] = 湿度 × 100
  *              regs[4] = 状态标志位
  * @retval true  服务器正确应答，false 超时或异常
  *
  * @note 每次调用 trans_id 自动递增（0x0001 ~ 0xFFFF 循环）
  */
 bool ModbusTCP_WriteRegisters(const uint16_t regs[REG_COUNT]);
 
 /**
  * @brief  检查 Modbus TCP 是否已连接
  * @retval true 已连接，false 未连接或已断开
  */
 bool ModbusTCP_IsConnected(void);
 
 /**
  * @brief  重新连接（内部调用或手动触发）
  * @retval true 重连成功，false 失败
  */
 bool ModbusTCP_Reconnect(void);
 
 #endif /* __MODBUS_TCP_CLIENT_H */


