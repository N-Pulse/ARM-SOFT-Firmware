#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"

#include <stddef.h>

static float blend_pose(motor_pose_id_t from, motor_pose_id_t to, float blend, motor_id_t id)
{
    float start  = MotorMap_GetPose(from, id);
    float target = MotorMap_GetPose(to, id);
    return start + (target - start) * blend;
}

static float strength_to_blend(uint8_t strength)
{
    float blend = (float)strength / 100.0f;
    if (blend < 0.0f)
    {
        blend = 0.0f;
    }
    if (blend > 1.0f)
    {
        blend = 1.0f;
    }
    return blend;
}

static void apply_pose(motor_pose_id_t target_pose, uint8_t strength)
{
    float blend = strength_to_blend(strength);
    for (motor_id_t id = (motor_id_t)0; id < MOTOR_COUNT; id = (motor_id_t)(id + 1))
    {
        float position = blend_pose(MOTOR_POSE_OPEN, target_pose, blend, id);
        Motor_SetTarget(id, position, strength);
    }
}

static void apply_pinch(uint8_t strength)
{
    float blend = strength_to_blend(strength);
    for (motor_id_t id = (motor_id_t)0; id < MOTOR_COUNT; id = (motor_id_t)(id + 1))
    {
        motor_pose_id_t pose = (id == MOTOR_THUMB || id == MOTOR_INDEX) ? MOTOR_POSE_PINCH : MOTOR_POSE_NEUTRAL;
        float           position = blend_pose(MOTOR_POSE_OPEN, pose, blend, id);
        Motor_SetTarget(id, position, strength);
    }
}

void IntentRouter_Handle(const intent_t *intent)
{
    if (intent == NULL)
    {
        return;
    }

    switch (intent->id)
    {
    case INTENT_OPEN_HAND:
        apply_pose(MOTOR_POSE_OPEN, intent->strength);
        break;

    case INTENT_CLOSE_HAND:
        apply_pose(MOTOR_POSE_CLOSED, intent->strength);
        break;

    case INTENT_PINCH:
        apply_pinch(intent->strength);
        break;

    case INTENT_POINT:
        apply_pose(MOTOR_POSE_POINT, intent->strength);
        break;

    case INTENT_STOP:
        Motor_StopAll();
        break;

    default:
        break;
    }
}
