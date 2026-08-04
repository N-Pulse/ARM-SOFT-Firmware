/*
 * encoder_handle.h
 *
 *  Created on: Mar 26, 2026
 *      Author: cleme
 */
#ifndef ENCODER_HANDLE_H_
#define ENCODER_HANDLE_H_

#include "main.h"

// Constants
extern const float sample_time;
extern const float df_alpha;

/* --- Encoder class --- */

typedef struct {
    // ── Private variables
    TIM_HandleTypeDef  *_ecod_timer;
    int16_t             _prevCount;
    int32_t             _totCounts;
    uint32_t            _ecod_resol;
    uint32_t            _ecod_lines;
    uint32_t            _multipl;

    // ── Shared variables
    float               _alpha;

    // ── Public variables
    volatile float      speed;
    volatile float      pos;

    uint8_t type;
} Encoder_t;

/* --- defined in main --- */
extern Encoder_t Encoders[MOTOR_COUNT];
extern TIM_HandleTypeDef *base_timer;
extern volatile uint8_t control_tick ;

extern const float sample_time;
extern const int ARR;

/* --- Constructor --- */
void Encoder_Init(Encoder_t         *self,
                       TIM_HandleTypeDef *ecod_timer,
                       uint32_t           ecod_lines,
                       uint32_t           multipl);

/* --- Public functions --- */
void Encoder_Reset(Encoder_t *self);

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

#endif /* SRC_ENCODER_HANDLE_H_ */
