#ifndef BENCH_H
#define BENCH_H
#include "bench_encoder.h"
#include "motor_pid.h"

#define BENCH_HARD_PWM_LIMIT 25.0f
#define BENCH_INITIAL_PWM_LIMIT 5.0f
#define BENCH_HEARTBEAT_MS 500U
#define BENCH_MAX_CONTROL_MS 50U
typedef enum { BENCH_DISARMED, BENCH_ARMED, BENCH_MOVING, BENCH_PULSE, BENCH_FAULT } bench_state_t;
typedef enum {
    BENCH_FAULT_NONE, BENCH_FAULT_ESTOP, BENCH_FAULT_RX, BENCH_FAULT_LOOP,
    BENCH_FAULT_LINK, BENCH_FAULT_UART, BENCH_FAULT_ENCODER,
    BENCH_FAULT_POSITION, BENCH_FAULT_VELOCITY, BENCH_FAULT_DELTA,
    BENCH_FAULT_MOTION_TIMEOUT, BENCH_FAULT_STALL, BENCH_FAULT_PID
} bench_fault_t;
typedef struct {
    float min_rad, max_rad, max_step_rad, max_velocity_rad_s;
    uint32_t max_move_ms;
} bench_limits_t;
typedef struct {
    uint32_t max_count_delta;
    float stall_pwm, stall_displacement_rad;
    uint32_t stall_ms;
    float settle_velocity_rad_s;
} bench_safety_config_t;
typedef struct {
    bench_state_t state;
    bench_fault_t fault;
    bench_encoder_sample_t encoder;
    bench_encoder_config_t calibration;
    bench_limits_t limits;
    bench_safety_config_t safety;
    motor_pid_config_t pid;
    float target_rad, error_rad, pid_output, applied_pwm, dt_s;
    float pwm_limit, tolerance_rad;
    uint32_t timestamp_ms, last_ping_ms;
    bool armed, encoder_verified, wiring_ack, limits_configured;
    bool pid_configured, safety_configured, session_live;
    bool target_reached;
} bench_status_t;

void Bench_Init(uint32_t raw, uint32_t now_ms, bool encoder_available);
void Bench_Update(uint32_t raw, uint32_t now_ms);
void Bench_SetEncoderAvailable(bool available);
void Bench_Ping(uint32_t now_ms);
void Bench_Disarm(void);
void Bench_Stop(void); /* Disarms: every new movement needs explicit rearming. */
void Bench_EStop(void);
bool Bench_ClearFault(void);
bool Bench_Zero(void);
bool Bench_Calibrate(const bench_encoder_config_t *config);
bool Bench_VerifyEncoder(void); /* Operator confirms powered encoder, direction, scaling. */
bool Bench_AckWiring(void); /* Operator confirms wiring AND actual motor/supply voltage. */
bool Bench_SetLimits(const bench_limits_t *limits);
bool Bench_SetSafety(const bench_safety_config_t *config);
bool Bench_SetPID(const motor_pid_config_t *config, float pwm_limit, float tolerance_rad);
bool Bench_Arm(bool wiring_supply_checked); /* true also acknowledges wiring. */
bool Bench_Move(float target_rad);
bool Bench_Jog(float step_rad);
bool Bench_Pulse(float signed_percent, uint32_t duration_ms);
const bench_status_t *Bench_Status(void);
const char *Bench_LastError(void);
const char *Bench_StateName(void);
const char *Bench_FaultName(void);
#endif
