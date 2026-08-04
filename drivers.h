/*
 * analog.h
 *
 *  Created on: Apr 28, 2026
 *      Author: cleme
 */
#ifndef SRC_DRIVERS_H_
#define SRC_DRIVERS_H_

#include "main.h"

extern HRTIM_HandleTypeDef hhrtim1;

typedef struct {
    // ── Private variables
    TIM_HandleTypeDef  *_pwm_a_timer;
    TIM_HandleTypeDef  *_pwm_b_timer;
    uint32_t 			_hr_timer;
    uint32_t             _pwm_a_channel;
    uint32_t             _pwm_b_channel;
    uint8_t				_type;
} Motor_t;

void Motor_Init_normal(Motor_t         *self,
	    			TIM_HandleTypeDef  *pwm_a_timer,
	    			TIM_HandleTypeDef  *pwm_b_timer,
					uint32_t             pwm_a_channel,
					uint32_t             pwm_b_channel);

void Motor_Init_HR(Motor_t		*self,
					uint32_t 	hr_timer,
					uint32_t 	output_mask);

void Set_speed(Motor_t *self, float pwmDuty);

#endif /* SRC_DRIVERS_H_ */
