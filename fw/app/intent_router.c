// #include "intent_router.h"
// #include "motor_control.h"
// #include "motor_map.h"
// #include <stddef.h>

// #define SIM_POSITION_RANGE_RAD 3.1415926f

// static float q15_to_float(int16_t raw) {
//     return ((float)raw / 32767.0f) * SIM_POSITION_RANGE_RAD;
// }

// static float strength_to_blend(uint8_t strength) {
//     float blend = (float)strength / 100.0f;
//     return (blend < 0.0f) ? 0.0f : (blend > 1.0f) ? 1.0f : blend;
// }

// static float blend_pose(motor_pose_id_t from, motor_pose_id_t to, float blend, motor_id_t id) {
//     float start  = MotorMap_GetPose(from, id);
//     float target = MotorMap_GetPose(to, id);
//     return start + (target - start) * blend;
// }

// static void apply_pose(motor_pose_id_t target_pose, uint8_t strength) {
//     float blend = strength_to_blend(strength);
//     // CRITICAL: Loop through ALL 8 motors
//     for (int i = 0; i < MOTOR_COUNT; i++) {
//         motor_id_t id = (motor_id_t)i;
//         float position = blend_pose(MOTOR_POSE_OPEN, target_pose, blend, id);
//         Motor_SetTarget(id, position, strength);
//     }
// }

// static void apply_direct_control(const int16_t *positions_q15) {
//     for (int i = 0; i < MOTOR_COUNT; i++) {
//         Motor_SetTarget((motor_id_t)i, q15_to_float(positions_q15[i]), 100);
//     }
// }

// void IntentRouter_Handle(const intent_t *intent) {
//     if (intent == NULL) return;

//     switch (intent->id) {
//         case INTENT_DIRECT_CONTROL:
//             apply_direct_control(intent->positions);
//             break;
//         case INTENT_OPEN_HAND:
//             apply_pose(MOTOR_POSE_OPEN, intent->strength);
//             break;
//         case INTENT_CLOSE_HAND:
//             apply_pose(MOTOR_POSE_CLOSED, intent->strength);
//             break;
//         case INTENT_PINCH:
//             // Pinch also needs to iterate through all 8 to maintain the 'neutral' pose for wrist/palm
//             for (int i = 0; i < MOTOR_COUNT; i++) {
//                 motor_id_t id = (motor_id_t)i;
//                 motor_pose_id_t pose = (id == MOTOR_THUMB || id == MOTOR_INDEX) ? MOTOR_POSE_PINCH : MOTOR_POSE_NEUTRAL;
//                 float position = blend_pose(MOTOR_POSE_OPEN, pose, strength_to_blend(intent->strength), id);
//                 Motor_SetTarget(id, position, intent->strength);
//             }
//             break;
//         case INTENT_STOP:
//             Motor_StopAll();
//             break;
//         default: break;
//     }
// }

#include "intent_router.h"
#include "motor_control.h"
#include "motor_map.h"
#include <stddef.h>

#define SIM_POSITION_RANGE_RAD 3.1415926f

static float q15_to_float(int16_t raw) {
    return ((float)raw / 32767.0f) * SIM_POSITION_RANGE_RAD;
}

static float strength_to_blend(uint8_t strength) {
    float blend = (float)strength / 100.0f;
    return (blend < 0.0f) ? 0.0f : (blend > 1.0f) ? 1.0f : blend;
}

static float blend_pose(motor_pose_id_t from, motor_pose_id_t to, float blend, motor_id_t id) {
    float start  = MotorMap_GetPose(from, id);
    float target = MotorMap_GetPose(to, id);
    return start + (target - start) * blend;
}

static void apply_pose(motor_pose_id_t target_pose, uint8_t strength) {
    float blend = strength_to_blend(strength);
    // CRITICAL: Loop through ALL 8 motors
    for (int i = 0; i < MOTOR_COUNT; i++) {
        motor_id_t id = (motor_id_t)i;
        float position = blend_pose(MOTOR_POSE_OPEN, target_pose, blend, id);
        Motor_SetTarget(id, position, strength);
    }
}

static void apply_direct_control(const int16_t *positions_q15) {
    for (int i = 0; i < MOTOR_COUNT; i++) {
        Motor_SetTarget((motor_id_t)i, q15_to_float(positions_q15[i]), 100);
    }
}

void IntentRouter_Handle(const intent_t *intent) {
    if (intent == NULL) return;

    switch (intent->id) {
        case INTENT_DIRECT_CONTROL:
            apply_direct_control(intent->positions);
            break;
        case INTENT_OPEN_HAND:
            apply_pose(MOTOR_POSE_OPEN, intent->strength);
            break;
        case INTENT_CLOSE_HAND:
            apply_pose(MOTOR_POSE_CLOSED, intent->strength);
            break;
        case INTENT_PINCH:
            // Pinch also needs to iterate through all 8 to maintain the 'neutral' pose for wrist/palm
            for (int i = 0; i < MOTOR_COUNT; i++) {
                motor_id_t id = (motor_id_t)i;
                motor_pose_id_t pose = (id == MOTOR_THUMB || id == MOTOR_INDEX) ? MOTOR_POSE_PINCH : MOTOR_POSE_NEUTRAL;
                float position = blend_pose(MOTOR_POSE_OPEN, pose, strength_to_blend(intent->strength), id);
                Motor_SetTarget(id, position, intent->strength);
            }
            break;
        case INTENT_STOP:
            Motor_StopAll();
            break;
        default: break;
    }
}