#include "encoder_bsp.h"
#include "stm32g4xx_hal.h"
#include "main.h"
#include "motor_lock.h"
#include "hw_test_config.h"
#include "old_hw_encoder_defaults.h"
#if N_PULSE_OLD_HW
#include "board_link.h"
#include "thumb_encoder.h"
#include "drv8848.h"
#include "motor_safety_config.h"
static int64_t previous_thumb_count;
#endif
#include <float.h>
#include <limits.h>

#include <math.h>
#include <string.h>

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim20;
#if !N_PULSE_OLD_HW
extern LPTIM_HandleTypeDef hlptim1;
#endif

typedef enum {
    ENCODER_TIM,
    ENCODER_LPTIM
} encoder_kind_t;

typedef struct {
    encoder_kind_t kind;
    TIM_TypeDef *tim_instance;
    bool counter_is_32bit;
} encoder_cfg_t;

#if N_PULSE_OLD_HW
static const encoder_cfg_t s_encoder_cfg[MOTOR_COUNT] = {
    [MOTOR_THUMB] = {ENCODER_TIM,NULL,false}, /* Historical TIM15 cannot decode quadrature. */
    [MOTOR_INDEX] = {ENCODER_TIM,TIM2,true},
    [MOTOR_MIDDLE] = {ENCODER_TIM,TIM3,false},
    [MOTOR_RING] = {ENCODER_TIM,TIM8,false},
    [MOTOR_LITTLE] = {ENCODER_TIM,TIM3,false},
    [MOTOR_WRIST_X] = {ENCODER_TIM,TIM2,true},
    [MOTOR_WRIST_Y] = {ENCODER_TIM,TIM8,false},
    [MOTOR_PALM] = {ENCODER_TIM,TIM20,false},
};
static TIM_HandleTypeDef *tim_handle(TIM_TypeDef *instance)
{
    if (instance == TIM2) return &htim2;
    if (instance == TIM3) return &htim3;
    if (instance == TIM8) return &htim8;
    if (instance == TIM20) return &htim20;
    return NULL;
}
#else
static const encoder_cfg_t s_encoder_cfg[MOTOR_COUNT] = {
    [MOTOR_INDEX]   = { ENCODER_TIM,   TIM8,  false }, /* PA15/PB8 */
    [MOTOR_MIDDLE]  = { ENCODER_TIM,   TIM2,  false }, /* PD3/PD4; ARR=0xffff */
    [MOTOR_RING]    = { ENCODER_TIM,   TIM4,  false }, /* PB6/PB7 */
    [MOTOR_LITTLE]  = { ENCODER_TIM,   TIM20, false }, /* PE2/PE3 */
    [MOTOR_THUMB]   = { ENCODER_TIM,   TIM1,  false }, /* PC0/PC1 */
    [MOTOR_PALM]    = { ENCODER_LPTIM, NULL,  false }, /* PB5/PC2 */
    [MOTOR_WRIST_X] = { ENCODER_TIM,   TIM5,  false }, /* PA0/PA1; ARR=0xffff */
    [MOTOR_WRIST_Y] = { ENCODER_TIM,   TIM3,  false }  /* PA6/PA4 */
};

static TIM_HandleTypeDef *tim_handle(TIM_TypeDef *instance)
{
    if (instance == TIM1) return &htim1;
    if (instance == TIM2) return &htim2;
    if (instance == TIM3) return &htim3;
    if (instance == TIM4) return &htim4;
    if (instance == TIM5) return &htim5;
    if (instance == TIM8) return &htim8;
    if (instance == TIM20) return &htim20;
    return NULL;
}

#endif

static int64_t s_raw_count[MOTOR_COUNT];
static uint32_t s_previous_counter[MOTOR_COUNT];
static float s_position_rad[MOTOR_COUNT];
static float s_velocity_rad_s[MOTOR_COUNT];
static uint32_t s_previous_ms[MOTOR_COUNT];
static bool s_started[MOTOR_COUNT];
static bool s_initialized;
static bool s_init_attempted;
static encoder_calibration_t s_calibration[MOTOR_COUNT];
static float s_scale[MOTOR_COUNT];
static bool s_numeric_fault[MOTOR_COUNT];
static bool s_count_fault[MOTOR_COUNT];

static uint32_t read_counter(motor_id_t id)
{
    const encoder_cfg_t *cfg = &s_encoder_cfg[id];
#if !N_PULSE_OLD_HW
    if (cfg->kind == ENCODER_LPTIM) {
        return (uint32_t)(uint16_t)hlptim1.Instance->CNT;
    }
#endif

    TIM_HandleTypeDef *htim = tim_handle(cfg->tim_instance);
    return htim == NULL ? 0U : __HAL_TIM_GET_COUNTER(htim);
}

