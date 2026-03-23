#include "freertos_tasks.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#include "user_config.h"
#include "bsp_led.h"
#include "bsp_usart1.h"

/* === 任务句柄 === */
static TaskHandle_t echo_task_handle = NULL;

/* === 任务定义 === */

/* 启动任务 */
#define START_TASK_STACK         512
#define START_TASK_PRIORITY      3
static void start_task(void *pvParameters);

/* 串口回显任务 */
#define ECHO_TASK_STACK           384
#define ECHO_TASK_PRIORITY        2
static void echo_task(void *pvParameters);

/* LED 任务  */
#define LED_TASK_STACK           384
#define LED_TASK_PRIORITY        1
void led_task(void *pvParameters);



/**
  * @brief  启动任务
  */
static void start_task(void *pvParameters)
{
    (void)pvParameters;

    taskENTER_CRITICAL();

    /* 创建 LED 任务 */
    xTaskCreate(led_task, "led_task", LED_TASK_STACK, NULL, LED_TASK_PRIORITY, NULL);

    /* 创建 Echo 任务 */
    xTaskCreate(echo_task, "echo_task", ECHO_TASK_STACK, NULL, ECHO_TASK_PRIORITY, &echo_task_handle);

    /* 注册任务句柄，使能空闲中断唤醒 */
    USART1_SetRxTaskHandle(echo_task_handle);

    taskEXIT_CRITICAL();

    USART1_Print("Start Task: all tasks created.\r\n");
    vTaskDelete(NULL);
}


/**
  * @brief  FreeRTOS 启动入口
  */
void freertos_start(void)
{
    xTaskCreate(start_task, "start_task", START_TASK_STACK, NULL, START_TASK_PRIORITY, NULL);
    vTaskStartScheduler();
}


/**
  * @brief  LED 闪烁任务
  * @note   每 50000ms 翻转一次 LED，验证 FreeRTOS 调度器正常工作。
  */
void led_task(void *pvParameters)
{
    (void)pvParameters;

    USART1_Print("[LED Task] Task is running.\r\n");

    while (1)
    {
        LED0_Toggle();
        USART1_Print("LED Toggled\r\n");
        vTaskDelay(pdMS_TO_TICKS(50000));
    }
}


/**
  * @brief 串口回显任务 (演示 DMA 循环接收 + 空闲中断 + 环形缓冲区)
  */
static void echo_task(void *pvParameters)
{
    uint8_t rx_buf[64];
    uint16_t read_len;

    (void)pvParameters;

    USART1_Print("[Echo Task] Task is running. Waiting for data...\r\n");

    while (1)
    {
        /* 1. 阻塞等待空闲中断唤醒（中断驱动，真正零 CPU 占用） */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        /* 2. 不断从环形缓冲区读出所有数据直到空 */
        while ((read_len = USART1_ReadRingBuf(rx_buf, 64)) > 0)
        {
            USART1_Printf("\r\n[Echo Task] Received %u bytes: [", read_len);
            usart1_write(rx_buf, read_len);   // 直接发送原始字节，无需逐字节 putchar
            USART1_Print("]\r\n");
        }
    }
}


/**
  * @brief  栈溢出钩子函数（configCHECK_FOR_STACK_OVERFLOW = 2 时必须提供）
  * @param  xTask: 溢出的任务句柄
  * @param  pcTaskName: 溢出的任务名称字符串
  * @retval None
  */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    portDISABLE_INTERRUPTS();
    while (1)
    {
        __NOP();
    }
}
