#include "board_link.h"

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
