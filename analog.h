/*
 * analog.h
 *
 *  Created on: Apr 28, 2026
 *      Author: cleme
 */

#ifndef SRC_ANALOG_H_
#define SRC_ANALOG_H_

#include "stm32g4xx_hal.h"

//calculations
extern const float vref;
extern const float mult_curr;
extern const float mult_curr_amp;
extern const float mult_batt;

//values array
extern float curr[MOTOR_COUNT];
extern float batt_lvl;

//external definitions
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern ADC_HandleTypeDef hadc4;
extern ADC_HandleTypeDef hadc5;
extern OPAMP_HandleTypeDef hopamp1;
extern OPAMP_HandleTypeDef hopamp2;
extern OPAMP_HandleTypeDef hopamp3;
extern OPAMP_HandleTypeDef hopamp4;
extern OPAMP_HandleTypeDef hopamp5;
extern OPAMP_HandleTypeDef hopamp6;

void Analog_Init (void);

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);

void ADC_Store();

#endif /* SRC_ANALOG_H_ */
