#include "motor_control.h"
#include "motor_backend.h"
#include "motor_safety.h"
#include "board_link.h"
#include "motor_l298n.h"
#include "motor_pid.h"
#include "motor_pid_config.h"

static motor_state_t s_motors[MOTOR_COUNT];

static void Motor_UpdateReferences(float dt);
static void Motor_UpdatePID(motor_id_t id, motor_state_t *motor,float measurement_dt);
static void Motor_UpdateAllPID(motor_state_t *motors, size_t count, float dt);
static void Motor_ApplyAllOutput(const motor_state_t *motors, size_t count);

void Motor_InitAll(void)
{
    MotorSafety_Init();

    /* Initialize PID, stops if initialization failed*/
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        motor_state_t *motor = &s_motors[i];

        motor->target_position    = 0.0f;
        motor->reference_position = 0.0f;
        motor->max_reference_rate = 180.0f * 0.0174532925f; // rad/s
        motor->pwm_limit           = 0.0f;
        motor->measured_position   = 0.0f;
        motor->output_pwm          = 0.0f;
        motor->enabled            = false;

        if (!MotorPID_Init(&motor->pid, &g_motor_pid_config[i]))
        {
            return;
        }
    }

    MotorBackend_Init();
    Motor_L298N_Init();

}

// Main loop in ControlTask, performed every 10 ms
void Motor_Update(float dt)
{
    MotorBackend_RefreshFeedback();
    Motor_UpdateReferences(dt);
    Motor_UpdateAllPID(s_motors, MOTOR_COUNT, dt);
    Motor_ApplyAllOutput(s_motors, MOTOR_COUNT);
    MotorBackend_Flush();
}

void Motor_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    float   safe_position = position;
    uint8_t safe_speed    = speed_percent;

    if ((uint32_t)id >= MOTOR_COUNT) {
        return;
    }

    if (!MotorSafety_FilterCommand(id, &safe_position, &safe_speed))
    {
        MotorBackend_StopAll();
        return;
    }

    /* Two-board topology: master forwards "non-local" motors over the
     * inter-board UART link instead of driving them itself. The slave
     * receives those commands and re-enters Motor_SetTarget locally,
     * where IsLocalMotor returns true and the rest of this function
     * runs normally. */
    if (!BoardLink_IsLocalMotor(id))
    {
        BoardLink_SendMotorCmd(id, safe_position, safe_speed);
        return;
    }

    motor_state_t *motor = &s_motors[id];

    motor->target_position = safe_position;
    motor->pwm_limit = safe_speed > 100U ? 100U : safe_speed;

}

void Motor_SetAllTargets(float position, uint8_t speed_percent)
{
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        Motor_SetTarget((motor_id_t)i, position, speed_percent);
    }
}

void Motor_OnPositionFeedback(motor_id_t id, float position_rad, float velocity_rad_s)
{
    if ((uint32_t)id >= MOTOR_COUNT) {
        return;
    }

    if (!isfinite(position_rad)) {
        return;
    }

    motor_state_t *motor = &s_motors[id];
    motor->measured_position = position_rad;
}

static void Motor_UpdateReferences(float dt)
{
    if (!isfinite(dt) || dt <= 0.0f) {
        return;
    }

    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        motor_state_t *motor = &s_motors[i];

        if (!motor->enabled) {
            continue;
        }

        float max_step = motor->max_reference_rate * dt;
        float delta =
            motor->target_position - motor->reference_position;

        if (delta > max_step) {
            delta = max_step;
        } else if (delta < -max_step) {
            delta = -max_step;
        }

        motor->reference_position += delta;
    }
}

static void Motor_UpdatePID(motor_id_t id, motor_state_t *motor,float measurement_dt)
{
    bool pid_state = MotorPID_Update(
        &motor->pid,
        motor->reference_position,
        motor->measured_position,
        measurement_dt,
        motor->pwm_limit,
        &motor->output_pwm);

    if (!pid_state)
    {
        motor->enabled = false;
        motor->output_pwm = 0.0f;
        MotorPID_Reset(&motor->pid);
        return;
    }

}

static void Motor_UpdateAllPID(motor_state_t *motors, size_t count, float dt)
{
    if (motors == NULL) {
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        motor_state_t *motor = &motors[i];

        if (!motor->enabled)
        {
            motor->output_pwm = 0.0f;
            MotorPID_Reset(&motor->pid);
            continue;
        }

        Motor_UpdatePID((motor_id_t)i, motor, dt);
    }
}

static void Motor_ApplyAllOutput(const motor_state_t *motors, size_t count)
{
    if (motors == NULL || count > MOTOR_COUNT) {
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const motor_state_t *motor = &motors[i];

        float output = 0.0f;

        if (motor->enabled) {
            output = motor->output_pwm;
        }

        MotorBackend_ApplyOutput((motor_id_t)i, motor->reference_position, output);
    }
}

void Motor_StopAll(void)
{
    MotorSafety_RequestStop();
    MotorBackend_StopAll();
    Motor_L298N_StopAll();
}
