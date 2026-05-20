/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "stm32g4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */
  /* PB14 (line 14) + PB15 (line 15) : encodeur software THUMB sur slave.
   * Le bouton USER (PC13, line 13) reste gere par BSP_PB_IRQHandler ci-dessous. */
  extern void Thumb_SwEnc_IRQ(void);
  if (EXTI->PR1 & ((1UL << 14) | (1UL << 15))) {
    Thumb_SwEnc_IRQ();
  }
  /* USER CODE END EXTI15_10_IRQn 0 */
  BSP_PB_IRQHandler(BUTTON_USER);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt, DAC1 and DAC3 channel underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/**
  * @brief SysTick is used exclusively by FreeRTOS (HAL timebase is TIM6).
  */
void SysTick_Handler(void)
{
  xPortSysTickHandler();
}

/* USER CODE BEGIN 1 */
/* RX ring buffer fed by LPUART1_IRQHandler, drained by Comms_Task.
 * 256 bytes is enough for a few back-to-back frames at 115200 baud. */
#define COMMS_RXRING_SIZE  256U
volatile uint8_t  g_rxring[COMMS_RXRING_SIZE];
volatile uint16_t g_rxring_head = 0;  /* written by ISR */
volatile uint16_t g_rxring_tail = 0;  /* read by task */

void LPUART1_IRQHandler(void)
{
    USART_TypeDef *u = LPUART1;
    /* Clear sticky error flags so the IRQ can keep firing. */
    if (u->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    }
    while (u->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)u->RDR;
        uint16_t next = (uint16_t)((g_rxring_head + 1U) % COMMS_RXRING_SIZE);
        if (next != g_rxring_tail) {  /* drop on overflow rather than corrupt */
            g_rxring[g_rxring_head] = b;
            g_rxring_head = next;
        }
    }
}

/* RX ring buffer for USART3 — board-to-board link drained by BoardLink_RxTask
 * on the slave side. Same pattern as LPUART1. */
#define USART3_RXRING_SIZE  256U
volatile uint8_t  g_usart3_rxring[USART3_RXRING_SIZE];
volatile uint16_t g_usart3_rxring_head = 0;
volatile uint16_t g_usart3_rxring_tail = 0;

void USART3_IRQHandler(void)
{
    USART_TypeDef *u = USART3;
    if (u->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    }
    while (u->ISR & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)u->RDR;
        uint16_t next = (uint16_t)((g_usart3_rxring_head + 1U)
                                   % USART3_RXRING_SIZE);
        if (next != g_usart3_rxring_tail) {
            g_usart3_rxring[g_usart3_rxring_head] = b;
            g_usart3_rxring_head = next;
        }
    }
}
/* USER CODE END 1 */
