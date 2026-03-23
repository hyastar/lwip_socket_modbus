#ifndef __FREERTOS_TASKS_H
#define __FREERTOS_TASKS_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* 任务启动函数声明 */
void freertos_start(void);

/* 栈溢出钩子（configCHECK_FOR_STACK_OVERFLOW = 2 时必须提供） */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

#endif /* __FREERTOS_TASKS_H */