static int32_t counter_delta(motor_id_t id, uint32_t current)
{
    const encoder_cfg_t *cfg = &s_encoder_cfg[id];
    if (cfg->counter_is_32bit) {
        return (int32_t)(current - s_previous_counter[id]);
    }
    return (int32_t)(int16_t)((uint16_t)current -
                              (uint16_t)s_previous_counter[id]);
}

bool Encoder_BSP_IsLocal(motor_id_t id)
{
    return (uint32_t)id < MOTOR_COUNT && s_started[id];
}

static bool calibrated_locked(motor_id_t id)
{
    return s_started[id] && s_scale[id] != 0.0f && !s_numeric_fault[id]
#if N_PULSE_OLD_HW
        && (id == MOTOR_THUMB ? ThumbEncoder_HealthyLocked() : OldHW_EncoderRunning(id))
#endif
        ;
}

bool Encoder_BSP_IsCalibrated(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return false;
    uint32_t key = Motor_Lock();
    bool valid = calibrated_locked(id);
    Motor_Unlock(key);
    return valid;
}

float Encoder_BSP_RadiansPerCount(motor_id_t id)
{
    if ((uint32_t)id>=MOTOR_COUNT) return NAN;
    uint32_t key=Motor_Lock();
    float resolution=calibrated_locked(id)?fabsf(s_scale[id]):NAN;
    Motor_Unlock(key); return resolution;
}

bool Encoder_BSP_RawAvailable(motor_id_t id)
{
    if ((uint32_t)id>=MOTOR_COUNT) return false;
    uint32_t key=Motor_Lock();
    bool valid=s_started[id] && !s_numeric_fault[id] && !s_count_fault[id];
#if N_PULSE_OLD_HW
    valid=valid && (id==MOTOR_THUMB ? ThumbEncoder_HealthyLocked() : OldHW_EncoderRunning(id));
#endif
    Motor_Unlock(key); return valid;
}

static bool calibration_scale(const encoder_calibration_t *cal, float *scale)
{
    if (!cal || !cal->counts_per_revolution ||
        (cal->quadrature_factor != 1U && cal->quadrature_factor != 2U && cal->quadrature_factor != 4U) ||
        !isfinite(cal->gear_ratio) || cal->gear_ratio <= 0.0f ||
        (cal->direction != 1 && cal->direction != -1)) return false;
    /* Check the derived denominator AND the actual float scale. Reject
     * overflow and subnormal scales rather than fabricate valid zero motion. */
    double product = (double)cal->counts_per_revolution * cal->quadrature_factor * cal->gear_ratio;
    if (!isfinite(product) || product > FLT_MAX || product < FLT_MIN) return false;
    double derived = 6.28318530717958647692 / product;
    if (!isfinite(derived) || derived > FLT_MAX || derived < FLT_MIN) return false;
    *scale = (float)derived * cal->direction;
    return isfinite(*scale) && fabsf(*scale) >= FLT_MIN;
}

bool Encoder_BSP_SetCalibration(motor_id_t id, const encoder_calibration_t *calibration)
{
    if ((uint32_t)id >= MOTOR_COUNT || !calibration) return false;
    encoder_calibration_t proposed = *calibration;
    float scale;
    if (!calibration_scale(&proposed, &scale)) return false;
    uint32_t key = Motor_Lock();
    /* Refuse live reference changes atomically with activation. Rejected
     * changes preserve the old calibration and measurement. No homing. */
    if (!s_started[id] || s_numeric_fault[id] || Motor_IsEnabled(id)
#if N_PULSE_OLD_HW
        || Motor_DRV8848_IsArmed() || Motor_DRV8848_IsTransitioning()
        || (id == MOTOR_THUMB && !ThumbEncoder_HealthyLocked())
#endif
        ) {
        Motor_Unlock(key); return false;
    }
    s_calibration[id] = proposed;
    s_scale[id] = scale;
    s_position_rad[id] = s_velocity_rad_s[id] = NAN;
    Motor_InvalidatePositionFeedback(id);
    Motor_Unlock(key);
    return true;
}

