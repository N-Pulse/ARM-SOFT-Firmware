 #ifndef MOTOR_MAP_H
#define MOTOR_MAP_H

#include "motor_control.h"

typedef enum
{
    MOTOR_POSE_OPEN,
    MOTOR_POSE_CLOSED,
    MOTOR_POSE_PINCH,
    MOTOR_POSE_POINT,
    MOTOR_POSE_NEUTRAL,
    MOTOR_POSE_COUNT
} motor_pose_id_t;

float MotorMap_GetPose(motor_pose_id_t pose, motor_id_t id);

#endif /* MOTOR_MAP_H */
