#include "motor_backend.h"
#include "main.h"
#include "motor_safety.h"
#include "motor_lock.h"

#if !defined(MOTOR_BACKEND_SIM)

#include "encoder_bsp.h"
#include "adc_dma_bsp.h"
#include "board_link.h"
#if N_PULSE_OLD_HW
#include "l298n_config.h"
#endif

/* Signed PID output uses the OLD_HW L298N boundary. Production stays disarmed;
 * a dedicated commissioning build and fresh operator preflight are required.
 * Current acquisition stays unavailable until an old-board signal path exists.
 * Non-OLD native regression tests retain their original DRV8848 boundary. */

void MotorBackend_Init(void)
{
    if (!ADC_DMA_BSP_Init()) {
        Error_Handler();
    }
#if N_PULSE_OLD_HW && !N_PULSE_OLD_HW_MOTION_OPT_IN
    MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER, MOTOR_COUNT);
#endif

    /* Raw acquisition is optional and off by default. It provides no
     * calibrated current feedback or validated current protection. */
}

void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    (void)id;
    (void)position;
    (void)speed_percent;
}

void MotorBackend_SendPreview(motor_id_t id, float position, uint8_t speed_percent)
{
    /* Sim-only : pas de port série de preview en HW mode. */
    (void)id;
    (void)position;
    (void)speed_percent;
}

void MotorBackend_Flush(void)
{
    /* Direct PID output: no queued hardware command can survive STOP. */
}

void MotorBackend_StopAll(void)
{
    Motor_DRV8848_StopAll();
}

void MotorBackend_InhibitOutputsLocked(void) { /* No HW queue. */ }

//Get Encoder Feedbacks
void MotorBackend_RefreshFeedback(void)
{
    if (Motor_DRV8848_FaultActive())
    {
        MotorSafety_RequestStopCause(MOTOR_STOP_DRIVER, MOTOR_COUNT);
#if !N_PULSE_OLD_HW
        return;
#endif
    }

    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
        motor_id_t id = (motor_id_t)i;

        motor_feedback_t feedback = {0};
        feedback.id = id;
        uint32_t key = Motor_Lock();
        encoder_snapshot_t sample;
        if (!Encoder_BSP_GetSnapshot(id, &sample)) {
            Motor_InvalidatePositionFeedback(id);
            Motor_Unlock(key);
            continue;
        }
        feedback.position = sample.position_rad;
        feedback.velocity = sample.velocity_rad_s;
        feedback.valid_fields = MOTOR_FB_POSITION | MOTOR_FB_VELOCITY;

        /* Reference changes and delivery share the same short serialization domain. */
        MotorBackend_OnFeedback(&feedback);
        Motor_Unlock(key);
    }
}

void MotorBackend_ApplyOutput(motor_id_t id, float reference_position, float output_percent)
{
    (void)reference_position; // HW 输出只需要 PID 的结果

    if ((uint32_t)id >= MOTOR_COUNT) {
        return;
    }

    if (!isfinite(output_percent)) {
        MotorSafety_RequestStopCause(MOTOR_STOP_PID, id);
        return;
    }

    if (output_percent > 100.0f) {
        output_percent = 100.0f;
    } else if (output_percent < -100.0f) {
        output_percent = -100.0f;
    }

    Motor_DRV8848_SetRaw(id, output_percent);
}

#endif /* !MOTOR_BACKEND_SIM */