bool Encoder_BSP_Init(void)
{
    /* Only the BSP starts the timers. Repeated successful initialization must
     * neither restart HAL channels nor erase counts or calibration. */
    if (s_init_attempted) return s_initialized;
    s_init_attempted = true;
    memset(s_raw_count, 0, sizeof(s_raw_count));
    memset(s_previous_counter, 0, sizeof(s_previous_counter));
    memset(s_position_rad, 0, sizeof(s_position_rad));
    memset(s_velocity_rad_s, 0, sizeof(s_velocity_rad_s));
    memset(s_calibration, 0, sizeof(s_calibration));

    for (uint32_t i = 0U; i < MOTOR_COUNT; ++i) {
#if N_PULSE_OLD_HW
        if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
        if (i == MOTOR_THUMB) {
            uint32_t key = Motor_Lock();
            bool ok = ThumbEncoder_Init() && ThumbEncoder_ReadLocked(&previous_thumb_count);
            Motor_Unlock(key);
            if (!ok) return false;
            s_previous_ms[i] = HAL_GetTick();
            s_started[i] = true;
            g_old_hw_encoder_unavailable_mask &= ~(1U << MOTOR_THUMB);
            continue;
        }
#endif
        s_previous_ms[i] = HAL_GetTick();

#if !N_PULSE_OLD_HW
        if (s_encoder_cfg[i].kind == ENCODER_LPTIM) {
            if (HAL_LPTIM_Encoder_Start(&hlptim1, 0xFFFFU) != HAL_OK)
                return false;
            s_previous_counter[i] = read_counter((motor_id_t)i);
            s_started[i] = true;
            continue;
        }
#endif

        TIM_HandleTypeDef *htim = tim_handle(s_encoder_cfg[i].tim_instance);
        if (htim == NULL ||
            HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL) != HAL_OK)
            return false;
        s_previous_counter[i] = read_counter((motor_id_t)i);
        s_started[i] = true;
    }
#if N_PULSE_OLD_HW && N_PULSE_HW_SELECTABLE_FINGER
    /* No manual calibrate command for the common test image. Preserve the
     * real encoder validity checks; install the legacy nominal scale only
     * for local fingers, with the raw origin captured above at power-on. */
    const encoder_calibration_t nominal = {
        OLD_HW_ENCODER_PPR, OLD_HW_ENCODER_QUADRATURE,
        OLD_HW_ENCODER_GEAR_RATIO, OLD_HW_ENCODER_DIRECTION, 0
    };
    for (unsigned i = 0; i < 5U; ++i) {
        if (s_started[i] && !Encoder_BSP_SetCalibration((motor_id_t)i, &nominal))
            return false;
    }
#endif
    s_initialized = true;
    return true;
}

static void sample_locked(motor_id_t id)
{
    uint32_t now = HAL_GetTick();
    uint32_t current = 0;
    int32_t delta = 0;
#if N_PULSE_OLD_HW
    if (id == MOTOR_THUMB) {
        int64_t full;
        if (!ThumbEncoder_ReadLocked(&full)) s_numeric_fault[id] = true;
        else {
            uint64_t magnitude = full >= previous_thumb_count ? (uint64_t)full - (uint64_t)previous_thumb_count :
                                 (uint64_t)previous_thumb_count - (uint64_t)full;
            if (magnitude > INT32_MAX) s_numeric_fault[id] = true;
            else delta = full >= previous_thumb_count ? (int32_t)magnitude : -(int32_t)magnitude;
            previous_thumb_count = full;
        }
    } else if (!OldHW_EncoderRunning(id)) s_numeric_fault[id] = true;
    else
#endif
    { current = read_counter(id); delta = counter_delta(id,current); }
    uint32_t elapsed_ms = now - s_previous_ms[id];
#if N_PULSE_OLD_HW
    /* Beyond this gap a wrapped 16-bit hardware count is not trustworthy. */
    if (elapsed_ms > MOTOR_FEEDBACK_TIMEOUT_MS) s_numeric_fault[id] = true;
#endif
    if ((delta > 0 && s_raw_count[id] > INT64_MAX - delta) ||
        (delta < 0 && s_raw_count[id] < INT64_MIN - delta)) s_count_fault[id] = s_numeric_fault[id] = true;
    else if (!s_count_fault[id]) s_raw_count[id] += delta;
    s_previous_counter[id] = current;
    s_previous_ms[id] = now;
    if (calibrated_locked(id)) {
        /* Unsigned magnitude subtraction is exact even at opposite int64
         * extremes, and preserves a 1-count difference near INT64_MAX. */
        int64_t raw = s_raw_count[id], offset = s_calibration[id].zero_offset_counts;
        double difference = raw >= offset ? (double)((uint64_t)raw - (uint64_t)offset) :
                                          -(double)((uint64_t)offset - (uint64_t)raw);
        double position = difference * s_scale[id];
        double velocity = elapsed_ms ? (double)delta * 1000.0 * s_scale[id] / elapsed_ms : s_velocity_rad_s[id];
        if (!elapsed_ms && !isfinite(velocity)) velocity = 0.0;
        if (!isfinite(position) || fabs(position) > FLT_MAX ||
            !isfinite(velocity) || fabs(velocity) > FLT_MAX ||
            (position != 0.0 && (float)position == 0.0f) ||
            (velocity != 0.0 && (float)velocity == 0.0f)) s_numeric_fault[id] = true;
        else {
            s_position_rad[id] = (float)position;
            s_velocity_rad_s[id] = (float)velocity;
        }
    }
    if (!calibrated_locked(id)) {
        s_position_rad[id] = s_velocity_rad_s[id] = NAN;
        Motor_InvalidatePositionFeedback(id);
    }
}

