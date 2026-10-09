#include "motor_outputs.h"
#include <math.h>

/* RM0440 rev9 table232: DIV1 (CKPSC=5) compare minimum = 3.
 * Avoid close PER/CMP events at both extremes. This is a register constraint,
 * NOT an electrical commutation delay or a calibrated motor threshold. */
static uint32_t hrtim_compare(float duty, uint32_t period)
{
    if (duty >= 100.0f) return period + 1U; /* No compare match. */
    uint32_t ticks = (uint32_t)(duty * (float)period / 100.0f);
    if (ticks < 3U) return 3U; /* Source disabled separately for tiny commands. */
    return ticks > period - 3U ? period - 3U : ticks;
}

bool MotorOutputs_HRTIMPair(HRTIM_HandleTypeDef *h, uint32_t timer,
                           float duty1, float duty2, bool restore_enabled)
{
    HRTIM_TypeDef *r = h->Instance;
    HRTIM_Timerx_TypeDef *t = &r->sTimerxRegs[timer];
    const uint32_t outputs = 3U << (2U * timer);
    const uint32_t enable = HRTIM_MCR_TACEN << timer;
    const uint32_t update = HRTIM_CR2_TASWU << timer;
    const uint32_t inhibit = HRTIM_CR1_TAUDIS << timer;
    const uint32_t running = r->sMasterRegs.MCR & enable;
    const uint32_t enabled = restore_enabled ? (~r->sCommonRegs.ODSR & outputs) : 0U;
    r->sCommonRegs.ODISR = outputs; /* Hold BOTH inputs at configured idle LOW. */
    r->sMasterRegs.MCR &= ~enable;
    __DSB();
    const uint32_t period = t->PERxR;
    if ((t->TIMxCR & HRTIM_TIMCR_CK_PSC) != HRTIM_PRESCALERRATIO_DIV1 ||
        period < 6U || period > 0xFFFCU ||
        !isfinite(duty1) || !isfinite(duty2) || duty1 < 0.0f || duty1 > 100.0f ||
        duty2 < 0.0f || duty2 > 100.0f) return false;

    /* No automatic update during the multi-register preparation. PREEN stays
     * enabled; software update transfers the complete pair while held idle.
     * First remove old SET/RESET sources, then explicitly reset both latches.
     * SST/SRT are immediate, self-clearing, non-preloaded bits (RM0440). */
    t->TIMxCR |= HRTIM_TIMCR_PREEN;
    r->sCommonRegs.CR1 |= inhibit;
    t->SETx1R = 0U; t->SETx2R = 0U;
    t->RSTx1R = 0U; t->RSTx2R = 0U;
    r->sCommonRegs.CR1 &= ~inhibit;
    r->sCommonRegs.CR2 = update;
    t->RSTx1R = HRTIM_RST1R_SRT;
    t->RSTx2R = HRTIM_RST2R_SRT;
    __DSB();

    const bool active1 = duty1 * (float)period / 100.0f >= 3.0f;
    const bool active2 = duty2 * (float)period / 100.0f >= 3.0f;
    r->sCommonRegs.CR1 |= inhibit;
    __HAL_HRTIM_SETCOMPARE(h, timer, HRTIM_COMPAREUNIT_1, hrtim_compare(duty1, period));
    __HAL_HRTIM_SETCOMPARE(h, timer, HRTIM_COMPAREUNIT_2, hrtim_compare(duty2, period));
    t->SETx1R = active1 ? HRTIM_SET1R_PER : 0U;
    t->SETx2R = active2 ? HRTIM_SET2R_PER : 0U;
    t->RSTx1R = active1 && duty1 < 100.0f ? HRTIM_RST1R_CMP1 : 0U;
    t->RSTx2R = active2 && duty2 < 100.0f ? HRTIM_RST2R_CMP2 : 0U;
    r->sCommonRegs.CR1 &= ~inhibit;
    r->sCommonRegs.CR2 = update;
    /* Flat HIGH is explicit; zero has no SET source and was reset above. */
    if (duty1 >= 100.0f) t->SETx1R = HRTIM_SET1R_PER | HRTIM_SET1R_SST;
    if (duty2 >= 100.0f) t->SETx2R = HRTIM_SET2R_PER | HRTIM_SET2R_SST;
    t->CNTxR = 0U;
    __DSB();
    r->sMasterRegs.MCR |= running;
    if (enabled != 0U) r->sCommonRegs.OENR = enabled;
    return true;
}

