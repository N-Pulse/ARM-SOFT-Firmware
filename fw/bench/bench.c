#include "bench.h"
#include "bench_hw.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static bench_status_t s;
static bench_encoder_t encoder;
static motor_pid_t pid;
static const char *last_error;
static uint32_t previous_ms, move_started_ms, pulse_ms, settled_ms, stall_started_ms;
static float pulse_pwm, pulse_origin, stall_position;
static bool ping_seen, observed_motion, settling, stall_tracking;

static bool reject(const char *why) { last_error = why; return false; }
static bool finite_between(float x, float low, float high)
{ return isfinite(x) && x >= low && x <= high; }
static void sync_encoder(void)
{
    s.encoder = encoder.sample;
    s.encoder.valid = encoder.sample.valid && s.encoder_verified;
    s.calibration = encoder.config;
    s.error_rad = s.encoder.calibrated ? s.target_rad - s.encoder.position_rad : NAN;
}
static void stop_outputs(void)
{
    BenchHW_Disable();
    s.armed = false;
    s.pid_output = s.applied_pwm = 0.0f;
    MotorPID_Reset(&pid);
    settling = stall_tracking = false;
}
static void fault(bench_fault_t cause)
{
    stop_outputs();
    if (s.fault == BENCH_FAULT_NONE) s.fault = cause;
    s.state = BENCH_FAULT;
    s.wiring_ack = false;
    s.target_reached = false;
}
static bool consume_async(void)
{
    uint32_t bits = BenchHW_TakeAsyncFault();
    if (!bits) return false;
    /* Hardware ISR already forced the bridge off before these diagnostics. */
    fault((bits & 1U) ? BENCH_FAULT_ESTOP : (bits & 2U) ? BENCH_FAULT_RX :
          (bits & 4U) ? BENCH_FAULT_LOOP : (bits & 8U) ? BENCH_FAULT_LINK :
          (bits & 32U) ? BENCH_FAULT_ENCODER : BENCH_FAULT_UART);
    return true;
}
static bool configurable(void)
{
    if (s.armed || s.state == BENCH_FAULT) return reject("requires_disarmed_no_fault");
    return true;
}
static void changed_configuration(void)
{
    s.wiring_ack = false;
    s.target_reached = false;
    MotorPID_Reset(&pid);
    last_error = "ok";
}

void Bench_Init(uint32_t raw, uint32_t now_ms, bool encoder_available)
{
    memset(&s, 0, sizeof(s));
    BenchEncoder_Init(&encoder, encoder_available, raw, now_ms);
    memset(&pid, 0, sizeof(pid));
    s.state = BENCH_DISARMED;
    s.timestamp_ms = previous_ms = now_ms;
    s.pwm_limit = BENCH_INITIAL_PWM_LIMIT;
    s.target_rad = s.error_rad = NAN;
    ping_seen = observed_motion = false;
    settling = stall_tracking = false;
    last_error = "ok";
    stop_outputs();
    sync_encoder();
}

void Bench_SetEncoderAvailable(bool available)
{
    encoder.sample.available = available;
    if (!available) {
        encoder.sample.valid = false;
        encoder.sample.position_rad = encoder.sample.velocity_rad_s = NAN;
        s.encoder_verified = false;
        encoder.sample.zeroed = false;
        s.limits_configured = false;
        s.wiring_ack = false;
        observed_motion = false;
        if (s.armed) fault(BENCH_FAULT_ENCODER);
    }
    sync_encoder();
}

void Bench_Ping(uint32_t now_ms)
{
    s.timestamp_ms = now_ms;
    s.last_ping_ms = now_ms;
    ping_seen = true;
    s.session_live = true;
    BenchHW_Heartbeat(now_ms);
}

void Bench_Disarm(void)
{
    stop_outputs();
    if (s.fault == BENCH_FAULT_NONE) s.state = BENCH_DISARMED;
    s.target_rad = s.encoder.calibrated ? s.encoder.position_rad : NAN;
    s.target_reached = false;
    sync_encoder();
}
void Bench_Stop(void) { Bench_Disarm(); }
void Bench_EStop(void) { fault(BENCH_FAULT_ESTOP); }

