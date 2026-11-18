#include "wrist.h"
#include "bench_hw.h"
#include <math.h>
#include <string.h>

#define DEGREES 57.29577951308232f
static wrist_status_t s;
static const char *error;
static uint32_t previous, started, settled, stall_since[2], hold_since[2];
static float stall_position[2];
static bool settling, stalled[2], hold_bad[2];

static bool reject(const char *why) { error = why; return false; }
static bool range(float v, float lo, float hi) { return isfinite(v) && v >= lo && v <= hi; }
static void sync_motor(unsigned i)
{
    wrist_motor_t *m = &s.motors[i];
    m->position = m->encoder.sample.position_rad * DEGREES;
    m->velocity = m->encoder.sample.velocity_rad_s * DEGREES;
    m->error = m->target - m->position;
}
void Wrist_Stop(void)
{
    BenchHW_Disable();
    s.armed = s.active = s.holding = s.reached = false;
    settling = false;
    for (unsigned i = 0; i < 2; ++i) {
        wrist_motor_t *m = &s.motors[i];
        m->demand = m->applied = 0;
        m->target = m->position;
        m->error = 0;
        MotorPID_Reset(&m->pid);
        stalled[i] = hold_bad[i] = false;
    }
}
static void fault(const char *why, int motor)
{
    Wrist_Stop();
    if (!s.fault) { s.fault = why; s.fault_motor = motor; }
}
static bool consume_async(void)
{
    uint32_t bits = BenchHW_TakeAsyncFault();
    if (!bits) return false;
    fault(bits & BENCH_ASYNC_ESTOP ? "estop" : bits & BENCH_ASYNC_RX ? "rx_overrun" :
          bits & BENCH_ASYNC_LOOP ? "loop_timeout" : bits & BENCH_ASYNC_LINK ? "communication_timeout" :
          bits & BENCH_ASYNC_ENCODER ? "encoder_invalid" : "uart_error", 0);
    return true;
}
static bool configurable(unsigned i)
{
    if (i > 1) return reject("invalid_motor_use_1_or_2");
    if (s.armed || s.fault) return reject("requires_disarmed_no_fault");
    return true;
}
void Wrist_Init(uint32_t now)
{
    memset(&s, 0, sizeof s);
    s.now = previous = now;
    error = "ok";
    for (unsigned i = 0; i < 2; ++i) {
        BenchEncoder_Init(&s.motors[i].encoder, BenchHW_EncoderRunning(i), BenchHW_Counter(i), now);
        s.motors[i].encoder.counter_mask = i == 0 ? UINT32_MAX : UINT16_MAX;
        s.motors[i].target = NAN;
        sync_motor(i);
    }
    Wrist_Stop();
}
void Wrist_Ping(uint32_t now)
{ s.now = s.last_ping = now; s.ping_seen = true; BenchHW_Heartbeat(now); }
void Wrist_Estop(void) { fault("estop", 0); }
bool Wrist_ClearFault(void)
{
    Wrist_Stop();
    if (consume_async()) return reject("pending_hardware_fault_retry");
    for (unsigned i = 0; i < 2; ++i) {
        wrist_motor_t *m = &s.motors[i];
        bench_encoder_config_t config = m->encoder.config;
        bool calibrated = m->encoder.sample.calibrated;
        BenchEncoder_Init(&m->encoder, BenchHW_EncoderRunning(i), BenchHW_Counter(i), s.now);
        m->encoder.counter_mask = i == 0 ? UINT32_MAX : UINT16_MAX;
        if (calibrated) (void)BenchEncoder_Configure(&m->encoder, &config);
        m->limits_set = false;
        m->target = NAN;
        sync_motor(i);
    }
    s.fault = NULL; s.fault_motor = 0;
    return true;
}
bool Wrist_Calibrate(unsigned i, const bench_encoder_config_t *c)
{
    if (!configurable(i)) return false;
    bench_encoder_t candidate = s.motors[i].encoder;
    if (!BenchEncoder_Configure(&candidate, c)) return reject("invalid_calibration");
    s.motors[i].encoder = candidate;
    /* A common zero must be repeated for BOTH motors after any calibration. */
    for (unsigned j = 0; j < 2; ++j) {
        s.motors[j].encoder.sample.zeroed = false;
        s.motors[j].limits_set = false;
        s.motors[j].target = NAN;
        sync_motor(j);
    }
    return true;
}
bool Wrist_Zero(void)
{
    if (!configurable(0)) return false;
    bench_encoder_t candidates[2] = {s.motors[0].encoder, s.motors[1].encoder};
    for (unsigned i = 0; i < 2; ++i)
        if (!candidates[i].sample.calibrated || !candidates[i].sample.valid ||
            !BenchEncoder_Zero(&candidates[i])) return reject("calibrate_both_encoders_first");
    for (unsigned i = 0; i < 2; ++i) {
        s.motors[i].encoder = candidates[i];
        s.motors[i].limits_set = false;
        s.motors[i].target = 0;
        sync_motor(i);
    }
    return true;
}
bool Wrist_Limits(unsigned i, const wrist_limits_t *c)
{
    if (!configurable(i)) return false;
    if (!c || !range(c->min_deg, -1e6f, 1e6f) || !range(c->max_deg, -1e6f, 1e6f) ||
        c->min_deg >= c->max_deg || !range(c->step_deg, 1e-6f, c->max_deg-c->min_deg) ||
        !range(c->velocity_deg_s, 1e-6f, 1e6f) || c->move_ms < 10 || c->move_ms > 60000)
        return reject("invalid_limits");
    if (!s.motors[i].encoder.sample.zeroed) return reject("common_zero_required_before_limits");
    s.motors[i].limits = *c; s.motors[i].limits_set = true;
    return true;
}
bool Wrist_PID(unsigned i, const motor_pid_config_t *c, float pwm, float tolerance)
{
    if (!configurable(i)) return false;
    motor_pid_t candidate;
    if (!c || !range(c->kp, 0, 1e6f) || !range(c->ki, 0, 1e6f) || !range(c->kd, 0, 1e6f) ||
        c->kp + c->ki + c->kd <= 0 || !range(c->integral_limit, 0, WRIST_HARD_PWM_PERCENT) ||
        !range(c->derivative_tau_s, 0, 10) || !range(c->max_dt_s, .001f, WRIST_MAX_CONTROL_MS / 1000.0f) ||
        !range(pwm, .001f, WRIST_HARD_PWM_PERCENT) || !range(tolerance, 1e-6f, 1000) ||
        !MotorPID_Init(&candidate, c)) return reject("invalid_pid");
    wrist_motor_t *m = &s.motors[i];
    m->pid = candidate; m->pwm_cap = pwm; m->tolerance = tolerance; m->pid_set = true;
    return true;
}
bool Wrist_Safety(unsigned i, const wrist_safety_t *c)
{
    if (!configurable(i)) return false;
    uint32_t max_delta = i == 0 ? 100000000U : 32767U;
    if (!c || !c->delta || c->delta > max_delta || !range(c->stall_pwm, .001f, WRIST_HARD_PWM_PERCENT) ||
        !range(c->stall_deg, 1e-9f, 1000) || c->stall_ms < 50 || c->stall_ms > 5000 ||
        !range(c->settle_deg_s, 1e-6f, 1000) || !range(c->hold_error_deg, 1e-6f, 1000) ||
        c->hold_error_ms < 10 || c->hold_error_ms > 60000) return reject("invalid_safety");
    s.motors[i].safety = *c; s.motors[i].safety_set = true;
    return true;
}
bool Wrist_Arm(void)
{
    if (consume_async()) return reject("hardware_fault");
    if (!configurable(0)) return false;
    if (!s.ping_seen || (uint32_t)(s.now-s.last_ping) > WRIST_HEARTBEAT_MS)
        return reject("heartbeat_required");
    for (unsigned i = 0; i < 2; ++i) {
        wrist_motor_t *m = &s.motors[i];
        if (!m->encoder.sample.valid || !m->encoder.sample.zeroed || !m->encoder.sample.calibrated ||
            (uint32_t)(s.now-m->encoder.sample.timestamp_ms) > WRIST_MAX_CONTROL_MS)
            return reject("both_calibrated_zeroed_fresh_encoders_required");
        if (!m->pid_set || !m->limits_set || !m->safety_set)
            return reject("both_pid_limits_safety_required");
        if (m->safety.stall_pwm > m->pwm_cap ||
            m->safety.stall_deg < fabs(m->encoder.radians_per_count)*DEGREES ||
            m->safety.hold_error_deg < m->tolerance)
            return reject("safety_incompatible_with_pid_or_resolution");
        if (!range(m->position, m->limits.min_deg, m->limits.max_deg))
            return reject("position_outside_limits");
    }
    if (!BenchHW_Arm(s.now)) return reject("hardware_not_ready");
    s.armed = true; s.active = s.holding = s.reached = false;
    for (unsigned i = 0; i < 2; ++i) {
        s.motors[i].target = s.motors[i].position;
        MotorPID_Reset(&s.motors[i].pid);
    }
    return true;
}
bool Wrist_Move(float m1, float m2)
{
    if (consume_async()) return reject("hardware_fault");
    if (!s.armed || s.fault) return reject("requires_arm");
    const float targets[] = {m1, m2};
    for (unsigned i = 0; i < 2; ++i) {
        wrist_motor_t *m = &s.motors[i];
        if (!range(targets[i], m->limits.min_deg, m->limits.max_deg) ||
            fabsf(targets[i]-m->position) > m->limits.step_deg ||
            fabsf(targets[i]-m->target) > m->limits.step_deg)
            return reject("target_outside_motor_limits");
    }
    /* Atomic validation: an invalid target for M2 never starts M1. */
    for (unsigned i = 0; i < 2; ++i) {
        s.motors[i].target = targets[i];
        MotorPID_Reset(&s.motors[i].pid);
        stalled[i] = hold_bad[i] = false;
        sync_motor(i);
    }
    s.active = true; s.holding = s.reached = settling = false;
    started = s.now;
    return true;
}
void Wrist_Update(uint32_t now)
{
    uint32_t dt = now-previous;
    s.now = now;
    consume_async();
    if (!dt) return;
    previous = now; s.dt = dt / 1000.0f;
    bool sampled[2];
    for (unsigned i = 0; i < 2; ++i) {
        wrist_motor_t *m = &s.motors[i];
        m->encoder.sample.available = BenchHW_EncoderRunning(i);
        sampled[i] = BenchEncoder_Update(&m->encoder, BenchHW_Counter(i), now);
        sync_motor(i);
    }
    BenchHW_ControlAlive(now);
    if (!s.armed) return;
    if (dt > WRIST_MAX_CONTROL_MS) { fault("loop_timeout", 0); return; }
    if ((uint32_t)(now-s.last_ping) > WRIST_HEARTBEAT_MS) { fault("communication_timeout", 0); return; }
    bool all_settled = true;
    float outputs[2] = {0, 0};
    for (unsigned i = 0; i < 2; ++i) {
        wrist_motor_t *m = &s.motors[i];
        if (!sampled[i] || !m->encoder.sample.valid || !isfinite(m->position) || !isfinite(m->velocity))
            { fault("encoder_invalid", (int)i+1); return; }
        if (fabs((double)m->encoder.sample.delta) > m->safety.delta)
            { fault("counter_delta", (int)i+1); return; }
        if (fabsf(m->velocity) > m->limits.velocity_deg_s)
            { fault("velocity_limit", (int)i+1); return; }
        if (!range(m->position, m->limits.min_deg, m->limits.max_deg))
            { fault("position_limit", (int)i+1); return; }
        if (!s.active) continue; /* Armed only: zero PWM until an explicit target. */
        bool within = fabsf(m->error) <= m->tolerance;
        all_settled &= within && fabsf(m->velocity) <= m->safety.settle_deg_s;
        if (!s.holding && (uint32_t)(now-started) >= m->limits.move_ms)
            { fault("motion_timeout", (int)i+1); return; }
        if (s.holding && fabsf(m->error) > m->safety.hold_error_deg) {
            if (!hold_bad[i]) { hold_bad[i] = true; hold_since[i] = now; }
            else if ((uint32_t)(now-hold_since[i]) >= m->safety.hold_error_ms)
                { fault("hold_error", (int)i+1); return; }
        } else hold_bad[i] = false;
        /* A stationary motor supporting load is EXPECTED: stall applies only
         * when position error exceeds tolerance, never just for holding torque. */
        if (!within && fabsf(m->applied) >= m->safety.stall_pwm) {
            if (!stalled[i]) { stalled[i] = true; stall_since[i] = now; stall_position[i] = m->position; }
            else if ((uint32_t)(now-stall_since[i]) >= m->safety.stall_ms) {
                if (fabsf(m->position-stall_position[i]) < m->safety.stall_deg)
                    { fault("stall", (int)i+1); return; }
                stall_since[i] = now; stall_position[i] = m->position;
            }
        } else stalled[i] = false;
        if (!MotorPID_Update(&m->pid, m->target, m->position, s.dt, m->pwm_cap, &outputs[i]))
            { fault("pid_invalid", (int)i+1); return; }
    }
    if (!s.active) return;
    if (all_settled) {
        if (!settling) { settling = true; settled = now; }
        s.reached = (uint32_t)(now-settled) >= WRIST_SETTLE_MS;
        if (s.reached) s.holding = true;
    } else { settling = false; s.reached = false; }
    BenchHW_SetMotors(outputs[0], outputs[1]);
    for (unsigned i = 0; i < 2; ++i) {
        s.motors[i].demand = outputs[i];
        s.motors[i].applied = BenchHW_AppliedPWM(i);
    }
}
const wrist_status_t *Wrist_Status(void) { return &s; }
const char *Wrist_Error(void) { return error; }
const char *Wrist_State(void)
{ return s.fault ? "fault" : !s.armed ? "disarmed" : !s.active ? "armed" : s.holding ? "holding" : "moving"; }
