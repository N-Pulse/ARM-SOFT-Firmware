#include "drivers.h"
#include "encoder_handle.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim20;

/*
 *
 * VARIABLES
 *
 */

/* --- Motor Variables --- */
//counter period on PWM timer. Should be equal or less than 8499 to ensure that the signals are not audible
const int ARR = htim8.Instance->ARR-1; //(change if not timer8)

Motor_t   Motors[MOTOR_COUNT];

/* --- Encoder Variables --- */
//encoder loop time (sec)
const float sample_time = 0.001f;

//default value for encoder filter
const float df_alpha = 0.1f;

Encoder_t Encoders[MOTOR_COUNT];
TIM_HandleTypeDef *base_timer = &htim6;

/*
 *
 * INITIALIZATION
 *
 */

/* --- Motors --- */
void MotorBackend_Init(void)
{
	//Motor_Init(&Motors[index], timer_pwm_a, timer_pwm_b, channel_pwm_a, channel_pwm_b);
	Motor_Init(&Motors[0], &htim8, &htim8, TIM_CHANNEL_3,TIM_CHANNEL_4);

	Encoder_Init(&Encoders[0], &htim1, 12, 64);		//for moon encoders
	Encoder_Init(&Encoders[1], &htim1, 4096, 203);	//for faulhaber encoders
}

/*
 *
 * USE
 *
 */

void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
	float current_pos = Encoders[id].pos;           // radians, même unité que safety
	float direction   = (position >= current_pos) ? 1.0f : -1.0f;
	float duty        = direction * ((float)speed_percent / 100.0f) * (float)ARR;

	Set_speed(&Motors[id], duty); //duty is a float, a number between 0 and ARR
}

float MotorBackend_GetPosition(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return 0.0f;
    return Encoders[id].pos;
}

void MotorBackend_StopAll(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++) {
        Set_speed(&Motors[i], 0.0f);
    }
}
