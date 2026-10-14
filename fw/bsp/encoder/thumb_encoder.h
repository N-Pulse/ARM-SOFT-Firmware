#ifndef THUMB_ENCODER_H
#define THUMB_ENCODER_H
#include <stdint.h>
#include <stdbool.h>
bool ThumbEncoder_Init(void);
void ThumbEncoder_IRQHandler(void);
/* Caller holds Motor_Lock for all task-side access. */
bool ThumbEncoder_ReadLocked(int64_t *count);
bool ThumbEncoder_HealthyLocked(void);
bool ThumbEncoder_ResetLocked(void);
bool ThumbEncoder_CanResetLocked(void);
extern volatile uint32_t g_thumb_encoder_invalid_transitions;
#endif
