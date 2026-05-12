#include "motor_map.h"

#include <stddef.h>

#define DEG2RAD(x) ((x) * 0.0174532925f)

/* Reset position = main ouverte + poignet droit → tous les angles à 0°.
 * Chaque moteur est à sa contrainte mécanique au boot, et on appelle ça 0°.
 * Les autres poses sont définies par rapport à ce zéro. */
static const float s_pose_table[MOTOR_POSE_COUNT][MOTOR_COUNT] = {
    [MOTOR_POSE_OPEN] = {
        DEG2RAD(0.0f),  DEG2RAD(0.0f),  DEG2RAD(0.0f),  DEG2RAD(0.0f),
        DEG2RAD(0.0f),  DEG2RAD(0.0f),  DEG2RAD(0.0f),  DEG2RAD(0.0f),
    },
    [MOTOR_POSE_CLOSED] = {
        DEG2RAD(85.0f), DEG2RAD(85.0f), DEG2RAD(85.0f), DEG2RAD(80.0f),
        DEG2RAD(75.0f), DEG2RAD(15.0f), DEG2RAD(15.0f), DEG2RAD(10.0f),
    },
    [MOTOR_POSE_PINCH] = {
        DEG2RAD(70.0f), DEG2RAD(70.0f), DEG2RAD(25.0f), DEG2RAD(20.0f),
        DEG2RAD(20.0f), DEG2RAD(5.0f),  DEG2RAD(5.0f),  DEG2RAD(5.0f),
    },
    [MOTOR_POSE_POINT] = {
        DEG2RAD(40.0f), DEG2RAD(5.0f),  DEG2RAD(70.0f), DEG2RAD(70.0f),
        DEG2RAD(65.0f), DEG2RAD(10.0f), DEG2RAD(0.0f),  DEG2RAD(0.0f),
    },
    [MOTOR_POSE_NEUTRAL] = {
        DEG2RAD(20.0f), DEG2RAD(20.0f), DEG2RAD(20.0f), DEG2RAD(20.0f),
        DEG2RAD(20.0f), DEG2RAD(0.0f),  DEG2RAD(0.0f),  DEG2RAD(0.0f),
    },
};

float MotorMap_GetPose(motor_pose_id_t pose, motor_id_t id)
{
    if ((pose >= MOTOR_POSE_COUNT) || (id >= MOTOR_COUNT))
    {
        return 0.0f;
    }

    return s_pose_table[pose][id];
}
