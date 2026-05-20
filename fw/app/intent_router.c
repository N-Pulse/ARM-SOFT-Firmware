#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"
#include <stddef.h>

/* Presentation build : MAIN seule. Pas de poignet — les actions
 * ROTATE_WRIST_R/L (valeurs 3/4 dans le proto) tombent dans le default
 * et déclenchent un Motor_StopAll (safety). */
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
        case ACTION_OPEN_HAND:      apply_full_pose(MOTOR_POSE_OPEN);              break;
        case ACTION_CLOSE_HAND:     apply_full_pose(MOTOR_POSE_CLOSED);            break;
        case ACTION_PINCH:          apply_full_pose(MOTOR_POSE_PINCH);             break;

        case ACTION_UNKNOWN:
        default:
            /* Safety fallback (link timeout, ROTATE_WRIST ignoré, ou intent
             * non reconnu) → coupe tout. */
            Motor_StopAll();
            break;
    }
}
