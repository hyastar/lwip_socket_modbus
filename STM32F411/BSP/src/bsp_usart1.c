/**
  ******************************************************************************
  * @file    bsp_usart1.c
  * @brief   BSP USART1 implementation — DMA circular RX + software ring buffer + idle interrupt.
  * @note    PA9=TX, PA10=RX, AF7, 115200 8N1.
  *          RX path:
  *            DMA (circular) → dma_raw_buf[256]
  *            Idle ISR       → copy to ring_buf[512], notify task
  *            Task           → USART1_ReadRingBuf() pulls from ring buffer
  *          TX path:
  *            BSP_USART1_SendBytes() → HAL_UART_Transmit_DMA (normal mode)
  ******************************************************************************
  */
#include "bsp_usart1.h"
#include "stm32f4xx_hal.h"
#include "sys.h"
#include <string.h>

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"

/*==========================================================================
 * Private Variables
 *========================================================================*/
static UART_HandleTypeDef huart1;
static DMA_HandleTypeDef  hdma_usart1_tx;
static DMA_HandleTypeDef  hdma_usart1_rx;

/* DMA circular raw receive buffer — DMA writes here continuously */
static uint8_t dma_raw_buf[USART1_DMA_BUF_SIZE];

/* Software ring buffer — idle ISR copies data here, tasks read from here */
static uint8_t  ring_buf[USART1_RING_BUF_SIZE];
static volatile uint16_t ring_buf_head = 0;  /* read pointer (task side) */
static volatile uint16_t ring_buf_tail = 0;  /* write pointer (ISR side) */
static volatile uint16_t dma_last_pos  = 0;  /* last DMA write position */

/* Task handle for notification — ISR calls vTaskNotifyGiveFromISR on this handle */
static TaskHandle_t usart1_rx_task_handle = NULL;

/*==========================================================================
 * Forward-declare ISR weak symbols (may also be defined in stm32f4xx_it.c)
 *========================================================================*/
void USART1_IRQHandler(void);
void DMA2_Stream7_IRQHandler(void);
void DMA2_Stream2_IRQHandler(void);

/*==========================================================================
 * Static helpers
 *========================================================================*/

/**
  * @brief  Calculate available bytes in ring buffer (ISR-safe, no lock needed
  *         because only ISR writes tail and task reads head).
  */
static inline uint16_t ring_buf_available(void)
{
    if (ring_buf_tail >= ring_buf_head)
    {
        return ring_buf_tail - ring_buf_head;
    }
    return USART1_RING_BUF_SIZE - ring_buf_head + ring_buf_tail;
}

/**
  * @brief  Copy data from dma_raw_buf segment [from, to) into ring buffer.
  *         Handles wrap-around of ring buffer tail.
  * @note   Must be called with interrupts disabled or from ISR context.
  */
static void copy_to_ringbuf(uint16_t from, uint16_t to)
{
    uint16_t len = to - from;

    /* Handle wrap-around of ring buffer */
    uint16_t space_to_end = USART1_RING_BUF_SIZE - ring_buf_tail;
    if (len <= space_to_end)
    {
        /* No wrap needed */
        memcpy(&ring_buf[ring_buf_tail], &dma_raw_buf[from], len);
        ring_buf_tail = (ring_buf_tail + len) % USART1_RING_BUF_SIZE;
    }
    else
    {
        /* Wrap: copy end piece first, then beginning piece */
        memcpy(&ring_buf[ring_buf_tail], &dma_raw_buf[from], space_to_end);
        memcpy(&ring_buf[0], &dma_raw_buf[from + space_to_end], len - space_to_end);
        ring_buf_tail = (ring_buf_tail + len) % USART1_RING_BUF_SIZE;
    }
}

/*==========================================================================
 * BSP_USART1_Init()
 *========================================================================*/
