#include "board_link.h"
#if N_PULSE_OLD_HW
#include "old_hw.h"
#include "motor_safety.h"

void BoardLink_Init(void) { OldHW_DetectRole(); }
board_role_t BoardLink_Role(void) { return g_old_hw_role; }
bool BoardLink_IsLocalMotor(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return false;
#ifdef MOTOR_BACKEND_SIM
    return true;
#else
    bool master = id == MOTOR_WRIST_X || id == MOTOR_WRIST_Y || id == MOTOR_LITTLE;
    return g_old_hw_role == BOARD_ROLE_MASTER ? master :
           g_old_hw_role == BOARD_ROLE_SLAVE && !master;
#endif
}
void BoardLink_SendMotorCmd(motor_id_t id, float target, uint8_t speed)
{
#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
    BoardLink_RouteTarget(id,target,speed,MotorSafety_EpochLocked());
#else
    (void)id; (void)target; (void)speed;
    ++g_old_hw_rejected_commands;
    MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER,MOTOR_COUNT);
#endif
}
void BoardLink_TaskCreate(void) {
#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
    BoardLink_StartTask();
#endif
}
#else

void BoardLink_Init(void)
{
    /* No board-to-board hardware is used in the single-board topology. */
}

bool BoardLink_IsLocalMotor(motor_id_t id)
{
    return (uint32_t)id < (uint32_t)MOTOR_COUNT;
}

void BoardLink_SendMotorCmd(motor_id_t id, float target_rad, uint8_t speed_percent)
{
    (void)id;
    (void)target_rad;
    (void)speed_percent;
}

void BoardLink_TaskCreate(void)
{
    /* No RX task exists in the single-board topology. */
}
#endif
