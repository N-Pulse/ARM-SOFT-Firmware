#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"
#include "finger_test.h"
#include <stddef.h>

/* Speed used for the L298N single-finger test bench. */
#define FINGER_TEST_SPEED_PCT  70U

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
        case ACTION_OPEN_HAND:
            apply_full_pose(MOTOR_POSE_OPEN);
            Finger_Test_MoveToPose(MOTOR_POSE_OPEN, FINGER_TEST_SPEED_PCT);
            break;
        case ACTION_CLOSE_HAND:
            apply_full_pose(MOTOR_POSE_CLOSED);
            Finger_Test_MoveToPose(MOTOR_POSE_CLOSED, FINGER_TEST_SPEED_PCT);
            break;
        case ACTION_PINCH:
            apply_full_pose(MOTOR_POSE_PINCH);
            Finger_Test_MoveToPose(MOTOR_POSE_PINCH, FINGER_TEST_SPEED_PCT);
            break;
        case ACTION_ROTATE_WRIST_R: Motor_SetTarget(MOTOR_WRIST_X,  1.57f, 100);   break;
        case ACTION_ROTATE_WRIST_L: Motor_SetTarget(MOTOR_WRIST_X, -1.57f, 100);   break;

        case ACTION_UNKNOWN:
        default:
            /* Safety fallback (link timeout or unrecognised intent). */
            Motor_StopAll();
            Finger_Test_Stop();
            break;
    }
}
