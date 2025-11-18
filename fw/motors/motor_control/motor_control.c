#include "motor_control.h"

#define MOTOR_COUNT 8  // Met à jour ce nombre si tu ajoutes des moteurs

void Motor_InitAll(void) {}
void Motor_Update(void) {}

void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    (void)id; (void)position; (void)speed_percent;
    // plus tard: appel au driver BLDC / FOC
}

void Motor_SetAllTargets(float position, uint8_t speed_percent)
{
    for (int i = 0; i < MOTOR_COUNT; ++i) {
        Motor_SetTarget((motor_id_t)i, position, speed_percent);
    }
}

void Motor_StopAll(void)
{
    for (int i = 0; i < MOTOR_COUNT; ++i) {
        Motor_SetTarget((motor_id_t)i, 0.0f, 0);
    }
}
