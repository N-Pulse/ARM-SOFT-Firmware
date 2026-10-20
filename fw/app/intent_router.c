#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"
#include <stddef.h>
#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
#include "board_link.h"
#define ROUTE_TARGET BoardLink_RouteTarget
#else
#define ROUTE_TARGET Motor_SetTargetAtEpoch
#endif

static void apply_full_pose(motor_pose_id_t pose, uint32_t epoch)
{
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        float target_rad = MotorMap_GetPose(pose, (motor_id_t)i);
        if (!ROUTE_TARGET((motor_id_t)i, target_rad, 100, epoch)) return;
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
static void drive_wrist(float rotation_rad, float flexion_rad, uint8_t speed, uint32_t epoch)
{
    if (!ROUTE_TARGET(MOTOR_WRIST_X, rotation_rad + flexion_rad, speed, epoch)) return;
    ROUTE_TARGET(MOTOR_WRIST_Y, rotation_rad - flexion_rad, speed, epoch);
}

void IntentRouter_Handle(const intent_t *intent)
{
    if (intent == NULL) return;

    switch (intent->id)
    {
        case ACTION_OPEN_HAND:      apply_full_pose(MOTOR_POSE_OPEN, intent->epoch); break;
        case ACTION_CLOSE_HAND:     apply_full_pose(MOTOR_POSE_CLOSED, intent->epoch); break;
        case ACTION_PINCH:          apply_full_pose(MOTOR_POSE_PINCH, intent->epoch); break;
        /* Rotation pure : les 2 moteurs du poignet dans le même sens. */
        case ACTION_ROTATE_WRIST_R: drive_wrist( 1.57f, 0.0f, 100, intent->epoch); break;
        case ACTION_ROTATE_WRIST_L: drive_wrist(-1.57f, 0.0f, 100, intent->epoch); break;

           case ACTION_UNKNOWN:
            /* Safety fallback (link timeout). */
            Motor_StopAll();
            break;

        default:
        {
            #ifdef MOTOR_BACKEND_SIM
                        /* DEBUG (sim builds only): move ONE motor, leave the others alone.
                        *   10..17 -> motor (id - 10) to 30 deg
                        *   20..27 -> motor (id - 20) back to 0 deg
                        * Motor order = motor_id_t: 0 thumb, 1 index, 2 middle, 3 ring,
                        * 4 little, 5 wrist_x, 6 wrist_y, 7 palm.
                        * The safety filter still clamps (e.g. palm max 15 deg). */
                        int id = (int)intent->id;
                        if ((id >= 10) && (id < 10 + MOTOR_COUNT))
                        {
                            Motor_SetTarget((motor_id_t)(id - 10), 30.0f * 0.0174532925f, 100);
                            break;
                        }
                        if ((id >= 20) && (id < 20 + MOTOR_COUNT))
                        {
                            Motor_SetTarget((motor_id_t)(id - 20), 0.0f, 100);
                            break;
                        }
            #endif
                        /* Safety fallback (unrecognised intent). */
                        Motor_StopAll();
                        break;
         }
    }
}
