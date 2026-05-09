/*
 * analog.h
 *
 *  Created on: Apr 28, 2026
 *      Author: cleme
 */
#ifndef SRC_DRIVERS_H_
#define SRC_DRIVERS_H_

#include "motor_backend.h"
#include "stm32g4xx_hal.h"

extern const int ARR;
extern const int reset_timeout_ms;

typedef struct {
    // ── Private variables
    TIM_HandleTypeDef  *_pwm_a_timer;
    TIM_HandleTypeDef  *_pwm_b_timer;
    int32_t             _pwm_a_channel;
    int32_t             _pwm_b_channel;

    uint8_t				index;
} Motor_t;

void Motor_Init(Motor_t         *self,
	    			TIM_HandleTypeDef  *pwm_a_timer,
	    			TIM_HandleTypeDef  *pwm_b_timer,
					int32_t             pwm_a_channel,
					int32_t             pwm_b_channel);

void Set_speed(Motor_t *self, float pwmDuty);

#endif /* SRC_DRIVERS_H_ */
