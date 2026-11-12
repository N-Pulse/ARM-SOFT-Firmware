#include "boot_homing.h"
#include "main.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "motor_lock.h"
#include "motor_safety.h"
#include "motor_safety_config.h"
#include "lifecycle_config.h"
#include "hw_test_config.h"
#include "force_sensor.h"
#include <math.h>

#if !defined(MOTOR_BACKEND_SIM) && !N_PULSE_HW_SINGLE_MOTOR_TEST

#define HOME_PWM 10.0f
#define HOME_PERIOD_MS 10U
#define HOME_TIMEOUT_MS 15000U
#define HOME_START_MS 300U
#define HOME_SETTLE_MS 250U
#define HOME_TOL_RAD 0.01f
#define HOME_HOLD_MAX_RAD 0.15f
#define HOME_MIN_SPAN_RAD 0.05f
#define HOME_MAX_SPAN_RAD 6.3f
#define HOME_KP 100.0f
#define HOME_MIN_PWM 3.0f

/* Positive PWM must increase calibrated encoder angle for every motor. */
static const int8_t pwm_sign[MOTOR_COUNT] = {1,1,1,1,1,1,1,1};
#define CAL {APP_ENCODER_PPR, APP_ENCODER_QUADRATURE, APP_ENCODER_GEAR_RATIO, APP_ENCODER_DIRECTION, 0}
static encoder_calibration_t calibration[MOTOR_COUNT] = {
    CAL,CAL,CAL,CAL,CAL,CAL,CAL,CAL
};
#undef CAL

static bool attempted, running, done;
static uint32_t active_mask;
typedef struct { int64_t reference; uint32_t last_motion; } stall_t;
typedef struct { encoder_snapshot_t x,y; float rotation,flexion; } wrist_t;

static float clamp(float x, float limit) {
    return fmaxf(-limit, fminf(limit,x));
}
static bool healthy(void) {
    return !MotorSafety_IsFaultActive() && Motor_DRV8848_IsArmed() &&
           !Motor_DRV8848_FaultActive();
}
bool BootHoming_Done(void) { return done; }
bool BootHoming_OutputAllowedLocked(motor_id_t id, float pwm) {
    return (unsigned)id<MOTOR_COUNT && running &&
           (active_mask&(1U<<id)) && !Motor_IsEnabled(id) &&
           MotorSafety_CanArmLocked() &&
           MotorSafety_CanEnableLocked(id,HAL_GetTick()) && isfinite(pwm) &&
           fabsf(pwm)<=MotorSafety_PwmLimitLocked(id,HOME_PWM,HAL_GetTick());
}
static bool read_motor(motor_id_t id, encoder_snapshot_t *s) {
    /* Homing runs before TelemetryTask exists; keep the same sensor hook live. */
    ForceSensor_Service();
    return healthy() && Encoder_BSP_GetSnapshot(id,s) && s->valid &&
           isfinite(s->position_rad);
}
static bool read_wrist(wrist_t *s) {
    if(!read_motor(MOTOR_WRIST_X,&s->x) || !read_motor(MOTOR_WRIST_Y,&s->y)) return false;
    s->rotation=0.5f*(s->x.position_rad+s->y.position_rad);
    s->flexion=0.5f*(s->x.position_rad-s->y.position_rad);
    return isfinite(s->rotation) && isfinite(s->flexion);
}
/* Same raw-count progress/window criterion as the normal MotorGuard.
 * Expected end stops are local results, not latched system faults. */
