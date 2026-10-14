#if N_PULSE_OLD_HW && !defined(MOTOR_BACKEND_SIM)
#include "thumb_encoder.h"
#include "quadrature_decoder.h"
#include "main.h"
#include "board_link.h"
#include "motor_lock.h"
#define THUMB_LINES (GPIO_PIN_14 | GPIO_PIN_15)
static quadrature_decoder_t decoder;
static bool initialized;
volatile uint32_t g_thumb_encoder_invalid_transitions;
static uint8_t levels(void)
{
    uint32_t pins = GPIOB->IDR; /* One simultaneous snapshot of A and B. */
    return (uint8_t)(((pins >> 13) & 2U) | ((pins >> 15) & 1U));
}
static bool configured(void)
{
    return (EXTI->IMR1 & THUMB_LINES) == THUMB_LINES &&
           (EXTI->RTSR1 & THUMB_LINES) == THUMB_LINES &&
           (EXTI->FTSR1 & THUMB_LINES) == THUMB_LINES &&
           (SYSCFG->EXTICR[3] & 0xFF00U) == 0x1100U &&
           (GPIOB->MODER & 0xF0000000U) == 0U;
}
bool ThumbEncoder_CanResetLocked(void)
{ return configured() && (!initialized || NVIC_GetEnableIRQ(EXTI15_10_IRQn)); }
bool ThumbEncoder_ResetLocked(void)
{
    if (!ThumbEncoder_CanResetLocked()) return false;
    /* Establish a new explicitly requested raw origin, never a mechanical zero.
     * Only our lines are cleared. Shared EXTI13/button state is untouched. */
    EXTI->PR1 = THUMB_LINES;
    Quadrature_Init(&decoder, levels());
    g_thumb_encoder_invalid_transitions = 0;
    if (configured()) g_old_hw_encoder_unavailable_mask &= ~(1U << MOTOR_THUMB);
    return true;
}
bool ThumbEncoder_Init(void)
{
    if (initialized) return configured();
    if (BoardLink_Role() != BOARD_ROLE_SLAVE) return false;
    uint32_t key = Motor_Lock();
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_SYSCFG_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = THUMB_LINES; g.Mode = GPIO_MODE_IT_RISING_FALLING;
    g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB,&g);
    ThumbEncoder_ResetLocked();
    initialized = configured();
    /* Above RTOS ceiling: ISR contains no RTOS API and only bounded work. */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn,3,0);
    if (initialized) HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    else EXTI->IMR1 &= ~THUMB_LINES;
    Motor_Unlock(key);
    return initialized;
}
bool ThumbEncoder_HealthyLocked(void)
{ return initialized && !decoder.fault && configured() && NVIC_GetEnableIRQ(EXTI15_10_IRQn); }
bool ThumbEncoder_ReadLocked(int64_t *count)
{
    if (!count || !ThumbEncoder_HealthyLocked()) return false;
    *count = decoder.count;
    return true;
}
void ThumbEncoder_IRQHandler(void)
{
    if (!initialized) return; /* Do not take ownership of another role's shared EXTI. */
    uint32_t pending = EXTI->PR1 & THUMB_LINES;
    if (!pending) return;
#if N_PULSE_TIMING_DIAGNOSTICS
    uint32_t timing=OldHW_TimingBegin(OLD_TIME_THUMB);
#endif
    EXTI->PR1 = pending; /* W1C BEFORE IDR read; new edges remain pending. */
    uint32_t key = Motor_Lock();
    uint8_t bits = (uint8_t)(((pending >> 13) & 2U) | ((pending >> 15) & 1U));
    bool valid = Quadrature_Edge(&decoder,levels(),bits);
    g_thumb_encoder_invalid_transitions = decoder.invalid;
    if (!valid) {
        g_old_hw_encoder_unavailable_mask |= 1U << MOTOR_THUMB;
        Motor_InvalidatePositionFeedback(MOTOR_THUMB); /* Immediate STOP if enabled; no RTOS. */
    }
    Motor_Unlock(key);
#if N_PULSE_TIMING_DIAGNOSTICS
    OldHW_TimingEnd(OLD_TIME_THUMB,timing);
#endif
}
#endif
