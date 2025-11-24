#include "motor_control.h"
#include "motor_backend.h"
#include "motor_safety.h"

void Motor_InitAll(void)
{
    MotorSafety_Init();
    MotorBackend_Init();
}

void Motor_Update(void)
{
    MotorBackend_Flush();
}

void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    float   safe_position = position;
    uint8_t safe_speed    = speed_percent;
    
    if (!MotorSafety_FilterCommand(id, &safe_position, &safe_speed))
    {
        MotorBackend_StopAll();
        return;
    }

    MotorBackend_SetTarget(id, safe_position, safe_speed);
}

void Motor_SetAllTargets(float position, uint8_t speed_percent)
{
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        Motor_SetTarget((motor_id_t)i, position, speed_percent);
    }
}

void Motor_StopAll(void)
{
    MotorSafety_RequestStop();
    MotorBackend_StopAll();
}
