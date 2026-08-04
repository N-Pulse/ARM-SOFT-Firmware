/*
 * analog.c
 *
 *  Created on: Apr 28, 2026
 *      Author: cleme
 */
#include "analog.h"

#define ADC1_CH 1
#define ADC2_CH 2
#define ADC3_CH 1
#define ADC4_CH 3
#define ADC5_CH 2

uint16_t adc1_buf[ADC1_CH]={0};
uint16_t adc2_buf[ADC2_CH]={0};
uint16_t adc3_buf[ADC3_CH]={0};
uint16_t adc4_buf[ADC4_CH]={0};
uint16_t adc5_buf[ADC5_CH]={0};

uint8_t adc1_ready =0;
uint8_t adc2_ready =0;
uint8_t adc3_ready =0;
uint8_t adc4_ready =0;
uint8_t adc5_ready =0;

void Analog_Init (void){
	HAL_OPAMP_Start(&hopamp1);
	HAL_OPAMP_Start(&hopamp2);
	HAL_OPAMP_Start(&hopamp3);
	HAL_OPAMP_Start(&hopamp4);
	HAL_OPAMP_Start(&hopamp5);
	HAL_OPAMP_Start(&hopamp6);
	HAL_ADC_Start_DMA(&hadc1, adc1_buf, ADC1_CH);
	HAL_ADC_Start_DMA(&hadc2, adc2_buf, ADC2_CH);
	HAL_ADC_Start_DMA(&hadc3, adc3_buf, ADC3_CH);
	HAL_ADC_Start_DMA(&hadc4, adc4_buf, ADC4_CH);
	HAL_ADC_Start_DMA(&hadc5, adc5_buf, ADC5_CH);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1){
		adc1_ready = 1;
	}
	if (hadc->Instance == ADC2){
		adc2_ready = 1;
	}
	if (hadc->Instance == ADC3){
		adc3_ready = 1;
	}
	if (hadc->Instance == ADC4){
		adc4_ready = 1;
	}
	if (hadc->Instance == ADC5){
		adc5_ready = 1;
	}
}

void ADC_Store(){
	if ((adc1_ready)&&(adc2_ready)&&(adc3_ready)&&(adc4_ready)&&(adc5_ready)){
		curr[0] = (vref*adc1_buf[0]/4095)*mult_curr;
		curr[1] = (vref*adc2_buf[0]/4095)*mult_curr_amp;
		curr[2] = (vref*adc2_buf[1]/4095)*mult_curr_amp;
		curr[3] = (vref*adc5_buf[0]/4095)*mult_curr_amp;
		curr[4] = (vref*adc3_buf[0]/4095)*mult_curr_amp;
		curr[5] = (vref*adc4_buf[0]/4095)*mult_curr_amp;
		curr[6] = (vref*adc5_buf[1]/4095)*mult_curr_amp;
		curr[7] = (vref*adc4_buf[2]/4095)*mult_curr_amp;
		batt_lvl = (vref*adc4_buf[1]/4095)*mult_batt;
	}
}
