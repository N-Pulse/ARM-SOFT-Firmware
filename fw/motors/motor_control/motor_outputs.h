#ifndef MOTOR_OUTPUTS_H
#define MOTOR_OUTPUTS_H
#include <stdbool.h>
#include "stm32g4xx_hal.h"
/* Bounded register updates. Caller holds Motor_Lock (or owns startup).
 * No function starts a stopped counter or enables a previously disabled output.
 * Logical HIGH/LOW assume the existing active-high, independent input mapping. */
bool MotorOutputs_HRTIMPair(HRTIM_HandleTypeDef *h, uint32_t timer,
                           float duty1, float duty2, bool restore_enabled);
bool MotorOutputs_TIMPair(TIM_HandleTypeDef *a, uint32_t channel_a,
                         TIM_HandleTypeDef *b, uint32_t channel_b,
                         float duty_a, float duty_b);
#endif
