#ifndef MOTOR_L298N_H
#define MOTOR_L298N_H
#include "motor_control.h"
typedef enum { L298N_INHIBITED, L298N_ZERO, L298N_BLANKED, L298N_APPLIED, L298N_FAULT } l298n_output_result_t;
void Motor_L298N_Init(void);
bool Motor_L298N_Arm(void);
void Motor_L298N_Disarm(void);
bool Motor_L298N_CleanupStoppedInternal(void);
void Motor_L298N_InhibitLocked(void);
bool Motor_L298N_IsArmed(void);
bool Motor_L298N_IsTransitioning(void);
bool Motor_L298N_FaultActive(void);
l298n_output_result_t Motor_L298N_ApplySigned(motor_id_t id,float output_percent);
void Motor_L298N_SetRaw(motor_id_t id,float output_percent);
void Motor_L298N_SetSignedRaw(motor_id_t id,int direction,uint8_t speed_percent);
void Motor_L298N_Stop(motor_id_t id);
void Motor_L298N_Brake(motor_id_t id);
void Motor_L298N_StopAll(void);
int Motor_L298N_DrivingDir(motor_id_t id);
float Motor_L298N_AppliedPWM(motor_id_t id);
#endif
