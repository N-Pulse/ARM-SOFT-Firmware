#include "intent_router.h"
#include <stddef.h>

// Hardware Interface Mocks/Externs
extern void Motor_SetTarget(int id, float position, uint8_t strength);
extern float MotorMap_GetPose(int pose, int id);
extern void Motor_StopAll(void);

// Motor Map Enums (matching your motor_map.c)
typedef enum {
    MOTOR_POSE_OPEN,
    MOTOR_POSE_CLOSED,
    MOTOR_POSE_PINCH,
    MOTOR_POSE_NEUTRAL
} motor_pose_id_t;

#define MOTOR_WRIST_X 5

// Helper to apply a full-hand pose at 100% strength
static void apply_full_pose(motor_pose_id_t pose) {
    for(int i = 0; i < 8; i++) {
        float target_rad = MotorMap_GetPose(pose, i);
        Motor_SetTarget(i, target_rad, 100); 
    }
}

void IntentRouter_Handle(const intent_t* intent) {
    if (intent == NULL) return;

    switch(intent->id) {
        case MODE_OPEN:  
            apply_full_pose(MOTOR_POSE_OPEN);
            break;
            
        case MODE_CLOSE: 
            apply_full_pose(MOTOR_POSE_CLOSED);
            break;
            
        case MODE_PINCH: 
            apply_full_pose(MOTOR_POSE_PINCH);
            break;
            
        case MODE_WRIST_R: 
            Motor_SetTarget(MOTOR_WRIST_X, 1.57f, 100);
            break;
            
        case MODE_WRIST_L: 
            Motor_SetTarget(MOTOR_WRIST_X, -1.57f, 100);
            break;
            
        default:
            // This handles MODE_NEUTRAL or any undefined index
            Motor_StopAll();
            break;
    }
}