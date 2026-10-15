#ifndef MOTOR_ESTIMATION_H
#define MOTOR_ESTIMATION_H
#include "motor_kalman.h"
#include "motor_control.h"
void MotorEstimation_Init(void);
void MotorEstimation_Service(void);
bool MotorEstimation_GetSnapshot(motor_id_t id,motor_kalman_t *out);
bool MotorEstimation_Reset(motor_id_t id,bool stationary_reference_verified);
void MotorEstimation_Stop(motor_id_t id);
/* Task context, disabled/disarmed only; configuration never arms/resets. */
bool MotorEstimation_Configure(motor_id_t id,const motor_kalman_config_t *config);
#endif
