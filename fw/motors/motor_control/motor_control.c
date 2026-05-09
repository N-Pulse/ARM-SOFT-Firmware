#include "motor_control.h"
#include "motor_backend.h"
#include "motor_safety.h"
#include "board_link.h"
#include "motor_l298n.h"

void Motor_InitAll(void)
{
    MotorSafety_Init();
    MotorBackend_Init();
    Motor_L298N_Init();
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

    /* Two-board topology: master forwards "non-local" motors over the
     * inter-board UART link instead of driving them itself. The slave
     * receives those commands and re-enters Motor_SetTarget locally,
     * where IsLocalMotor returns true and the rest of this function
     * runs normally. */
    if (!BoardLink_IsLocalMotor(id))
    {
        BoardLink_SendMotorCmd(id, safe_position, safe_speed);
        return;
    }

    MotorBackend_SendPreview(id, safe_position, safe_speed);
    MotorBackend_SetTarget(id, safe_position, safe_speed);

    /* Drive the physical H-bridge for whatever local motors are wired.
     * motor_l298n only acts on motors actually initialized at boot
     * (those whose pins it claimed). Motors with no L298N attached are
     * still configured as outputs but stay at duty 0. */
    Motor_L298N_MoveToAngle(id, safe_position, safe_speed);
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
    Motor_L298N_StopAll();
}
