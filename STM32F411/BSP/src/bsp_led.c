/**
  ******************************************************************************
  * @file    bsp_led.c
  * @brief   BSP LED implementation — PC13, active-low.
  * @note    Black Pill 板载 LED 位于 PC13，低电平点亮。
  ******************************************************************************
  */
#include "bsp_led.h"

/**
  * @brief  Initialize PC13 as push-pull output, no pull-up/down.
  */
void BSP_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOC clock */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Configure PC13 */
    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Default state: LED off (PC13 high) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

/**
  * @brief  Toggle PC13.
  */
void LED0_Toggle(void)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

/**
  * @brief  Turn LED on (PC13 low).
  */
void LED0_On(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
}

/**
  * @brief  Turn LED off (PC13 high).
  */
void LED0_Off(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}
