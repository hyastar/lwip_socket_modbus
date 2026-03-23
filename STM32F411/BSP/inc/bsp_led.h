/**
  ******************************************************************************
  * @file    bsp_led.h
  * @brief   BSP LED header — PC13, active-low (Black Pill on-board LED).
  ******************************************************************************
  */
#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f4xx_hal.h"

void BSP_LED_Init(void);
void LED0_Toggle(void);
void LED0_On(void);
void LED0_Off(void);

#endif /* __BSP_LED_H */
