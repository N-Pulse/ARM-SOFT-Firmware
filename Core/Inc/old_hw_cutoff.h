#ifndef N_PULSE_OLD_HW_CUTOFF_H
#define N_PULSE_OLD_HW_CUTOFF_H
#include "stm32g4xx.h"

/* No HAL, handles, role, RTOS, polling or dependency on successful startup.
 * ENA pins are the union of both historical boards. Board validation pending. */
static inline void OldHW_CutoffRegisters(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;
    __DSB();
    GPIOA->BSRR = 0x0F00U << 16; /* TIM1 CH1..4, slave ENA */
    GPIOB->BSRR = 0x03C0U << 16; /* TIM4 CH1..4, master + palm ENA */
    __DSB();
    GPIOA->OTYPER &= ~0x0F00U;
    GPIOB->OTYPER &= ~0x03C0U;
    GPIOA->PUPDR &= ~0x00FF0000U;
    GPIOB->PUPDR &= ~0x000FF000U;
    GPIOA->MODER = (GPIOA->MODER & ~0x00FF0000U) | 0x00550000U;
    GPIOB->MODER = (GPIOB->MODER & ~0x000FF000U) | 0x00055000U;
    if (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        TIM1->BDTR &= ~(TIM_BDTR_MOE | TIM_BDTR_AOE);
        TIM1->CCER = 0U;
        TIM1->CR1 &= ~TIM_CR1_CEN;
        TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = TIM1->CCR4 = 0U;
    }
    if (RCC->APB1ENR1 & RCC_APB1ENR1_TIM4EN) {
        TIM4->CCER = 0U;
        TIM4->CR1 &= ~TIM_CR1_CEN;
        TIM4->CCR1 = TIM4->CCR2 = TIM4->CCR3 = TIM4->CCR4 = 0U;
    }
    __DSB();
}
#endif