bool Bench_ClearFault(void)
{
    stop_outputs();
    if (consume_async()) return reject("pending_hardware_fault_retry_after_inspection");
    if (!encoder.sample.available) return reject("encoder_unavailable");
    /* Relative coordinates cannot silently survive feedback recovery. */
    bench_encoder_config_t c = encoder.config;
    bool was_calibrated = encoder.sample.calibrated;
    BenchEncoder_Init(&encoder, true, encoder.sample.raw, s.timestamp_ms);
    if (was_calibrated && !BenchEncoder_Configure(&encoder, &c)) return reject("invalid_calibration");
    s.encoder_verified = s.wiring_ack = s.limits_configured = false;
    observed_motion = false;
    s.fault = BENCH_FAULT_NONE;
    s.state = BENCH_DISARMED;
    s.target_reached = false;
    s.target_rad = s.error_rad = NAN;
    sync_encoder();
    last_error = "ok";
    return true;
}

bool Bench_Zero(void)
{
    if (!configurable()) return false;
    if (!BenchEncoder_Zero(&encoder)) return reject("encoder_unavailable");
    s.target_rad = 0.0f;
    s.limits_configured = false; /* Existing limits referred to the old origin. */
    changed_configuration();
    sync_encoder();
    return true;
}

bool Bench_Calibrate(const bench_encoder_config_t *c)
{
    if (!configurable()) return false;
    bench_encoder_t candidate = encoder;
    if (!BenchEncoder_Configure(&candidate, c)) return reject("invalid_calibration");
    encoder = candidate;
    s.encoder_verified = s.limits_configured = false;
    observed_motion = false;
    s.target_rad = 0.0f;
    changed_configuration();
    sync_encoder();
    return true;
}

bool Bench_VerifyEncoder(void)
{
    if (!configurable()) return false;
    if (!encoder.sample.valid || !encoder.sample.calibrated || !encoder.sample.zeroed || !observed_motion)
        return reject("requires_calibration_manual_rotation_and_zero");
    s.encoder_verified = true;
    sync_encoder();
    last_error = "ok";
    return true;
}

bool Bench_AckWiring(void)
{
    if (!configurable()) return false;
    s.wiring_ack = true;
    last_error = "ok";
    return true;
}

bool Bench_SetLimits(const bench_limits_t *l)
{
    if (!configurable()) return false;
    if (!l || !finite_between(l->min_rad, -1.0e6f, 1.0e6f) ||
        !finite_between(l->max_rad, -1.0e6f, 1.0e6f) || l->min_rad >= l->max_rad ||
        !finite_between(l->max_step_rad, 1.0e-7f, l->max_rad - l->min_rad) ||
        !finite_between(l->max_velocity_rad_s, 1.0e-7f, 1.0e6f) ||
        l->max_move_ms < 10U || l->max_move_ms > 60000U) return reject("invalid_limits");
    s.limits = *l;
    s.limits_configured = true;
    changed_configuration();
    return true;
}

bool Bench_SetSafety(const bench_safety_config_t *c)
{
    if (!configurable()) return false;
    if (!c || c->max_count_delta == 0 || c->max_count_delta > 100000000U ||
        !finite_between(c->stall_pwm, 0.001f, BENCH_HARD_PWM_LIMIT) ||
        !finite_between(c->stall_displacement_rad, 1.0e-9f, 1000.0f) ||
        c->stall_ms < 50U || c->stall_ms > 5000U ||
        !finite_between(c->settle_velocity_rad_s, 1.0e-7f, 1000.0f)) return reject("invalid_safety");
    s.safety = *c;
    s.safety_configured = true;
    changed_configuration();
    return true;
}

bool Bench_SetPID(const motor_pid_config_t *c, float pwm_limit, float tolerance_rad)
{
    if (!configurable()) return false;
    motor_pid_t candidate;
    if (!c || !finite_between(c->kp, 0.0f, 1.0e6f) ||
        !finite_between(c->ki, 0.0f, 1.0e6f) || !finite_between(c->kd, 0.0f, 1.0e6f) ||
        (c->kp == 0.0f && c->ki == 0.0f && c->kd == 0.0f) ||
        !finite_between(c->integral_limit, 0.0f, BENCH_HARD_PWM_LIMIT) ||
        !finite_between(c->derivative_tau_s, 0.0f, 10.0f) ||
        !finite_between(c->max_dt_s, 0.001f, BENCH_MAX_CONTROL_MS / 1000.0f) ||
        !finite_between(pwm_limit, 0.001f, BENCH_HARD_PWM_LIMIT) ||
        !finite_between(tolerance_rad, 1.0e-7f, 1000.0f) || !MotorPID_Init(&candidate, c))
        return reject("invalid_pid_or_pwm");
    pid = candidate;
    s.pid = *c;
    s.pwm_limit = pwm_limit;
    s.tolerance_rad = tolerance_rad;
    s.pid_configured = true;
    changed_configuration();
    return true;
}

