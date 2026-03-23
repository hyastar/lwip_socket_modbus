/**
  ******************************************************************************
  * @file    bsp_timebase.c
  * @brief   BSP Timebase implementation — TIM5 provides 1ms tick for HAL.
  * @note    F411 has no TIM6; TIM5 is used instead.
  *
  *          Clock analysis:
  *            SYSCLK = 100MHz, PCLK1 = 50MHz (APB1 divider=2)
  *            APB1 timer clock = PCLK1 × 2 = 100MHz  (timer clocks are doubled
  *            when APB prescaler != 1, per RM p.96)
  *            TIM5 clock = 100MHz
  *            PSC = 99   → div by 100 → 1MHz
  *            ARR = 999  → div by 1000 → 1kHz = 1ms period
  *
  *          SysTick is now exclusively owned by FreeRTOS. HAL_GetTick() and
  *          HAL_Delay() are driven by TIM5 via HAL_IncTick().
  *
  *          HAL_InitTick() is reimplemented here to override the __weak__
  *          version in stm32f4xx_hal.c so that HAL_Init() uses TIM5 instead
  *          of SysTick.
  ******************************************************************************
  */
#include "bsp_timebase.h"
#include "stm32f4xx_hal.h"

/* Private macro ------------------------------------------------------------*/
#define TIMEBASE_TICK_FREQ_HZ   1000U   /* 1000 Hz = 1ms period */

/* Private variables ---------------------------------------------------------*/
/* TIM5 handle must be accessible from ISR and callback — defined at file scope */
static TIM_HandleTypeDef htim5;
/* TIM2 handle for run-time stats — defined at file scope to avoid dangling pointer */
static TIM_HandleTypeDef htim2;

/* Private function prototypes -----------------------------------------------*/
static void TIM5_NVIC_Config(void);

/**
  * @brief  TIM5 interrupt priority and enable configuration.
  * @retval None
  */
static void TIM5_NVIC_Config(void)
{
    /* Configure NVIC — priority 15 (lowest), lower than FreeRTOS interrupts */
    HAL_NVIC_SetPriority(TIM5_IRQn, 15U, 0U);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
}

/**
  * @brief  Initialize TIM5 as HAL timebase (1ms tick).
  * @note   Call this BEFORE vTaskStartScheduler().
  *         HAL_Init() internally calls HAL_InitTick() which invokes this
  *         function, so explicit call is only needed for standalone use.
  * @retval None
  */
void BSP_TimBase_Init(void)
{
    /* 1. Enable TIM5 clock — TIM5 is on APB1 bus */
    __HAL_RCC_TIM5_CLK_ENABLE();

    /* 2. TIM5 base configuration
     *    Clock source: PCLK1×2 = 100MHz
     *    PSC = 99    → counter increments at 100MHz / 100 = 1MHz
     *    ARR = 999  → period = 1000 counts → 1ms
     */
    htim5.Instance               = TIM5;
    htim5.Init.Prescaler         = 99U;           /* 100MHz / 100 = 1MHz */
    htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim5.Init.Period            = 999U;           /* 1MHz / 1000 = 1kHz */
    htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
    {
        /* Initialization error — halt */
        while (1) { __NOP(); }
    }

    /* 3. Configure NVIC */
    TIM5_NVIC_Config();

    /* 4. Start TIM5 counter in interrupt mode */
    if (HAL_TIM_Base_Start_IT(&htim5) != HAL_OK)
    {
        while (1) { __NOP(); }
    }
}

/**
  * @brief  TIM5 interrupt service routine.
  * @retval None
  */
void TIM5_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim5);
}

/**
  * @brief  Period elapsed callback — called by HAL_TIM_IRQHandler on Update.
  * @param  htim: pointer to TIM_HandleTypeDef
  * @retval None
  * @note   Overrides the weak __weak version in stm32f4xx_hal_tim.c
  *         Only TIM5 triggers HAL_IncTick() here.
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5)
    {
        HAL_IncTick();
    }
}

/**
  * @brief  Reimplement HAL_InitTick to use TIM5 instead of SysTick.
  * @param  TickPriority: tick interrupt priority (not used for TIM5 here)
  * @retval HAL status
  * @note   This overrides the __weak HAL_InitTick() defined in stm32f4xx_hal.c.
  *         HAL_Init() calls this function; by providing our own version,
  *         HAL_Init() will use TIM5 for its tick instead of SysTick.
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    (void)TickPriority;   /* unused — TIM5 priority is hardcoded to 15 in BSP_TimBase_Init */

    BSP_TimBase_Init();
    return HAL_OK;
}


/*==================================================================================================
 * TIM2 — 高精度运行时间计数器 (用于 configGENERATE_RUN_TIME_STATS)
 *==================================================================================================
 *  Clock: APB1 timer clock = PCLK1 × 2 = 100MHz
 *  PSC = 99 → counter increments at 1MHz (1us per tick)
 *  ARR = 0xFFFFFFFF → ~71 minutes before overflow
 *  No interrupt needed — FreeRTOS polls TIM2->CNT directly via portGET_RUN_TIME_COUNTER_VALUE
 *================================================================================================*/

/**
  * @brief  Initialize TIM2 as free-running counter for run-time stats.
  * @retval None
  * @note   TIM2 counts at 1MHz (1us per count). ARR=0xFFFFFFFF gives ~71min range.
  *         portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() calls this; main.c may call it early too.
  */
void BSP_TimBase_RunStats_Init(void)
{
    /* TIM2 is on APB1 bus — clock is already enabled by BSP_TimBase_Init via PCLK1 */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Configure: no interrupt, free-running, 1us period
     *   PSC = 99   → 100MHz / 100 = 1MHz
     *   ARR = 0xFFFFFFFF → 32-bit max, ~71 min before overflow
     *   No NVIC enable — polled directly via TIM2->CNT
     */
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 99U;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFFU;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        while (1) { __NOP(); }
    }

    /* Start counter in polling mode (no interrupt) */
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        while (1) { __NOP(); }
    }
}
