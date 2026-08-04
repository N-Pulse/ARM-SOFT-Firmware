/*
 * encoder_handle.c
 *
 *  Created on: Mar 26, 2026
 *      Author: cleme
 */
#include "encoder_handle.h"
#include <math.h>

#define ENC_ARR 65534
#define ENC_HALF (ENC_ARR / 2)

/* --- Local Variables --- */
static uint8_t base_timer_started = 0;

/* --- local functions prototypes --- */
static void Encoder_Calculate(Encoder_t *self);
static void Encoder_Common_Init(Encoder_t         *self,
					uint32_t           ecod_lines,
					uint32_t           multipl);

void Encoder_Init(Encoder_t         *self,
					TIM_HandleTypeDef *ecod_timer,
					uint32_t           ecod_lines,
					uint32_t           multipl)
{
	self->type = 0;
	self->_ecod_timer = ecod_timer;
    // Start this instance's hardware encoder timer
    HAL_TIM_Encoder_Start(self->_ecod_timer,
                          TIM_CHANNEL_1 | TIM_CHANNEL_2);

    Encoder_Common_Init(self, ecod_lines, multipl);

    // Snapshot current count to avoid a spike on the first callback
     self->_prevCount = (int16_t)self->_ecod_timer->Instance->CNT;
}

static void Encoder_Common_Init(Encoder_t         *self,
					uint32_t           ecod_lines,
					uint32_t           multipl){
    self->_alpha = df_alpha ;
    self->_multipl = multipl;

    // ── Private variables ────────────────────────────────────────────────────
    self->_ecod_lines = ecod_lines;
    self->_ecod_resol = ecod_lines*4 ;
    self->_prevCount = 0;
    self->_totCounts   = 0;

    // ── Public variables ─────────────────────────────────────────────────────
    self->speed    = 0.0f;
    self->pos = 0.0f;

     // Start the shared base timer only once across all instances
     if (!base_timer_started) {
         HAL_TIM_Base_Start_IT(base_timer);
         base_timer_started = 1;
     }
}


void Encoder_Reset(Encoder_t *self)
{
    // Reset position
    self->pos      = 0.0f;
    self->_totCounts   = 0;
    self->_prevCount = 0;
}

static void Encoder_Calculate(Encoder_t *self)
{
	 int16_t count;
	if (self->type ==0){
		count = (int16_t)self->_ecod_timer->Instance->CNT;
	}
    // Deplacement
    int16_t diff = (int16_t)(count - self->_prevCount);

    if (diff > ENC_HALF)
        diff -= (ENC_ARR + 1);

    if (diff < -ENC_HALF)
        diff += (ENC_ARR + 1);

    int16_t deltaCount = diff;

    //int16_t deltaCount    = count - self->_prevCount;
    self->_prevCount  = count;

    self->_totCounts += deltaCount;

    //Position
    self->pos= ((float)self->_totCounts / (float)self->_ecod_resol)
    				* (4*M_PI / (float)self->_multipl);

    //Speed
    float rawSpeed = ((float)deltaCount / (float)self->_ecod_resol)
                     * (60.0f /sample_time)
                     / (float)self->_multipl;

    self->speed = self->_alpha * rawSpeed
                + (1.0f - self->_alpha) * self->speed;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // Guard: only react to the shared base timer
    if (base_timer == NULL) return;
    if (htim->Instance != base_timer->Instance) return;

    // Dispatch to every encoder instance
    for (int i = 0; i < MOTOR_COUNT; i++) {
        Encoder_Calculate(&Encoders[i]);
    }
    control_tick=1;
}
