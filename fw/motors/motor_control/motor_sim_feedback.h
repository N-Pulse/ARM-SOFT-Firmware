#ifndef MOTOR_SIM_FEEDBACK_H
#define MOTOR_SIM_FEEDBACK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Bounded payload decoding only. No SIM RX is connected in lot 3. */
bool process_joint_feedback(const uint8_t *payload, size_t length);
bool process_fsr_feedback(const uint8_t *payload, size_t length);
#endif