static void tim_mode(TIM_TypeDef *t, uint32_t channel, uint32_t mode)
{
    /* CCPC=0: mode changes are immediate even with CCR preload enabled. */
    uint32_t shift = channel == TIM_CHANNEL_1 ? 0U : 8U;
    uint32_t mask = channel == TIM_CHANNEL_1 ? TIM_CCMR1_OC1M : TIM_CCMR1_OC2M;
    t->CCMR1 = (t->CCMR1 & ~mask) | (mode << shift);
}

bool MotorOutputs_TIMPair(TIM_HandleTypeDef *a, uint32_t channel_a,
                         TIM_HandleTypeDef *b, uint32_t channel_b,
                         float duty_a, float duty_b)
{
    TIM_TypeDef *ta = a->Instance, *tb = b->Instance;
    const uint32_t running_a = ta->CR1 & TIM_CR1_CEN;
    const uint32_t running_b = tb->CR1 & TIM_CR1_CEN;
    ta->CR1 &= ~TIM_CR1_CEN; tb->CR1 &= ~TIM_CR1_CEN;
    ta->CR2 &= ~TIM_CR2_CCPC; tb->CR2 &= ~TIM_CR2_CCPC;
    /* Both inactive BEFORE either input can become active (also TIM16/17).
     * No invented dead time; these are ordered bounded register writes. */
    tim_mode(ta, channel_a, TIM_OCMODE_FORCED_INACTIVE);
    tim_mode(tb, channel_b, TIM_OCMODE_FORCED_INACTIVE);
    __DSB();
    if (ta->ARR == 0U || ta->ARR > 65534U || tb->ARR == 0U || tb->ARR > 65534U ||
        !isfinite(duty_a) || !isfinite(duty_b) || duty_a < 0.0f || duty_a > 100.0f ||
        duty_b < 0.0f || duty_b > 100.0f)
        return false;
    uint32_t cr1_a = ta->CR1, cr1_b = tb->CR1;
    ta->CR1 &= ~TIM_CR1_UDIS; tb->CR1 &= ~TIM_CR1_UDIS;
    /* ARR+1 is the up-counter cycle length; CCR preload is transferred by UG
     * while stopped and forced inactive. No CCER/MOE enable is written here. */
    uint32_t preload_a = channel_a == TIM_CHANNEL_1 ? TIM_CCMR1_OC1PE : TIM_CCMR1_OC2PE;
    uint32_t preload_b = channel_b == TIM_CHANNEL_1 ? TIM_CCMR1_OC1PE : TIM_CCMR1_OC2PE;
    ta->CCMR1 |= preload_a; tb->CCMR1 |= preload_b;
    __HAL_TIM_SET_COMPARE(a, channel_a, (uint32_t)(duty_a * (float)(ta->ARR + 1U) / 100.0f));
    __HAL_TIM_SET_COMPARE(b, channel_b, (uint32_t)(duty_b * (float)(tb->ARR + 1U) / 100.0f));
    ta->EGR = TIM_EGR_UG;
    if (tb != ta) tb->EGR = TIM_EGR_UG;
    ta->CR1 = cr1_a; tb->CR1 = cr1_b;
    tim_mode(ta, channel_a, duty_a >= 100.0f ? TIM_OCMODE_FORCED_ACTIVE :
             duty_a <= 0.0f ? TIM_OCMODE_FORCED_INACTIVE : TIM_OCMODE_PWM1);
    tim_mode(tb, channel_b, duty_b >= 100.0f ? TIM_OCMODE_FORCED_ACTIVE :
             duty_b <= 0.0f ? TIM_OCMODE_FORCED_INACTIVE : TIM_OCMODE_PWM1);
    __DSB();
    ta->CR1 |= running_a; tb->CR1 |= running_b;
    return true;
}
