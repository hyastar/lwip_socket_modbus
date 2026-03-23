/**
  ******************************************************************************
  * @file    bsp_timebase.h
  * @brief   BSP Timebase header — TIM5 provides 1ms tick for HAL library.
  * @note    F411 has no TIM6; TIM5 is used instead. TIM5_IRQHandler is the
  *          interrupt entry. TIM5 clock = PCLK1×2 = 100MHz.
  *          PSC=99, ARR=999 → 1ms period.
  *          SysTick is reserved for FreeRTOS scheduler.
  ******************************************************************************
  */
#ifndef __BSP_TIMEBASE_H
#define __BSP_TIMEBASE_H

#include <stdint.h>

void BSP_TimBase_Init(void);
void BSP_TimBase_RunStats_Init(void);

#endif /* __BSP_TIMEBASE_H */
