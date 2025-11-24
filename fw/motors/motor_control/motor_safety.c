#include "motor_safety.h"

#include <math.h>
#include <string.h>

#include "stm32g4xx_hal.h"

#define DEGREES_TO_RAD(x) ((x) * 0.0174532925f)
#define MAX_RATE_DEG_PER_SEC 180.0f
#define FORCE_SOFT_LIMIT_MN 1200U
#define FORCE_HARD_LIMIT_MN 1600U
#define FORCE_REDUCED_SPEED 25U

static const float s_pos_min[MOTOR_COUNT] = {
    DEGREES_TO_RAD(-10.0f), /* Thumb */
    DEGREES_TO_RAD(0.0f),   /* Index */
    DEGREES_TO_RAD(0.0f),   /* Middle */
    DEGREES_TO_RAD(0.0f),   /* Ring */
    DEGREES_TO_RAD(0.0f),   /* Little */
    DEGREES_TO_RAD(-30.0f), /* Wrist X */
    DEGREES_TO_RAD(-30.0f), /* Wrist Y */
    DEGREES_TO_RAD(-5.0f),  /* Palm */
};

static const float s_pos_max[MOTOR_COUNT] = {
    DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(90.0f),
    DEGREES_TO_RAD(30.0f),
    DEGREES_TO_RAD(30.0f),
    DEGREES_TO_RAD(15.0f),
};

static const uint16_t s_force_limits[MOTOR_COUNT] = {
    FORCE_HARD_LIMIT_MN, FORCE_HARD_LIMIT_MN, FORCE_HARD_LIMIT_MN, FORCE_HARD_LIMIT_MN,
    FORCE_HARD_LIMIT_MN, 2000U, 2000U, FORCE_HARD_LIMIT_MN
};

static float    s_last_command[MOTOR_COUNT];
static uint32_t s_last_command_ms[MOTOR_COUNT];
static uint16_t s_latest_force[MOTOR_COUNT];
static bool     s_fault_active;

static float clamp(float value, float min_val, float max_val)
{
    if (value < min_val)
    {
        return min_val;
    }
    if (value > max_val)
    {
        return max_val;
    }
    return value;
}

void MotorSafety_Init(void)
{
    memset(s_last_command, 0, sizeof(s_last_command));
    memset(s_latest_force, 0, sizeof(s_latest_force));
    uint32_t now = HAL_GetTick();
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        s_last_command_ms[i] = now;
    }
    s_fault_active = false;
}

static float apply_rate_limit(motor_id_t id, float desired)
{
    uint32_t now     = HAL_GetTick();
    uint32_t elapsed = now - s_last_command_ms[id];
    s_last_command_ms[id] = now;

    float max_delta = DEGREES_TO_RAD(MAX_RATE_DEG_PER_SEC) * ((float)elapsed / 1000.0f);
    float delta     = desired - s_last_command[id];

    if (fabsf(delta) > max_delta)
    {
        if (delta > 0.0f)
        {
            desired = s_last_command[id] + max_delta;
        }
        else
        {
            desired = s_last_command[id] - max_delta;
        }
    }

    s_last_command[id] = desired;
    return desired;
}

static void handle_force_limits(motor_id_t id, uint8_t *speed_percent)
{
    uint16_t force = s_latest_force[id];
    if (force >= s_force_limits[id])
    {
        s_fault_active = true;
        return;
    }

    if (force >= FORCE_SOFT_LIMIT_MN)
    {
        if (*speed_percent > FORCE_REDUCED_SPEED)
        {
            *speed_percent = FORCE_REDUCED_SPEED;
        }
    }
}

bool MotorSafety_FilterCommand(motor_id_t id, float *position, uint8_t *speed_percent)
{
    if (s_fault_active)
    {
        return false;
    }

    float clamped = clamp(*position, s_pos_min[id], s_pos_max[id]);
    clamped       = apply_rate_limit(id, clamped);

    handle_force_limits(id, speed_percent);
    if (s_fault_active)
    {
        return false;
    }

    *position = clamped;
    return true;
}

void MotorSafety_OnFeedback(const motor_feedback_t *feedback)
{
    if (feedback == NULL)
    {
        return;
    }

    if ((uint32_t)feedback->id < MOTOR_COUNT)
    {
        s_latest_force[feedback->id] = feedback->force_mN;
        s_last_command[feedback->id] = feedback->position;
    }
}

bool MotorSafety_IsFaultActive(void)
{
    return s_fault_active;
}

void MotorSafety_RequestStop(void)
{
    s_fault_active = true;
}

