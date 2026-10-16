#ifndef ENCODER_BSP_H
#define ENCODER_BSP_H

#include <stdint.h>
#include <stdbool.h>
#include "motor_control.h"

/* Raw counts are independent of calibration. Angular reads return NAN when
 * unavailable; no invalid zero is suitable as controller feedback. */

/* Called once before tasks; failure must prevent normal startup. */
bool  Encoder_BSP_Init(void);
typedef struct { int64_t raw_count; float position_rad, velocity_rad_s; bool valid; } encoder_snapshot_t;
bool Encoder_BSP_GetSnapshot(motor_id_t id, encoder_snapshot_t *out);
float Encoder_BSP_GetPosition(motor_id_t id);   /* radians */
float Encoder_BSP_GetVelocity(motor_id_t id);   /* rad/s */
int64_t Encoder_BSP_GetRawCount(motor_id_t id);
bool Encoder_BSP_IsCalibrated(motor_id_t id);
bool Encoder_BSP_RawAvailable(motor_id_t id);
float Encoder_BSP_RadiansPerCount(motor_id_t id); /* positive resolution, NAN if unavailable */

typedef struct {
    uint32_t counts_per_revolution;
    uint8_t quadrature_factor;
    float gear_ratio;
    int8_t direction;
    int64_t zero_offset_counts;
} encoder_calibration_t;

/* Offset is in accumulated raw coordinates; all int64 values are supported.
 * CPR*quadrature*ratio and radians/count must be finite normal float values.
 * Reject invalid proposals without replacing the previous reference. Sticky
 * measurement/accumulation faults require a disabled-controller logical reset. */
bool Encoder_BSP_SetCalibration(motor_id_t id,
                                const encoder_calibration_t *calibration);

/* Calibration/reset refuse enabled controllers. Accepted changes invalidate
 * control feedback/PID revision. Reset establishes new raw origin, offset=0;
 * no timer restart. ResetAll refuses atomically if any controller is enabled. */
bool Encoder_BSP_Reset(motor_id_t id);
bool Encoder_BSP_ResetAll(void);

/* Vrai si ce moteur dispose d'une entrée encodeur sur la carte actuelle. */
bool Encoder_BSP_IsLocal(motor_id_t id);

/* Compatibilité d'API pour les anciens appelants ; aucun feedback distant
 * n'est utilisé dans l'architecture mono-carte. */
void Encoder_BSP_UpdateRemote(motor_id_t id, float position_rad, float velocity_rad_s);

#endif /* ENCODER_BSP_H */
