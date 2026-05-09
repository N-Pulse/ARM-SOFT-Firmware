/*
 * analog.c
 *
 *  Created on: Apr 28, 2026
 *      Author: cleme
 */
#include "drivers.h"

int nb_motors =0;

void Motor_Init(Motor_t         *self,
	    			TIM_HandleTypeDef  *pwm_a_timer,
	    			TIM_HandleTypeDef  *pwm_b_timer,
					int32_t             pwm_a_channel,
					int32_t             pwm_b_channel){
	self->_pwm_a_timer =pwm_a_timer;
	self->_pwm_b_timer =pwm_b_timer;
	self->_pwm_a_channel =pwm_a_channel;
	self->_pwm_b_channel =pwm_b_channel;

	self->index = nb_motors++;

	HAL_TIM_PWM_Start(pwm_a_timer, pwm_a_channel);
	__HAL_TIM_SET_COMPARE(pwm_a_timer, pwm_a_channel, 0);

	HAL_TIM_PWM_Start(pwm_b_timer, pwm_b_channel);
	__HAL_TIM_SET_COMPARE(pwm_b_timer, pwm_b_channel, 0);
}

void Set_speed(Motor_t *self, float pwmDuty){

	 //Apply to Timer (Handle direction if pwmDuty is negative)
	 if (pwmDuty >= 0) {
		__HAL_TIM_SET_COMPARE(self->_pwm_a_timer, self->_pwm_a_channel, (uint32_t)pwmDuty);
		__HAL_TIM_SET_COMPARE(self->_pwm_b_timer, self->_pwm_b_channel, 0);
	 } else {
		__HAL_TIM_SET_COMPARE(self->_pwm_a_timer, self->_pwm_a_channel, 0);
		__HAL_TIM_SET_COMPARE(self->_pwm_b_timer, self->_pwm_b_channel, (uint32_t)(-pwmDuty));
	 }
}
