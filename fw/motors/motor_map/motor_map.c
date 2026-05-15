#include "motor_map.h"

#include <stddef.h>

#define DEG2RAD(x) ((x) * 0.0174532925f)

/* Reset position = main ouverte + poignet droit → tous les angles à 0°.
 * Chaque moteur est à sa contrainte mécanique au boot, et on appelle ça 0°.
 * Les autres poses sont définies par rapport à ce zéro.
 *
 * v2 : amplitudes divisées par 2 — les courses précédentes étaient trop
 * grandes pour le mécanisme réel (la main fermait trop loin / forçait). */
static const float s_pose_table[MOTOR_POSE_COUNT][MOTOR_COUNT] = {
    [MOTOR_POSE_OPEN] = {
        DEG2RAD(0.0f),   DEG2RAD(0.0f),   DEG2RAD(0.0f),   DEG2RAD(0.0f),
        DEG2RAD(0.0f),   DEG2RAD(0.0f),   DEG2RAD(0.0f),   DEG2RAD(0.0f),
    },
    [MOTOR_POSE_CLOSED] = {
        DEG2RAD(42.5f),  DEG2RAD(42.5f),  DEG2RAD(42.5f),  DEG2RAD(40.0f),
        DEG2RAD(37.5f),  DEG2RAD(7.5f),   DEG2RAD(7.5f),   DEG2RAD(5.0f),
    },
    [MOTOR_POSE_PINCH] = {
        DEG2RAD(35.0f),  DEG2RAD(35.0f),  DEG2RAD(12.5f),  DEG2RAD(10.0f),
        DEG2RAD(10.0f),  DEG2RAD(2.5f),   DEG2RAD(2.5f),   DEG2RAD(2.5f),
    },
    [MOTOR_POSE_POINT] = {
        DEG2RAD(20.0f),  DEG2RAD(2.5f),   DEG2RAD(35.0f),  DEG2RAD(35.0f),
        DEG2RAD(32.5f),  DEG2RAD(5.0f),   DEG2RAD(0.0f),   DEG2RAD(0.0f),
    },
    [MOTOR_POSE_NEUTRAL] = {
        DEG2RAD(10.0f),  DEG2RAD(10.0f),  DEG2RAD(10.0f),  DEG2RAD(10.0f),
        DEG2RAD(10.0f),  DEG2RAD(0.0f),   DEG2RAD(0.0f),   DEG2RAD(0.0f),
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
