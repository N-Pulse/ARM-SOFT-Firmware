#include "intent_router.h"
#include "motor_driver.h" // Assume this provides Motor_SetPosition, Motor_Stop, etc.

/**
 * @brief Translates an abstract intent into hardware actions.
 */
void IntentRouter_Handle(const intent_t* intent) {
    if (!intent) return;

    switch (intent->id) {
        case ACTION_OPEN_HAND:
            // Move all fingers to 0 degrees (fully open)
            Motor_SetPosition(MOTOR_THUMB, 0);
            Motor_SetPosition(MOTOR_INDEX, 0);
            Motor_SetPosition(MOTOR_MIDDLE, 0);
            Motor_SetPosition(MOTOR_RING, 0);
            Motor_SetPosition(MOTOR_PINKY, 0);
            break;

        case ACTION_CLOSE_HAND:
            // Move all fingers to 90 degrees (clenched fist)
            Motor_SetPosition(MOTOR_THUMB, 90);
            Motor_SetPosition(MOTOR_INDEX, 90);
            Motor_SetPosition(MOTOR_MIDDLE, 90);
            Motor_SetPosition(MOTOR_RING, 90);
            Motor_SetPosition(MOTOR_PINKY, 90);
            break;

        case ACTION_PINCH:
            // Thumb and Index move, others remain open
            Motor_SetPosition(MOTOR_THUMB, 45);
            Motor_SetPosition(MOTOR_INDEX, 45);
            Motor_SetPosition(MOTOR_MIDDLE, 0);
            break;

        case ACTION_ROTATE_WRIST_R:
            Motor_StepWrist(10); // Rotate 10 degrees CW
            break;

        case ACTION_ROTATE_WRIST_L:
            Motor_StepWrist(-10); // Rotate 10 degrees CCW
            break;

        case ACTION_UNKNOWN:
        default:
            // Safety fallback
            Motor_StopAll();
            break;
    }
}
