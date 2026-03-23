#ifndef __SYS_H
#define __SYS_H

#include "stm32f4xx_hal.h"

/*
 * 函数声明
 * 将这些函数暴露给其他 .c 文件（例如 main.c）
 */
void SystemClock_Config(void);
void Error_Handler(void);

#endif /* __SYS_H */