int64_t Encoder_BSP_GetRawCount(motor_id_t id)
{
    if (!Encoder_BSP_IsLocal(id)) return 0;
    uint32_t key = Motor_Lock();
    sample_locked(id);
    int64_t raw = s_raw_count[id]; /* Last representable count after sticky overflow. */
    Motor_Unlock(key);
    return raw;
}

bool Encoder_BSP_GetSnapshot(motor_id_t id, encoder_snapshot_t *out)
{
    if (!out || !Encoder_BSP_IsLocal(id)) return false;
    uint32_t key = Motor_Lock();
    sample_locked(id);
    *out = (encoder_snapshot_t){s_raw_count[id], s_position_rad[id], s_velocity_rad_s[id], calibrated_locked(id)};
    Motor_Unlock(key);
    return out->valid;
}

float Encoder_BSP_GetPosition(motor_id_t id)
{
    encoder_snapshot_t sample;
    return Encoder_BSP_GetSnapshot(id, &sample) ? sample.position_rad : NAN;
}

float Encoder_BSP_GetVelocity(motor_id_t id)
{
#if N_PULSE_OLD_HW
    encoder_snapshot_t sample;
    return Encoder_BSP_GetSnapshot(id,&sample) ? sample.velocity_rad_s : NAN;
#else
    if (!Encoder_BSP_IsLocal(id)) return NAN;
    uint32_t key = Motor_Lock();
    float velocity = calibrated_locked(id) ? s_velocity_rad_s[id] : NAN;
    Motor_Unlock(key);
    return velocity;
#endif
}

bool Encoder_BSP_Reset(motor_id_t id)
{
    if (!Encoder_BSP_IsLocal(id)) return false;
    uint32_t key = Motor_Lock();
    if (Motor_IsEnabled(id)
#if N_PULSE_OLD_HW
        || Motor_DRV8848_IsArmed() || Motor_DRV8848_IsTransitioning()
#endif
        ) { Motor_Unlock(key); return false; }
#if N_PULSE_OLD_HW
    if (id == MOTOR_THUMB) {
        if (!ThumbEncoder_ResetLocked()) { Motor_Unlock(key); return false; }
        previous_thumb_count = 0;
    } else if (!OldHW_EncoderRunning(id)) { Motor_Unlock(key); return false; }
#endif
    /* New logical raw origin at the current hardware counter, and offset=0
     * in that new coordinate system. Never write/restart any encoder timer. */
    s_raw_count[id] = 0;
    s_calibration[id].zero_offset_counts = 0;
    s_count_fault[id] = s_numeric_fault[id] = false;
    s_previous_counter[id] = read_counter(id);
    s_position_rad[id] = s_velocity_rad_s[id] = 0.0f;
    s_previous_ms[id] = HAL_GetTick();
    Motor_InvalidatePositionFeedback(id);
    Motor_Unlock(key);
    return true;
}

bool Encoder_BSP_ResetAll(void)
{
    uint32_t key = Motor_Lock();
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
#if N_PULSE_OLD_HW
        if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
        if (Motor_DRV8848_IsArmed() || Motor_DRV8848_IsTransitioning()) { Motor_Unlock(key); return false; }
        if (i==MOTOR_THUMB ? !ThumbEncoder_CanResetLocked() : !OldHW_EncoderRunning((motor_id_t)i)) {
            Motor_Unlock(key); return false;
        }
#endif
        if (!s_started[i] || Motor_IsEnabled((motor_id_t)i)) { Motor_Unlock(key); return false; }
    }
    for (unsigned i = 0; i < MOTOR_COUNT; ++i) {
#if N_PULSE_OLD_HW
        if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
#endif
        if (!Encoder_BSP_Reset((motor_id_t)i)) { Motor_Unlock(key); return false; }
    }
    Motor_Unlock(key);
    return true;
}

void Encoder_BSP_UpdateRemote(motor_id_t id, float position, float velocity)
{
    (void)id; (void)position; (void)velocity;
}
