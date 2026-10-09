#include "motor_sim_feedback.h"
#include "motor_backend.h"

static bool valid(const uint8_t *payload, size_t length, size_t stride)
{
    if (!payload || length < 1U || length > 96U) return false;
    size_t count = payload[0];
    /* size_t prevents 52*5 and 86*3 from wrapping to small uint8 lengths. */
    if (count > MOTOR_COUNT || count > (length - 1U) / stride ||
        length != 1U + count * stride) return false;
    for (size_t i = 0U; i < count; ++i)
        if (payload[1U + i * stride] >= MOTOR_COUNT) return false;
    return true;
}
static int16_t read_i16(const uint8_t *p)
{ return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U)); }
bool process_joint_feedback(const uint8_t *p, size_t length)
{
    if (!valid(p, length, 5U)) return false;
    for (size_t i = 0U; i < p[0]; ++i) {
        size_t offset = 1U + i * 5U;
        motor_feedback_t fb = { .id = (motor_id_t)p[offset],
            .position = ((float)read_i16(p + offset + 1U) / 32767.0f) * 3.1415926f,
            .velocity = ((float)read_i16(p + offset + 3U) / 32767.0f) * 3.1415926f,
            .valid_fields = MOTOR_FB_POSITION | MOTOR_FB_VELOCITY };
        MotorBackend_OnFeedback(&fb);
    }
    return true;
}
bool process_fsr_feedback(const uint8_t *p, size_t length)
{
    if (!valid(p, length, 3U)) return false;
    for (size_t i = 0U; i < p[0]; ++i) {
        size_t offset = 1U + i * 3U;
        motor_feedback_t fb = { .id = (motor_id_t)p[offset],
            .force_mN = (uint16_t)p[offset + 1U] | ((uint16_t)p[offset + 2U] << 8U),
            .valid_fields = MOTOR_FB_FORCE };
        MotorBackend_OnFeedback(&fb);
    }
    return true;
}
