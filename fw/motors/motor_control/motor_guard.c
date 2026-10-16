#ifdef MOTOR_BACKEND_SIM
#include "motor_guard.h"
#include "motor_control.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include "motor_sim_config.h"
#include "sim_session.h"
#include <math.h>
#include <string.h>
static float reference[MOTOR_COUNT];
static uint32_t last_motion[MOTOR_COUNT],tripped;
static bool driving[MOTOR_COUNT];
void MotorGuard_Init(void) { memset(reference,0,sizeof(reference)); memset(last_motion,0,sizeof(last_motion)); memset(driving,0,sizeof(driving)); tripped=0U; }
uint32_t MotorGuard_Poll(void) {
    uint32_t newly=0U,key=Motor_Lock(),now=HAL_GetTick();
    for(unsigned i=0;i<MOTOR_COUNT;++i) {
        float position=0.0f,applied=0.0f; uint32_t bit=1U<<i;
        bool valid=SimSession_GuardSampleLocked((motor_id_t)i,&position,&applied);
        bool moved=valid && fabsf(position-reference[i])>=SIM_GUARD_EPS_RAD;
        if(tripped&bit) { if(moved) tripped&=~bit; driving[i]=false; }
        else if(!valid && Motor_OutputAllowedLocked((motor_id_t)i)) {
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_FEEDBACK,(motor_id_t)i);
        } else if(!valid || !Motor_OutputAllowedLocked((motor_id_t)i) || fabsf(applied)<SIM_GUARD_MIN_OUTPUT_PERCENT) {
            reference[i]=position; last_motion[i]=now; driving[i]=false;
        } else if(!driving[i] || moved) { reference[i]=position; last_motion[i]=now; driving[i]=true; }
        else if((uint32_t)(now-last_motion[i])>=SIM_GUARD_STALL_MS) {
            reference[i]=position; tripped|=bit; newly|=bit;
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_GUARD,(motor_id_t)i);
        }
    }
    Motor_Unlock(key); return newly;
}
bool MotorGuard_CanRecoverLocked(void) { return tripped==0U; }
#else
#include "motor_guard.h"
#include "motor_control.h"
#include "motor_safety.h"
#include "motor_safety_config.h"
#include "motor_lock.h"
#include "encoder_bsp.h"
#include "drv8848.h"
#include "hw_test_config.h"
#include <string.h>

static int64_t s_reference[MOTOR_COUNT];
static uint32_t s_last_motion_ms[MOTOR_COUNT];
static bool s_driving[MOTOR_COUNT];
static uint32_t s_sample_revision[MOTOR_COUNT];
static uint32_t s_tripped;

/* Unsigned subtraction avoids signed overflow for large accumulated counts. */
static bool moved(int64_t a, int64_t b)
{
    uint64_t delta = a >= b ? (uint64_t)a - (uint64_t)b : (uint64_t)b - (uint64_t)a;
    return delta >= MOTOR_GUARD_EPS_COUNTS;
}

void MotorGuard_Init(void)
{
    memset(s_reference, 0, sizeof(s_reference));
    memset(s_last_motion_ms, 0, sizeof(s_last_motion_ms));
    memset(s_driving, 0, sizeof(s_driving));
    memset(s_sample_revision, 0, sizeof(s_sample_revision));
    s_tripped = 0U;
}

uint32_t MotorGuard_Poll(void)
{
    if(N_PULSE_HW_FINGER_DIAGNOSTIC && !N_PULSE_DIAG_CHECK_GUARD) return 0U;
    uint32_t newly_tripped = 0U;
    for (unsigned i = 0U; i < MOTOR_COUNT; ++i) {
        motor_id_t id = (motor_id_t)i;
        uint32_t key = Motor_Lock();
        uint32_t revision = s_sample_revision[i];
        Motor_Unlock(key);
        bool local = Encoder_BSP_IsLocal(id);
        int64_t count = local ? Encoder_BSP_GetRawCount(id) : 0;
        key = Motor_Lock();
        /* Recovery also polls the guard. A BSP sample prepared before a
         * concurrent poll/trip must not clear that newer stall condition. */
        if (revision != s_sample_revision[i]) {
            Motor_Unlock(key);
            continue;
        }
        ++s_sample_revision[i];
        uint32_t now = HAL_GetTick();
        uint32_t bit = 1U << i;
        if (s_tripped & bit) {
            /* Stop remains latched. Movement is only evidence for a later
             * explicit recovery; zero drive after a stop proves nothing. */
            if (local && moved(count, s_reference[i])) s_tripped &= ~bit;
            s_driving[i] = false;
        } else if (!local && Motor_OutputAllowedLocked(id)) {
            s_tripped |= bit;
            newly_tripped |= bit;
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_GUARD, id);
        } else if (Motor_DRV8848_DrivingDir(id) == 0) {
            s_reference[i] = count;
            s_last_motion_ms[i] = now;
            s_driving[i] = false;
        } else if (!s_driving[i] || moved(count, s_reference[i])) {
            s_reference[i] = count;
            s_last_motion_ms[i] = now;
            s_driving[i] = true;
        } else if ((uint32_t)(now - s_last_motion_ms[i]) >= MOTOR_GUARD_STALL_MS) {
            s_reference[i] = count;
            s_tripped |= bit;
            newly_tripped |= bit;
            MotorSafety_RequestStopCauseLocked(MOTOR_STOP_GUARD, id);
        }
        Motor_Unlock(key);
    }
    return newly_tripped;
}

bool MotorGuard_CanRecoverLocked(void) { return s_tripped == 0U; }

#endif
