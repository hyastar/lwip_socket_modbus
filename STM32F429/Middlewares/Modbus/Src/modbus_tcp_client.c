/**
 * @file  modbus_tcp_client.c
 * @brief Modbus TCP 客户端实现，适用于 STM32F429 + LwIP
 */

 #include "modbus_tcp_client.h"
 #include "lwip/sockets.h"
 #include "lwip/netdb.h"
 #include "lwip/netif.h"
 #include <string.h>
 #include <stdio.h>
 
 /* 私有变量 ----------------------------------------------------------------- */
 static ModbusTCP_HandleTypeDef hmodbus = {0};
 
 /* 私有函数 ----------------------------------------------------------------- */
 
 /**
  * @brief  等待 socket 可读
  * @param  fd          socket 文件描述符
  * @param  timeout_ms  超时时间（毫秒）
  * @retval  1=可读 / 0=超时 / -1=错误
  */
 static int wait_ready(int fd, int timeout_ms)
 {
     fd_set rdset;
     struct timeval tv;
 
     FD_ZERO(&rdset);
     FD_SET(fd, &rdset);
 
     tv.tv_sec  = timeout_ms / 1000;
     tv.tv_usec = (timeout_ms % 1000) * 1000;
 
     return select(fd + 1, &rdset, NULL, NULL, &tv);
 }
 
 /* 公开 API ----------------------------------------------------------------- */
 
 bool ModbusTCP_Init(const char *ip, uint16_t port)
 {
     struct sockaddr_in server_addr;
     int ret;
 
     if (hmodbus.sockfd >= 0) {
         closesocket(hmodbus.sockfd);
     }
 
     hmodbus.sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (hmodbus.sockfd < 0) {
         printf("[ModbusTCP] socket create failed\r\n");
         return false;
     }
 
     /* 设置 recv 超时 */
     struct timeval tv;
     tv.tv_sec  = MODBUS_TCP_TIMEOUT_MS / 1000;
     tv.tv_usec = (MODBUS_TCP_TIMEOUT_MS % 1000) * 1000;
     setsockopt(hmodbus.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
 
     memset(&server_addr, 0, sizeof(server_addr));
     server_addr.sin_family      = AF_INET;
     server_addr.sin_port       = htons(port);
     server_addr.sin_addr.s_addr = inet_addr(ip);
 
     ret = connect(hmodbus.sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
     if (ret < 0) {
         printf("[ModbusTCP] connect failed\r\n");
         closesocket(hmodbus.sockfd);
         hmodbus.sockfd = -1;
         return false;
     }
 
     hmodbus.trans_id = 0;
     printf("[ModbusTCP] connected to %s:%u\r\n", ip, port);
     return true;
 }
 
 void ModbusTCP_DeInit(void)
 {
     if (hmodbus.sockfd >= 0) {
         closesocket(hmodbus.sockfd);
         hmodbus.sockfd = -1;
     }
 }
 
 bool ModbusTCP_IsConnected(void)
 {
     if (hmodbus.sockfd < 0) return false;
 
     /* 非阻塞探测 socket 是否仍然连通，send 0 字节会立即返回 */
     char dummy;
     int ret = recv(hmodbus.sockfd, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
     if (ret == 0) {
         printf("[ModbusTCP] connection closed by peer\r\n");
         return false;
     }
     return (ret >= 0);
 }
 
 bool ModbusTCP_Reconnect(void)
 {
     ModbusTCP_DeInit();
     return ModbusTCP_Init(MODBUS_TCP_SERVER_IP, MODBUS_TCP_SERVER_PORT);
 }
 
 bool ModbusTCP_WriteRegisters(const uint16_t regs[REG_COUNT])
 {
     uint8_t *tx = hmodbus.tx_buf;
     uint8_t *rx = hmodbus.rx_buf;
 
     /* 步骤 1. 组装请求帧 --------------------------------------------------- */
     /* 事务 ID，每次自增 */
     hmodbus.trans_id = (hmodbus.trans_id + 1) & 0xFFFF;
     tx[MBAP_TRANS_ID_OFF    ] = (uint8_t)(hmodbus.trans_id >> 8);
     tx[MBAP_TRANS_ID_OFF + 1] = (uint8_t)(hmodbus.trans_id & 0xFF);
 
     /* 协议 ID = 0，Modbus TCP 固定值 */
     tx[MBAP_PROT_ID_OFF    ] = 0x00;
     tx[MBAP_PROT_ID_OFF + 1] = 0x00;
 
     /* 后续长度 = PDU长度 = 单元ID(1) + 功能码(1) + 起始地址(2) + 寄存器数(2) + 字节数(1) + 数据(10) */
     uint8_t  byte_cnt = REG_DATA_BYTES;                   /* = 10 */
     uint8_t  pdu_len  = 1 + 2 + 2 + 1 + byte_cnt;        /* = 17 */
 
     tx[MBAP_LEN_OFF    ] = 0x00;
     tx[MBAP_LEN_OFF + 1] = pdu_len;                       /* = 17 */
 
     /* 单元 ID = 1，从站地址 */
     tx[MBAP_UNIT_ID_OFF] = 0x01;
 
     /* 功能码 = 0x10，写多个寄存器 */
     tx[PDU_FUNC_OFF] = FUNC_WRITE_MULTI_REG;
 
     /* 起始地址 = 0x0000 */
     tx[PDU_START_ADDR_OFF    ] = 0x00;
     tx[PDU_START_ADDR_OFF + 1] = 0x00;
 
     /* 寄存器数量 = 5 */
     tx[PDU_REG_COUNT_OFF    ] = 0x00;
     tx[PDU_REG_COUNT_OFF + 1] = REG_COUNT;                /* = 5 */
 
     /* 字节数 = 10 */
     tx[PDU_BYTE_CNT_OFF] = byte_cnt;                      /* = 10 */
 
     /* 填充寄存器数据，大端字节序，与 modpoll.py / Rust Server 保持一致 */
     for (int i = 0; i < REG_COUNT; i++) {
         tx[PDU_DATA_OFF + i * 2    ] = (uint8_t)(regs[i] >> 8);
         tx[PDU_DATA_OFF + i * 2 + 1] = (uint8_t)(regs[i] & 0xFF);
     }
 
     /* 步骤 2. 发送请求帧 --------------------------------------------------- */
     int sent = send(hmodbus.sockfd, (char *)tx, SEND_FRAME_LEN, 0);
     if (sent != SEND_FRAME_LEN) {
         printf("[ModbusTCP] send failed: %d\r\n", sent);
         return false;
     }
 
     /* 步骤 3. 接收响应帧 --------------------------------------------------- */
     int total = 0;
     int remain = RECV_FRAME_LEN;
 
     while (remain > 0) {
         if (wait_ready(hmodbus.sockfd, MODBUS_TCP_TIMEOUT_MS) != 1) {
             printf("[ModbusTCP] recv timeout\r\n");
             return false;
         }
         int n = recv(hmodbus.sockfd, (char *)rx + total, remain, 0);
         if (n <= 0) {
             printf("[ModbusTCP] recv error: %d\r\n", n);
             return false;
         }
         total += n;
         remain -= n;
     }
 
     /* 步骤 4. 校验响应帧 --------------------------------------------------- */
     /* 正常响应共 12 字节：
      *   [0-1]  事务 ID（与请求一致）
      *   [2-3]  协议 ID (= 0)
      *   [4-5]  后续长度 (= 6)
      *   [6]    单元 ID
      *   [7]    功能码 (= 0x10)
      *   [8-9]  起始寄存器地址
      *  [10-11] 写入的寄存器数量
      */
     if (total < RECV_FRAME_LEN) {
         printf("[ModbusTCP] resp too short: %d\r\n", total);
         return false;
     }
 
     if (rx[MBAP_PROT_ID_OFF] != 0x00 || rx[MBAP_PROT_ID_OFF + 1] != 0x00) {
         printf("[ModbusTCP] invalid protocol ID\r\n");
         return false;
     }
 
     /* 后续长度应为 0x00 0x06，即 MBAP 后续 6 个字节 */
     if (rx[MBAP_LEN_OFF] != 0x00 || rx[MBAP_LEN_OFF + 1] != 0x06) {
         printf("[ModbusTCP] invalid length in resp: 0x%02X%02X\r\n",
                rx[MBAP_LEN_OFF], rx[MBAP_LEN_OFF + 1]);
         return false;
     }
 
     if (rx[PDU_FUNC_OFF] != FUNC_WRITE_MULTI_REG) {
         uint8_t err = rx[PDU_FUNC_OFF] & 0x7F;
         printf("[ModbusTCP] server exception: 0x%02X\r\n", err);
         return false;
     }
 
     return true;
 }