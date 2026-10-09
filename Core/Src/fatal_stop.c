#include "fatal_stop.h"
#include "stm32g4xx.h"

volatile uint32_t g_n_pulse_fatal_cause;

void N_PULSE_FatalCutoff(n_pulse_fatal_cause_t cause)
{
    __disable_irq();
    /* Fixed G474 peripheral addresses, NOT potentially uninitialized handles.
     * Existing SLEEP mapping: PG0/PG2/PG4 and PC4. Latch low before selecting
     * GPIO output mode; preserve every unrelated pin and clock bit. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOGEN | RCC_AHB2ENR_GPIOCEN;
    (void)RCC->AHB2ENR; /* RCC clock-enable readback (no polling). */
    __DSB();
    GPIOG->BSRR = ((1U << 0) | (1U << 2) | (1U << 4)) << 16;
    GPIOC->BSRR = (1U << 4) << 16;
    GPIOG->OTYPER &= ~((1U << 0) | (1U << 2) | (1U << 4));
    GPIOC->OTYPER &= ~(1U << 4);
    GPIOG->PUPDR &= ~((3U << 0) | (3U << 4) | (3U << 8));
    GPIOC->PUPDR &= ~(3U << 8);
    GPIOG->MODER = (GPIOG->MODER & ~((3U << 0) | (3U << 4) | (3U << 8))) |
                  (1U << 0) | (1U << 4) | (1U << 8);
    GPIOC->MODER = (GPIOC->MODER & ~(3U << 8)) | (1U << 8);
    __DSB();
    /* Do not access clock-gated/uninitialized timer peripherals. Output
     * disable enters HRTIM idle; TIM MOE/CCER gate both wrist input pairs. */
    if (RCC->APB2ENR & RCC_APB2ENR_HRTIM1EN) {
        HRTIM1->sCommonRegs.ODISR = 0x0FFFU;
        HRTIM1->sMasterRegs.MCR &= ~(HRTIM_MCR_TACEN | HRTIM_MCR_TBCEN |
            HRTIM_MCR_TCCEN | HRTIM_MCR_TDCEN | HRTIM_MCR_TECEN | HRTIM_MCR_TFCEN);
    }
    if (RCC->APB2ENR & RCC_APB2ENR_TIM15EN) {
        TIM15->BDTR &= ~TIM_BDTR_MOE;
        TIM15->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E);
        TIM15->CR1 &= ~TIM_CR1_CEN;
    }
    if (RCC->APB2ENR & RCC_APB2ENR_TIM16EN) {
        TIM16->BDTR &= ~TIM_BDTR_MOE;
        TIM16->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE);
        TIM16->CR1 &= ~TIM_CR1_CEN;
    }
    if (RCC->APB2ENR & RCC_APB2ENR_TIM17EN) {
        TIM17->BDTR &= ~TIM_BDTR_MOE;
        TIM17->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE);
        TIM17->CR1 &= ~TIM_CR1_CEN;
    }
    __DSB();
    g_n_pulse_fatal_cause = (uint32_t)cause;
}

void N_PULSE_FatalHalt(n_pulse_fatal_cause_t cause)
{
    N_PULSE_FatalCutoff(cause);
    for (;;) { __NOP(); }
}

void N_PULSE_FatalUnhandledIRQ(void)
{
    N_PULSE_FatalHalt(N_PULSE_FATAL_UNHANDLED_IRQ);
}