bool Bench_Arm(bool wiring_supply_checked)
{
    if (consume_async()) return reject("hardware_fault");
    if (!configurable()) return false;
    if (wiring_supply_checked) s.wiring_ack = true;
    if (!ping_seen || !s.session_live || (uint32_t)(s.timestamp_ms - s.last_ping_ms) > BENCH_HEARTBEAT_MS)
        return reject("heartbeat_required");
    if (!s.wiring_ack) return reject("ack_wiring_supply_required");
    if (!s.encoder.valid || !s.encoder.zeroed || !s.encoder_verified ||
        (uint32_t)(s.timestamp_ms - s.encoder.timestamp_ms) > BENCH_MAX_CONTROL_MS)
        return reject("verified_fresh_encoder_required");
    if (!s.pid_configured || !s.limits_configured || !s.safety_configured)
        return reject("pid_limits_safety_required");
    if (s.safety.stall_pwm > s.pwm_limit ||
        s.safety.stall_displacement_rad < fabs(encoder.radians_per_count))
        return reject("stall_threshold_incompatible_with_pwm_or_encoder");
    if (s.encoder.position_rad < s.limits.min_rad || s.encoder.position_rad > s.limits.max_rad)
        return reject("position_outside_limits");
    if (!BenchHW_Arm(s.timestamp_ms)) {
        consume_async();
        return reject("hardware_not_ready");
    }
    s.state = BENCH_ARMED;
    s.armed = true;
    s.target_reached = false;
    s.target_rad = s.encoder.position_rad;
    sync_encoder();
    MotorPID_Reset(&pid);
    last_error = "ok";
    return true;
}

bool Bench_Move(float target_rad)
{
    if (consume_async()) return reject("hardware_fault");
    if (s.state != BENCH_ARMED || !s.armed) return reject("requires_armed_stopped");
    if (!isfinite(target_rad) || target_rad < s.limits.min_rad || target_rad > s.limits.max_rad ||
        fabsf(target_rad - s.encoder.position_rad) > s.limits.max_step_rad) return reject("target_outside_limits");
    s.target_rad = target_rad;
    s.state = BENCH_MOVING;
    s.target_reached = false;
    move_started_ms = s.timestamp_ms;
    settling = stall_tracking = false;
    MotorPID_Reset(&pid);
    sync_encoder();
    last_error = "ok";
    return true;
}

bool Bench_Jog(float step_rad)
{
    if (!isfinite(step_rad) || fabsf(step_rad) > s.limits.max_step_rad) return reject("step_outside_limits");
    return Bench_Move(s.encoder.position_rad + step_rad);
}

bool Bench_Pulse(float signed_percent, uint32_t duration_ms)
{
    if (consume_async()) return reject("hardware_fault");
    if (s.state != BENCH_ARMED || !s.armed) return reject("requires_armed_stopped");
    float cap = fminf(s.pwm_limit, BENCH_INITIAL_PWM_LIMIT);
    if (!finite_between(signed_percent, -cap, cap) || signed_percent == 0.0f ||
        duration_ms == 0 || duration_ms > 500U || duration_ms > s.limits.max_move_ms)
        return reject("pulse_max_5_percent_500_ms_within_limits");
    pulse_pwm = signed_percent;
    pulse_origin = s.encoder.position_rad;
    pulse_ms = duration_ms;
    move_started_ms = s.timestamp_ms;
    s.target_rad = s.encoder.position_rad;
    s.state = BENCH_PULSE;
    s.target_reached = false;
    stall_tracking = false;
    last_error = "ok";
    return true;
}

