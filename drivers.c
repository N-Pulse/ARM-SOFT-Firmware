/*
 * analog.c
 *
 *  Created on: Apr 28, 2026
 *      Author: cleme
 */
#include "drivers.h"

static void Set_speed_normal(Motor_t *self, float pwmDuty);
static void Set_speed_HR(Motor_t *self, float pwmDuty);

void Motor_Init_normal(Motor_t         *self,
		TIM_HandleTypeDef  *pwm_a_timer,
		TIM_HandleTypeDef  *pwm_b_timer,
		uint32_t             pwm_a_channel,
		uint32_t             pwm_b_channel){
	self->_pwm_a_timer =pwm_a_timer;
	self->_pwm_b_timer =pwm_b_timer;
	self->_pwm_a_channel =pwm_a_channel;
	self->_pwm_b_channel =pwm_b_channel;

	self->_type =0;

	HAL_TIM_PWM_Start(pwm_a_timer, pwm_a_channel);
	__HAL_TIM_SET_COMPARE(pwm_a_timer, pwm_a_channel, 0);

	HAL_TIM_PWM_Start(pwm_b_timer, pwm_b_channel);
	__HAL_TIM_SET_COMPARE(pwm_b_timer, pwm_b_channel, 0);
}

void Motor_Init_HR(Motor_t		*self,
					uint32_t 	hr_timer,
					uint32_t 	output_mask){
	self->_hr_timer =hr_timer;

	self->_type = 1;

	// Start Timer C
	HAL_HRTIM_WaveformCounterStart(&hhrtim1, hr_timer);

	// Start outputs
	HAL_HRTIM_WaveformOutputStart(&hhrtim1, output_mask);
}

void Set_speed(Motor_t *self, float pwmDuty){
	if (self->_type){
		Set_speed_HR(self, pwmDuty);
	}
	else{
		Set_speed_normal(self, pwmDuty);
	}
}

static void Set_speed_normal(Motor_t *self, float pwmDuty){
	 //Apply to Timer (Handle direction if pwmDuty is negative)
	 if (pwmDuty >= 0) {
		__HAL_TIM_SET_COMPARE(self->_pwm_a_timer, self->_pwm_a_channel, (uint32_t)pwmDuty);
		__HAL_TIM_SET_COMPARE(self->_pwm_b_timer, self->_pwm_b_channel, 0);
	 } else {
		__HAL_TIM_SET_COMPARE(self->_pwm_a_timer, self->_pwm_a_channel, 0);
		__HAL_TIM_SET_COMPARE(self->_pwm_b_timer, self->_pwm_b_channel, (uint32_t)(-pwmDuty));
	 }
}

static void Set_speed_HR(Motor_t *self, float pwmDuty){
	 if (pwmDuty >= 3) {
		 __HAL_HRTIM_SETCOMPARE(&hhrtim1, self->_hr_timer, HRTIM_COMPAREUNIT_1, (uint32_t)(pwmDuty));
		 __HAL_HRTIM_SETCOMPARE(&hhrtim1, self->_hr_timer, HRTIM_COMPAREUNIT_1, 3);
	 }
	 else if (pwmDuty <=3){
		 __HAL_HRTIM_SETCOMPARE(&hhrtim1, self->_hr_timer, HRTIM_COMPAREUNIT_1, 3);
		 __HAL_HRTIM_SETCOMPARE(&hhrtim1, self->_hr_timer, HRTIM_COMPAREUNIT_1, (uint32_t)(-pwmDuty));
	 }
	 else {
		 __HAL_HRTIM_SETCOMPARE(&hhrtim1, self->_hr_timer, HRTIM_COMPAREUNIT_1, 3);
		 __HAL_HRTIM_SETCOMPARE(&hhrtim1, self->_hr_timer, HRTIM_COMPAREUNIT_1, 3);
	 }
}
