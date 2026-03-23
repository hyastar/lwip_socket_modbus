#include "sys.h"

/**
  * @brief 系统时钟配置
  * @note  使用 25MHz HSE, 配置系统时钟 (SYSCLK) 为 100MHz (F411系列最高).
  * - PLLM = 25, PLLN = 200, PLLP = 2
  * - HCLK = 100MHz
  * - PCLK1 = 50MHz (APB1 最大 50MHz)
  * - PCLK2 = 100MHz (APB2 最大 100MHz)
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 启用电源时钟, 配置稳压器 */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* 初始化振荡器 (HSE + PLL) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;    // 修改点: 25MHz / 25 = 1MHz
    RCC_OscInitStruct.PLL.PLLN = 200;   // 修改点: 1MHz * 200 = 200MHz (VCO)
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; // 200MHz / 2 = 100MHz (SYSCLK)
    RCC_OscInitStruct.PLL.PLLQ = 4;     // (此配置下 PLLQ = 200/4 = 50MHz, USB/SDIO不可用)
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* 初始化总线时钟 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;  // HCLK = 100MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; // PCLK1 = 100MHz / 2 = 50MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; // PCLK2 = 100MHz / 1 = 100MHz

    /* 配置Flash等待周期 (100MHz, 3.3V, 需要 3 WS) */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief  HAL库配置错误时的处理函数.
  * @retval None
  */
void Error_Handler(void)
{
    /* 关闭中断并进入死循环 */
    __disable_irq();
    while (1)
    {
    }
}