void Bench_Update(uint32_t raw, uint32_t now_ms)
{
    uint32_t dt_ms = now_ms - previous_ms;
    s.timestamp_ms = now_ms;
    s.session_live = ping_seen && (uint32_t)(now_ms - s.last_ping_ms) <= BENCH_HEARTBEAT_MS;
    consume_async();
    if (dt_ms == 0) return;
    previous_ms = now_ms;
    s.dt_s = dt_ms / 1000.0f;
    bool sampled = BenchEncoder_Update(&encoder, raw, now_ms);
    if (sampled && encoder.sample.delta != 0) observed_motion = true;
    sync_encoder();
    BenchHW_ControlAlive(now_ms);
    if (!s.armed) return;
    if (dt_ms > BENCH_MAX_CONTROL_MS) { fault(BENCH_FAULT_LOOP); return; }
    if (!s.session_live) { fault(BENCH_FAULT_LINK); return; }
    if (!sampled || !s.encoder.valid || !isfinite(s.encoder.position_rad) || !isfinite(s.encoder.velocity_rad_s))
        { fault(BENCH_FAULT_ENCODER); return; }
    uint64_t delta = s.encoder.delta < 0 ? (uint64_t)(-(int64_t)s.encoder.delta) : (uint64_t)s.encoder.delta;
    if (delta > s.safety.max_count_delta) { fault(BENCH_FAULT_DELTA); return; }
    if (fabsf(s.encoder.velocity_rad_s) > s.limits.max_velocity_rad_s) { fault(BENCH_FAULT_VELOCITY); return; }
    if (s.encoder.position_rad < s.limits.min_rad || s.encoder.position_rad > s.limits.max_rad)
        { fault(BENCH_FAULT_POSITION); return; }
    if (s.state == BENCH_ARMED) { BenchHW_SetMotor(0.0f); return; }
    uint32_t elapsed = now_ms - move_started_ms;
    if (s.state == BENCH_PULSE && fabsf(s.encoder.position_rad - pulse_origin) > s.limits.max_step_rad)
        { fault(BENCH_FAULT_POSITION); return; }
    if (s.state == BENCH_PULSE && elapsed >= pulse_ms) { Bench_Disarm(); return; }
    if (elapsed >= s.limits.max_move_ms) { fault(BENCH_FAULT_MOTION_TIMEOUT); return; }
    float output;
    if (s.state == BENCH_MOVING) {
        if (fabsf(s.error_rad) <= s.tolerance_rad && fabsf(s.encoder.velocity_rad_s) <= s.safety.settle_velocity_rad_s) {
            if (!settling) { settling = true; settled_ms = now_ms; }
            if ((uint32_t)(now_ms - settled_ms) >= 100U) {
                stop_outputs();
                s.state = BENCH_DISARMED;
                s.target_reached = true;
                return;
            }
            output = 0.0f;
            MotorPID_Reset(&pid);
        } else {
            settling = false;
            if (!MotorPID_Update(&pid, s.target_rad, s.encoder.position_rad, s.dt_s, s.pwm_limit, &output))
                { fault(BENCH_FAULT_PID); return; }
        }
    } else output = pulse_pwm;
    if (!isfinite(output) || fabsf(output) > BENCH_HARD_PWM_LIMIT || fabsf(output) > s.pwm_limit)
        { fault(BENCH_FAULT_PID); return; }
    s.pid_output = s.state == BENCH_MOVING ? output : 0.0f;
    BenchHW_SetMotor(output);
    s.applied_pwm = BenchHW_AppliedPWM();
    if (fabsf(s.applied_pwm) >= s.safety.stall_pwm) {
        if (!stall_tracking) { stall_tracking = true; stall_started_ms = now_ms; stall_position = s.encoder.position_rad; }
        else if ((uint32_t)(now_ms - stall_started_ms) >= s.safety.stall_ms) {
            if (fabsf(s.encoder.position_rad - stall_position) < s.safety.stall_displacement_rad)
                { fault(BENCH_FAULT_STALL); return; }
            stall_started_ms = now_ms;
            stall_position = s.encoder.position_rad;
        }
    } else stall_tracking = false;
}

const bench_status_t *Bench_Status(void) { return &s; }
const char *Bench_LastError(void) { return last_error; }
const char *Bench_StateName(void)
{
    static const char *names[] = {"disarmed", "armed", "moving", "pulse", "fault"};
    return names[s.state];
}
const char *Bench_FaultName(void)
{
    static const char *names[] = {"none", "estop", "rx_overrun", "loop_timeout", "communication_timeout",
        "uart_error", "encoder_invalid", "position_limit", "velocity_limit", "counter_delta",
        "motion_timeout", "stall", "pid_invalid"};
    return names[s.fault];
}
