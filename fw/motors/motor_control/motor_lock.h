#ifndef MOTOR_LOCK_H
#define MOTOR_LOCK_H

#include "stm32g4xx_hal.h"
#include "old_hw_timing.h"

/* One serialization domain for task/ISR state and output register writes.
 * Preserve PRIMASK for nesting and pre-scheduler use. No waiting or blocking HAL operation is allowed while held. UART4
 * IT commitment is the audited bounded exception (pointer/state/IRQ writes). TIM6 is delayed only briefly. */
static inline uint32_t Motor_Lock(void)
{
    uint32_t previous = __get_PRIMASK();
    __disable_irq();
#if N_PULSE_TIMING_DIAGNOSTICS
    OldHW_TimingLockBegin(previous);
#endif
    return previous;
}

static inline void Motor_Unlock(uint32_t previous)
{
#if N_PULSE_TIMING_DIAGNOSTICS
    OldHW_TimingLockEnd(previous);
#endif
    __set_PRIMASK(previous);
}

#endif
