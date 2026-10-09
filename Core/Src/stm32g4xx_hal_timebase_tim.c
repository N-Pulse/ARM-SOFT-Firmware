/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g4xx_hal_timebase_tim.c
  * @brief   HAL time base based on the hardware TIM.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_tim.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* TIM6 is reserved for the HAL timebase, including before the scheduler. */
TIM_HandleTypeDef htim6;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  This function configures the TIM6 as a time base source.
  *         The time source is configured  to have 1ms time base with a dedicated
  *         Tick interrupt priority.
  * @note   This function is called  automatically at the beginning of program after
  *         reset by HAL_Init() or at any time when clock is configured, by HAL_RCC_ClockConfig().
  * @param  TickPriority: Tick interrupt priority.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef clkconfig;
  uint32_t latency;
  uint32_t timer_clock;
  HAL_StatusTypeDef status;

  /* This firmware requires a 1 ms HAL tick. Reject unsupported changes
   * instead of silently changing the units used by HAL timeouts. */
  if (TickPriority >= (1UL << __NVIC_PRIO_BITS) ||
      uwTickFreq != HAL_TICK_FREQ_1KHZ)
  {
    return HAL_ERROR;
  }

  __HAL_RCC_TIM6_CLK_ENABLE();
  HAL_RCC_GetClockConfig(&clkconfig, &latency);
  timer_clock = HAL_RCC_GetPCLK1Freq();
  if (clkconfig.APB1CLKDivider != RCC_HCLK_DIV1)
  {
    timer_clock *= 2U;
  }
  if (timer_clock < 1000000U || timer_clock % 1000000U != 0U)
  {
    return HAL_ERROR;
  }

  /* HAL_RCC_ClockConfig calls us again after changing the clock. Stop the
   * old timebase while reconfiguring it and discard the generated update. */
  HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
  htim6.Instance = TIM6;
  __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
  __HAL_TIM_DISABLE(&htim6);
  htim6.Init.Period = 999U;
  htim6.Init.Prescaler = timer_clock / 1000000U - 1U;
  htim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  status = HAL_TIM_Base_Init(&htim6);
  if (status != HAL_OK)
  {
    return status;
  }
  __HAL_TIM_SET_COUNTER(&htim6, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
  HAL_NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
  status = HAL_TIM_Base_Start_IT(&htim6);
  if (status == HAL_OK)
  {
    uwTickPrio = TickPriority;
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
  }
  return status;
}

/* No RTOS API is allowed here: TIM6 runs above the RTOS syscall ceiling so
 * HAL timeouts also progress during pre-scheduler RTOS object creation. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}

/**
  * @brief  Suspend Tick increment.
  * @note   Disable the tick increment by disabling TIM6 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_SuspendTick(void)
{
  /* Disable TIM6 update Interrupt */
  __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

/**
  * @brief  Resume Tick increment.
  * @note   Enable the tick increment by Enabling TIM6 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_ResumeTick(void)
{
  /* Enable TIM6 Update interrupt */
  __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}
