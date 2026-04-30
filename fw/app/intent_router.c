#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"
#include <stddef.h>

static void apply_full_pose(motor_pose_id_t pose)
{
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        float target_rad = MotorMap_GetPose(pose, (motor_id_t)i);
        Motor_SetTarget((motor_id_t)i, target_rad, 100);
    }
}

void IntentRouter_Handle(const intent_t *intent)
{
    if (intent == NULL) return;

    switch (intent->id)
    {
        case MODE_OPEN:    apply_full_pose(MOTOR_POSE_OPEN);              break;
        case MODE_CLOSE:   apply_full_pose(MOTOR_POSE_CLOSED);            break;
        case MODE_PINCH:   apply_full_pose(MOTOR_POSE_PINCH);             break;
        case MODE_WRIST_R: Motor_SetTarget(MOTOR_WRIST_X,  1.57f, 100);  break;
        case MODE_WRIST_L: Motor_SetTarget(MOTOR_WRIST_X, -1.57f, 100);  break;
        default:           Motor_StopAll();                                break;
    }
}
