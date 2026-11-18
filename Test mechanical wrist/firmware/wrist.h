#ifndef WRIST_H
#define WRIST_H
#include "bench_encoder.h"
#include "motor_pid.h"
#include "wrist_config.h"

typedef struct {
    float min_deg, max_deg, step_deg, velocity_deg_s;
    uint32_t move_ms;
} wrist_limits_t;
typedef struct {
    uint32_t delta;
    float stall_pwm, stall_deg;
    uint32_t stall_ms;
    float settle_deg_s, hold_error_deg;
    uint32_t hold_error_ms;
} wrist_safety_t;
typedef struct {
    bench_encoder_t encoder;
    motor_pid_t pid;
    wrist_limits_t limits;
    wrist_safety_t safety;
    float position, velocity, target, error, demand, applied, pwm_cap, tolerance;
    bool limits_set, pid_set, safety_set;
} wrist_motor_t;
typedef struct {
    wrist_motor_t motors[2];
    uint32_t now, last_ping;
    float dt;
    bool armed, active, holding, reached, ping_seen;
    const char *fault;
    int fault_motor; /* 0: global; 1/2: affected motor */
} wrist_status_t;

void Wrist_Init(uint32_t now);
void Wrist_Update(uint32_t now);
void Wrist_Ping(uint32_t now);
void Wrist_Stop(void);
void Wrist_Estop(void);
bool Wrist_ClearFault(void);
bool Wrist_Zero(void);
bool Wrist_Calibrate(unsigned motor, const bench_encoder_config_t *config);
bool Wrist_Limits(unsigned motor, const wrist_limits_t *config);
bool Wrist_PID(unsigned motor, const motor_pid_config_t *config, float pwm, float tolerance);
bool Wrist_Safety(unsigned motor, const wrist_safety_t *config);
bool Wrist_Arm(void);
bool Wrist_Move(float m1, float m2);
const wrist_status_t *Wrist_Status(void);
const char *Wrist_Error(void);
const char *Wrist_State(void);
#endif