static bool stalled(stall_t *s, int64_t count, uint32_t now) {
    uint64_t delta=count>=s->reference ?
        (uint64_t)count-(uint64_t)s->reference :
        (uint64_t)s->reference-(uint64_t)count;
    if(delta>=MOTOR_GUARD_EPS_COUNTS) {
        s->reference=count; s->last_motion=now;
    }
    return (uint32_t)(now-s->last_motion)>=MOTOR_GUARD_STALL_MS;
}
static void stop_outputs(void) {
    uint32_t key=Motor_Lock();
    Motor_DRV8848_StopAll(); active_mask=0;
    Motor_Unlock(key);
}
static bool drive(motor_id_t id, float pwm) {
    uint32_t key=Motor_Lock();
    bool ok=healthy() && BootHoming_OutputAllowedLocked(id,pwm*pwm_sign[id]);
    if(ok) Motor_DRV8848_SetRaw(id,pwm*pwm_sign[id]);
    Motor_Unlock(key);
    return ok;
}
static bool drive_wrist(float x, float y) {
    uint32_t key=Motor_Lock();
    bool ok=drive(MOTOR_WRIST_X,x) && drive(MOTOR_WRIST_Y,y);
    if(!ok) Motor_DRV8848_StopAll();
    Motor_Unlock(key);
    return ok;
}
static bool seek_joint(motor_id_t id) {
    encoder_snapshot_t s;
    if(!read_motor(id,&s)) return false;
    float start_position=s.position_rad;
    uint32_t start=HAL_GetTick();
    stall_t guard={s.raw_count,start};
    active_mask=1U<<id;
    for(;;) {
        if(!read_motor(id,&s)) return false;
        uint32_t now=HAL_GetTick();
        if((uint32_t)(now-start)>HOME_TIMEOUT_MS ||
           s.position_rad>start_position+HOME_TOL_RAD) return false;
        bool hit=stalled(&guard,s.raw_count,now);
        if(hit && (uint32_t)(now-start)>=HOME_START_MS) {
            stop_outputs(); HAL_Delay(HOME_SETTLE_MS); return healthy();
        }
        if(!drive(id,-HOME_PWM)) return false;
        HAL_Delay(HOME_PERIOD_MS);
    }
}
/* axis=0: rotation; axis=1: flexion. The other coordinate is held. */
static bool seek_wrist(unsigned axis, int direction, float hold, float *endpoint) {
    wrist_t s;
    if(!read_wrist(&s)) return false;
    float initial=axis ? s.flexion : s.rotation;
    uint32_t start=HAL_GetTick();
    stall_t gx={s.x.raw_count,start}, gy={s.y.raw_count,start};
    active_mask=(1U<<MOTOR_WRIST_X)|(1U<<MOTOR_WRIST_Y);
    for(;;) {
        if(!read_wrist(&s)) return false;
        uint32_t now=HAL_GetTick();
        float value=axis ? s.flexion : s.rotation;
        float error=hold-(axis ? s.rotation : s.flexion);
        if((uint32_t)(now-start)>HOME_TIMEOUT_MS || fabsf(error)>HOME_HOLD_MAX_RAD ||
           direction*(value-initial)<-HOME_TOL_RAD) return false;
        bool sx=stalled(&gx,s.x.raw_count,now);
        bool sy=stalled(&gy,s.y.raw_count,now);
        if(sx && sy && (uint32_t)(now-start)>=HOME_START_MS) {
            stop_outputs(); HAL_Delay(HOME_SETTLE_MS);
            if(!read_wrist(&s)) return false;
            *endpoint=axis ? s.flexion : s.rotation;
            return true;
        }
        float scan=direction*(HOME_PWM*0.5f);
        float correction=clamp(HOME_KP*error,HOME_PWM*0.5f);
        float x=scan+correction;
        float y=axis ? correction-scan : scan-correction;
        if(!drive_wrist(x,y)) return false;
        HAL_Delay(HOME_PERIOD_MS);
    }
}
static float position_pwm(float error) {
    if(fabsf(error)<=HOME_TOL_RAD) return 0;
    float pwm=clamp(HOME_KP*error,HOME_PWM);
    return copysignf(fmaxf(fabsf(pwm),HOME_MIN_PWM),pwm);
}
static bool center_wrist(float rotation, float flexion) {
    uint32_t start=HAL_GetTick(), settled=0;
    bool in_range=false;
    wrist_t initial;
    if(!read_wrist(&initial)) return false;
    stall_t gx={initial.x.raw_count,start}, gy={initial.y.raw_count,start};
    active_mask=(1U<<MOTOR_WRIST_X)|(1U<<MOTOR_WRIST_Y);
    for(;;) {
        wrist_t s;
        if(!read_wrist(&s)) return false;
        uint32_t now=HAL_GetTick();
        if((uint32_t)(now-start)>HOME_TIMEOUT_MS) return false;
        float ex=rotation+flexion-s.x.position_rad;
        float ey=rotation-flexion-s.y.position_rad;
        bool sx=stalled(&gx,s.x.raw_count,now);
        bool sy=stalled(&gy,s.y.raw_count,now);
        if(fabsf(ex)<=HOME_TOL_RAD) gx.last_motion=now;
        if(fabsf(ey)<=HOME_TOL_RAD) gy.last_motion=now;
        if((uint32_t)(now-start)>=HOME_START_MS &&
           ((fabsf(ex)>HOME_TOL_RAD && sx) || (fabsf(ey)>HOME_TOL_RAD && sy))) return false;
        if(fabsf(ex)<=HOME_TOL_RAD && fabsf(ey)<=HOME_TOL_RAD) {
            Motor_DRV8848_Stop(MOTOR_WRIST_X);
            Motor_DRV8848_Stop(MOTOR_WRIST_Y);
            if(!in_range) { in_range=true; settled=now; }
            if((uint32_t)(now-settled)>=HOME_SETTLE_MS) {
                stop_outputs(); return true;
            }
        } else {
            in_range=false;
            if(!drive_wrist(position_pwm(ex),position_pwm(ey))) return false;
        }
        HAL_Delay(HOME_PERIOD_MS);
    }
}
static bool find_center(unsigned axis, float hold, float *center) {
    float low,high;
    if(!seek_wrist(axis,-1,hold,&low) || !seek_wrist(axis,+1,hold,&high)) return false;
    float span=high-low;
    if(span<HOME_MIN_SPAN_RAD || span>HOME_MAX_SPAN_RAD) return false;
    *center=low+0.5f*span;
    return axis ? center_wrist(hold,*center) : center_wrist(*center,hold);
}
bool BootHoming_RunOnce(void) {
    if(attempted) return done;
    attempted=true;
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        if(Motor_IsEnabled((motor_id_t)i) ||
           !Encoder_BSP_SetCalibration((motor_id_t)i,&calibration[i])) goto fail;
    }
    if(!Motor_DRV8848_Arm()) goto fail;
    running=true;
    const motor_id_t joints[]={MOTOR_PALM,MOTOR_THUMB,MOTOR_INDEX,
        MOTOR_MIDDLE,MOTOR_RING,MOTOR_LITTLE};
    for(unsigned i=0;i<sizeof(joints)/sizeof(joints[0]);++i)
        if(!seek_joint(joints[i])) goto fail;
    wrist_t wrist;
    float rotation_center,flexion_center;
    if(!read_wrist(&wrist) ||
       !find_center(0,wrist.flexion,&rotation_center) ||
       !find_center(1,rotation_center,&flexion_center) ||
       !center_wrist(rotation_center,flexion_center)) goto fail;
    stop_outputs();
    /* Capture all references only after every axis has finished moving. */
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        encoder_snapshot_t sample;
        if(!read_motor((motor_id_t)i,&sample)) goto fail;
        calibration[i].zero_offset_counts=sample.raw_count;
    }
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        if(!Encoder_BSP_SetCalibration((motor_id_t)i,&calibration[i])) goto fail;
    }
    running=false;
    done=true;
    return true;
fail:
    stop_outputs(); running=false;
    MotorSafety_RequestStopCause(MOTOR_STOP_INIT,MOTOR_COUNT);
    Motor_ServiceStop();
    return false;
}
#else
bool BootHoming_RunOnce(void) { return false; }
bool BootHoming_Done(void) { return false; }
bool BootHoming_OutputAllowedLocked(motor_id_t id, float pwm) {
    (void)id; (void)pwm; return false;
}
#endif
