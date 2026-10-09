#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fatal_stop.h"
#include "stm32g4xx.h"
GPIO_TypeDef mock_gpio[3];
RCC_TypeDef mock_rcc;
HRTIM_TypeDef *fatal_hrtim;
TIM_TypeDef *fatal_tim15, *fatal_tim16, *fatal_tim17;
static uint32_t masked;
static unsigned barriers;
void __disable_irq(void) { masked = 1U; }
void __NOP(void) { }
void __DSB(void)
{
    ++barriers;
    /* Apply BSRR write semantics; observe ordering before GPIO mode selection. */
    for (unsigned i = 0; i < 2; ++i) {
        mock_gpio[i].pins &= ~(mock_gpio[i].BSRR >> 16);
        mock_gpio[i].BSRR = 0;
    }
}
static void check_sleep(void)
{
    assert(masked == 1U);
    assert((mock_rcc.AHB2ENR & 0x44U) == 0x44U);
    assert((mock_gpio[0].pins & 0x15U) == 0U && (mock_gpio[1].pins & 0x10U) == 0U);
    assert((mock_gpio[0].MODER & 0x333U) == 0x111U);
    assert((mock_gpio[1].MODER & 0x300U) == 0x100U);
    assert((mock_gpio[0].OTYPER & 0x15U) == 0 && (mock_gpio[1].OTYPER & 0x10U) == 0);
    assert((mock_gpio[0].PUPDR & 0x333U) == 0 && (mock_gpio[1].PUPDR & 0x300U) == 0);
}
int main(void)
{
    /* Before clocks/handles: NULL timer pointers crash if accidentally accessed. */
    memset(mock_gpio, 0xFF, sizeof(mock_gpio));
    for (unsigned i=0;i<3;++i) mock_gpio[i].BSRR=0U; /* Write-only command register. */
    mock_rcc.AHB2ENR = 0x80000000U;
    N_PULSE_FatalCutoff(N_PULSE_FATAL_ERROR);
    check_sleep(); assert(mock_rcc.AHB2ENR == 0x80000044U);
    assert(g_n_pulse_fatal_cause == N_PULSE_FATAL_ERROR && barriers == 3);
    assert((mock_gpio[0].MODER & ~0x333U) == ~0x333U);
    assert((mock_gpio[1].pins & ~0x10U) == ~0x10U);
    puts("PASS fatal before peripheral initialization; unrelated pins/clocks preserved");

    HRTIM_TypeDef h = {0}; TIM_TypeDef t[3]; memset(t,0xFF,sizeof(t));
    fatal_hrtim=&h; fatal_tim15=&t[0]; fatal_tim16=&t[1]; fatal_tim17=&t[2];
    mock_rcc.APB2ENR = RCC_APB2ENR_HRTIM1EN | RCC_APB2ENR_TIM15EN |
        RCC_APB2ENR_TIM16EN | RCC_APB2ENR_TIM17EN;
    h.sMasterRegs.MCR = 0xFFFFFFFFU;
    masked = 1U; /* Already masked, never re-enabled. */
    N_PULSE_FatalCutoff(N_PULSE_FATAL_HARDFAULT);
    check_sleep(); assert(h.sCommonRegs.ODISR == 0xFFFU);
    assert((h.sMasterRegs.MCR & (0x3FU << 17)) == 0U);
    assert((h.sMasterRegs.MCR & ~(0x3FU << 17)) == ~(0x3FU << 17));
    for (unsigned i=0;i<3;++i) {
        assert(!(t[i].BDTR & TIM_BDTR_MOE)); assert(!(t[i].CR1 & TIM_CR1_CEN));
        assert(!(t[i].CCER & (TIM_CCER_CC1E | TIM_CCER_CC1NE)));
    }
    assert(!(t[0].CCER & TIM_CCER_CC2E));
    assert(g_n_pulse_fatal_cause == N_PULSE_FATAL_HARDFAULT);
    puts("PASS fatal active HRTIM and both wrists; interrupts already masked");

    /* Individually gated peripherals stay untouched; no unconditional access. */
    fatal_hrtim=NULL; fatal_tim15=NULL; fatal_tim17=NULL;
    mock_rcc.APB2ENR = RCC_APB2ENR_TIM16EN;
    for (unsigned cause=1;cause<=N_PULSE_FATAL_UNHANDLED_IRQ;++cause) {
        N_PULSE_FatalCutoff((n_pulse_fatal_cause_t)cause);
        assert(g_n_pulse_fatal_cause == cause); check_sleep();
    }
    puts("PASS fatal repeatability, each diagnostic cause, partial initialization");
    return 0;
}
