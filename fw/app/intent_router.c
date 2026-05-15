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

/* Poignet différentiel : WRIST_X et WRIST_Y sont 2 moteurs couplés
 * mécaniquement. Ils doivent TOUJOURS être commandés ensemble :
 *
 *   - même sens   → le poignet TOURNE  (rotation / prono-supination)
 *   - sens opposé → le poignet SE PLIE (flexion / extension)
 *
 * Mixage différentiel :
 *   WRIST_X =  rotation + flexion
 *   WRIST_Y =  rotation − flexion
 *
 * Donc rotation pure  → flexion = 0  → les 2 moteurs vont dans le même sens.
 *      flexion pure    → rotation = 0 → les 2 moteurs vont en sens opposé. */
static void drive_wrist(float rotation_rad, float flexion_rad, uint8_t speed)
{
    Motor_SetTarget(MOTOR_WRIST_X, rotation_rad + flexion_rad, speed);
    Motor_SetTarget(MOTOR_WRIST_Y, rotation_rad - flexion_rad, speed);
}

void IntentRouter_Handle(const intent_t *intent)
{
    if (intent == NULL) return;

    switch (intent->id)
    {
        case ACTION_OPEN_HAND:      apply_full_pose(MOTOR_POSE_OPEN);              break;
        case ACTION_CLOSE_HAND:     apply_full_pose(MOTOR_POSE_CLOSED);            break;
        case ACTION_PINCH:          apply_full_pose(MOTOR_POSE_PINCH);             break;
        /* Rotation pure : les 2 moteurs du poignet dans le même sens. */
        case ACTION_ROTATE_WRIST_R: drive_wrist( 1.57f, 0.0f, 100);               break;
        case ACTION_ROTATE_WRIST_L: drive_wrist(-1.57f, 0.0f, 100);               break;

        case ACTION_UNKNOWN:
        default:
            /* Safety fallback (link timeout or unrecognised intent). */
            Motor_StopAll();
            break;
    }
}
