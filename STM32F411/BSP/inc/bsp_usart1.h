/**
  ******************************************************************************
  * @file    bsp_usart1.h
  * @brief   BSP USART1 header — DMA circular RX + software ring buffer + idle interrupt.
  * @note    PA9=TX, PA10=RX, AF7, 115200 8N1.
  *          TX:  DMA2_Stream7_Channel4 (normal mode)
  *          RX:  DMA2_Stream2_Channel4 (circular mode)
  *          Idle interrupt detects end-of-frame.
  *          Ring buffer passes data to tasks via task notification.
  ******************************************************************************
  */
#ifndef __BSP_USART1_H
#define __BSP_USART1_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* Pin & GPIO */
#define USART1_TX_PIN      GPIO_PIN_9
#define USART1_RX_PIN      GPIO_PIN_10
#define USART1_GPIO_PORT   GPIOA

/* Baudrate */
#define USART1_BAUDRATE    115200

/* DMA raw buffer size (must match DMA transfer size) */
#define USART1_DMA_BUF_SIZE  256

/* Software ring buffer size (receive buffer for tasks) */
#define USART1_RING_BUF_SIZE 512

/* === Public API === */

/* Initialize USART1, GPIO, DMA, NVIC, ring buffer.
 * Call before vTaskStartScheduler(). */
void BSP_USART1_Init(void);

/* Send bytes via DMA. Blocks until DMA transfer completes. */
void BSP_USART1_SendBytes(uint8_t *buf, uint16_t len);

/* Read available bytes from ring buffer (non-blocking, returns immediately).
 * @param  buf     destination buffer
 * @param  max_len max bytes to read
 * @retval actual bytes copied into buf (0 if ring buffer is empty) */
uint16_t USART1_ReadRingBuf(uint8_t *buf, uint16_t max_len);

/* Register the receiving task handle so idle ISR can call vTaskNotifyGiveFromISR().
 * Call this after xTaskCreate() for the receiving task. */
void USART1_SetRxTaskHandle(TaskHandle_t handle);

/*==========================================================================
 * 轻量串口打印 API（替代 printf，无浮点/malloc，栈消耗极小）
 *========================================================================*/

/* 内部共用的轮询发送（调度器启动前/后均可用） */
void usart1_write(const uint8_t *buf, uint16_t len);

/* 发送字符串（不含格式化） */
void USART1_Print(const char *str);

/* 格式化打印（轻量版，栈上固定缓冲区，最大128字节） */
void USART1_Printf(const char *fmt, ...);

#endif /* __BSP_USART1_H */