void BSP_USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. Clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* 2. GPIO — PA9=TX, PA10=RX, AF7 */
    GPIO_InitStruct.Pin       = USART1_TX_PIN | USART1_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(USART1_GPIO_PORT, &GPIO_InitStruct);

    /* 3. USART1 — 115200 8N1, TX+RX */
    huart1.Instance            = USART1;
    huart1.Init.BaudRate       = USART1_BAUDRATE;
    huart1.Init.WordLength     = UART_WORDLENGTH_8B;
    huart1.Init.StopBits       = UART_STOPBITS_1;
    huart1.Init.Parity         = UART_PARITY_NONE;
    huart1.Init.Mode           = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling   = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    /* 4. DMA TX — DMA2_Stream7_Channel4, memory→periph, normal mode */
    hdma_usart1_tx.Instance                = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel           = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction         = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc         = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc           = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode              = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority          = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    /* 5. DMA RX — DMA2_Stream2_Channel4, peripheral→memory, CIRCULAR */
    hdma_usart1_rx.Instance                = DMA2_Stream2;
    hdma_usart1_rx.Init.Channel           = DMA_CHANNEL_4;
    hdma_usart1_rx.Init.Direction         = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc         = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc           = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode              = DMA_CIRCULAR;
    hdma_usart1_rx.Init.Priority          = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    /* 6. NVIC — USART1 prio=6, DMA TX prio=7, DMA RX prio=7 */
    HAL_NVIC_SetPriority(USART1_IRQn,        6, 0);
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn,  7, 0);
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn,  7, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

    /* 7. Start DMA circular reception (no restart needed after idle fires) */
    HAL_UART_Receive_DMA(&huart1, dma_raw_buf, USART1_DMA_BUF_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

/*==========================================================================
 * ISRs
 *========================================================================*/

/**
  * @brief  USART1 interrupt handler.
  * @retval None
  * @note   Idle flag is checked BEFORE HAL_UART_IRQHandler (some HAL versions
  *         clear it inside the handler). Also verifies idle interrupt is enabled.
  *         On idle detection: copies DMA-written data into ring buffer and
  *         notifies the receive task. DMA keeps running in circular mode (no restart).
  */
void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Check idle flag BEFORE HAL_UART_IRQHandler — older HAL versions clear
     * the flag inside the handler, making the check unreliable afterward. */
    uint8_t idle_flag = __HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)
                     && __HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_IDLE);

    /* Let HAL process all other UART interrupt sources (TC, RXNE, errors, ...) */
    HAL_UART_IRQHandler(&huart1);

    if (idle_flag)
    {
        /* Clear idle flag */
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);

        /* Read current DMA write position.
         * DMA counter counts down from DMA_BUF_SIZE to 0.
         * cur_pos = 0 means DMA just wrapped to start. */
        uint16_t cur_pos = USART1_DMA_BUF_SIZE
                         - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);

        if (cur_pos > dma_last_pos)
        {
            /* Normal: DMA has not wrapped around */
            copy_to_ringbuf(dma_last_pos, cur_pos);
        }
        else if (cur_pos < dma_last_pos)
        {
            /* DMA wrapped: copy tail segment [dma_last_pos, DMA_BUF_SIZE) */
            copy_to_ringbuf(dma_last_pos, USART1_DMA_BUF_SIZE);
            /* then copy head segment [0, cur_pos) */
            copy_to_ringbuf(0, cur_pos);
        }
        /* cur_pos == dma_last_pos → no new data, do nothing */

        if (cur_pos != dma_last_pos)
        {
            /* Update tracking position */
            dma_last_pos = cur_pos;

            /* Wake the receiving task via FreeRTOS task notification */
            if (usart1_rx_task_handle != NULL)
            {
                vTaskNotifyGiveFromISR(usart1_rx_task_handle,
                                       &xHigherPriorityTaskWoken);
            }
        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
  * @brief  DMA2 Stream7 (USART1 TX) interrupt handler.
  */
void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

/**
  * @brief  DMA2 Stream2 (USART1 RX) interrupt handler.
  */
void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

/*==========================================================================
 * Public API
 *========================================================================*/

/**
  * @brief  Send bytes via DMA. Blocks until transfer completes.
  * @note   Must NOT be called from ISR.
  * @param  buf  source data pointer
  * @param  len  number of bytes to send
  */
void BSP_USART1_SendBytes(uint8_t *buf, uint16_t len)
{
    HAL_UART_Transmit_DMA(&huart1, buf, len);

    while (huart1.gState != HAL_UART_STATE_READY)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
  * @brief  Read available bytes from ring buffer (non-blocking).
  * @param  buf     destination buffer
  * @param  max_len max bytes to copy
  * @retval actual bytes copied; 0 if ring buffer is empty
  * @note   Safe to call from task context. Uses critical section to ensure
  *         consistent head/tail read.
  */
uint16_t USART1_ReadRingBuf(uint8_t *buf, uint16_t max_len)
{
    if (max_len == 0)
    {
        return 0;
    }

    taskENTER_CRITICAL();
    uint16_t available = ring_buf_available();
    uint16_t copy_len  = (available < max_len) ? available : max_len;

    if (copy_len == 0)
    {
        taskEXIT_CRITICAL();
        return 0;
    }

    /* Copy from head, handle wrap-around of ring buffer */
    uint16_t space_to_end = USART1_RING_BUF_SIZE - ring_buf_head;
    if (copy_len <= space_to_end)
    {
        memcpy(buf, &ring_buf[ring_buf_head], copy_len);
        ring_buf_head = (ring_buf_head + copy_len) % USART1_RING_BUF_SIZE;
    }
    else
    {
        memcpy(buf, &ring_buf[ring_buf_head], space_to_end);
        memcpy(&buf[space_to_end], &ring_buf[0], copy_len - space_to_end);
        ring_buf_head = (ring_buf_head + copy_len) % USART1_RING_BUF_SIZE;
    }
    taskEXIT_CRITICAL();

    return copy_len;
}

/**
  * @brief  Register the task handle so idle ISR can wake it via vTaskNotifyGiveFromISR().
  * @note   Call this after xTaskCreate() for the receiving task.
  */
void USART1_SetRxTaskHandle(TaskHandle_t handle)
{
    usart1_rx_task_handle = handle;
}

/*==========================================================================
 * 轻量串口打印 API（替代 printf，无浮点/malloc，栈消耗极小）
 *========================================================================*/
#include <stdarg.h>
#include <string.h>

/**
  * @brief  内部共用的轮询发送（调度器启动前/后均可用）
  * @note   对外暴露，echo_task 需要直接发送原始字节
  */
void usart1_write(const uint8_t *buf, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 200);
}

/**
  * @brief  发送字符串（不含格式化）
  */
void USART1_Print(const char *str)
{
    if (str == NULL) return;
    usart1_write((const uint8_t *)str, (uint16_t)strlen(str));
}

/**
  * @brief  格式化打印（轻量版，栈上固定缓冲区，最大128字节）
  * @note   不支持浮点 %f；用 %d %u %s %x 足够日常调试
  */
void USART1_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0)
    {
        usart1_write((const uint8_t *)buf, (uint16_t)len);
    }
}
