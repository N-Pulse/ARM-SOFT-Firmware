#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"
#include "motor_lock.h"
#include "motor_safety.h"
#include <stddef.h>

static bool apply_full_pose(motor_pose_id_t pose, uint32_t epoch)
{
    bool ok=true;
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        float target_rad = MotorMap_GetPose(pose, (motor_id_t)i);
        ok=Motor_SetTargetAtEpoch((motor_id_t)i,target_rad,100,epoch) && ok;
    }
    return ok;
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
static bool drive_wrist(float rotation_rad, float flexion_rad, uint8_t speed, uint32_t epoch)
{
    bool x=Motor_SetTargetAtEpoch(MOTOR_WRIST_X,rotation_rad+flexion_rad,speed,epoch);
    bool y=Motor_SetTargetAtEpoch(MOTOR_WRIST_Y,rotation_rad-flexion_rad,speed,epoch);
    return x && y;
}

bool IntentRouter_Handle(const intent_t *intent)
{
    if (intent == NULL) return false;

    uint32_t key=Motor_Lock();
    if(intent->epoch!=MotorSafety_EpochLocked() || !MotorSafety_CanArmLocked()) {
        Motor_Unlock(key); return false;
    }
    if(intent->id!=ACTION_GRASPING && intent->id!=ACTION_PINCHING) Motor_EndGripLocked();
    bool ok=true;
    switch (intent->id)
    {
        case ACTION_OPEN_HAND:      ok=apply_full_pose(MOTOR_POSE_OPEN, intent->epoch); break;
        case ACTION_CLOSE_HAND:     ok=apply_full_pose(MOTOR_POSE_CLOSED, intent->epoch); break;
        case ACTION_PINCH:          ok=apply_full_pose(MOTOR_POSE_PINCH, intent->epoch); break;
        /* Rotation pure : les 2 moteurs du poignet dans le même sens. */
        case ACTION_ROTATE_WRIST_R: ok=drive_wrist( 1.57f, 0.0f, 100, intent->epoch); break;
        case ACTION_ROTATE_WRIST_L: ok=drive_wrist(-1.57f, 0.0f, 100, intent->epoch); break;
        case ACTION_THUMB_OPEN:      ok=apply_full_pose(MOTOR_THUMB_OPEN, intent->epoch); break;
        case ACTION_THUMB_CLOSE:      ok=apply_full_pose(MOTOR_THUMB_CLOSE, intent->epoch); break;
        case ACTION_INDEX_OPEN:      ok=apply_full_pose(MOTOR_INDEX_OPEN, intent->epoch); break;
        case ACTION_INDEX_CLOSE:      ok=apply_full_pose(MOTOR_INDEX_CLOSE, intent->epoch); break;
        case ACTION_MIDDLE_OPEN:      ok=apply_full_pose(MOTOR_MIDDLE_OPEN, intent->epoch); break;
        case ACTION_MIDDLE_CLOSE:      ok=apply_full_pose(MOTOR_MIDDLE_CLOSE, intent->epoch); break;
        case ACTION_RING_OPEN:      ok=apply_full_pose(MOTOR_RING_OPEN, intent->epoch); break;
        case ACTION_RING_CLOSE:      ok=apply_full_pose(MOTOR_RING_CLOSE, intent->epoch); break;
        case ACTION_LITTLE_OPEN:      ok=apply_full_pose(MOTOR_LITTLE_OPEN, intent->epoch); break;
        case ACTION_LITTLE_CLOSE:      ok=apply_full_pose(MOTOR_LITTLE_CLOSE, intent->epoch); break;
        case ACTION_WRIST_PALM:      ok=apply_full_pose(MOTOR_WRIST_PALMARFLEXION, intent->epoch); break;
        case ACTION_WRIST_DORSI:      ok=apply_full_pose(MOTOR_WRIST_DORSIFLEXION, intent->epoch); break;
        case ACTION_GRASPING:
            ok=Motor_BeginGrip((1U<<MOTOR_THUMB)|(1U<<MOTOR_INDEX)|(1U<<MOTOR_MIDDLE)|
                (1U<<MOTOR_RING)|(1U<<MOTOR_LITTLE),intent->epoch); break;
        case ACTION_PINCHING:
            ok=Motor_BeginGrip((1U<<MOTOR_THUMB)|(1U<<MOTOR_INDEX),intent->epoch); break;
        case ACTION_UNKNOWN:
            /* Safety fallback (link timeout or unrecognised intent). */
            Motor_StopAll(); ok=false;
            break;

        default: Motor_StopAll(); ok=false; break;
    }
    Motor_Unlock(key); return ok;
}
