#include "motor_backend.h"
#include "motor_safety.h"

#if !defined(MOTOR_BACKEND_SIM)

// ─── Hardware backend — STM32G474 ─────────────────────────────────────────────
//
// This file is the bridge between the motor logic layer and the real silicon.
// It must be completed in four steps once pin mapping is confirmed.
//
// STEP 1 — HRTIM (fw/bsp/hrtim/hrtim_bsp.c)
//   The STM32G474 HRTIM has 6 timer units (A–F). Assign one output per motor
//   driver IC PWM input. Carrier frequency target: 50–100 kHz.
//   → Implement HRTIM_BSP_Init() and HRTIM_BSP_SetDuty() in hrtim_bsp.c.
//   → Call HRTIM_BSP_Init() from MotorBackend_Init() below.
//
// STEP 2 — GPIO enables
//   Each motor driver IC needs EN / SLEEP / FAULT pins.
//   → Add the GPIOs to the .ioc file and regenerate.
//   → In MotorBackend_Init(): assert EN, deassert SLEEP, check FAULT.
//   → In MotorBackend_StopAll(): de-assert EN or set all PWM duty to 0.
//
// STEP 3 — MotorBackend_SetTarget() (position → PWM)
//   For brushed DC + encoder:
//     duty      = speed_percent / 100.0f
//     direction = sign(target_position - current_position_from_encoder)
//     → HRTIM_BSP_SetDuty(id, duty)
//     → set direction GPIO
//   For FOC / BLDC: feed target + electrical angle to the FOC library.
//
// STEP 4 — Feedback loop (fw/bsp/adc_dma/adc_dma_bsp.c,
//                          fw/bsp/encoder/encoder_bsp.c)
//   ADC in DMA mode reads current shunts → call MotorBackend_OnFeedback().
//   Timer encoder mode reads shaft position → used in STEP 3 for direction.
// ─────────────────────────────────────────────────────────────────────────────

void MotorBackend_Init(void)
{
    // TODO STEP 1: call HRTIM_BSP_Init() once hrtim_bsp.c is filled in.
    // TODO STEP 2: assert GPIO enable pins for all motor driver ICs.
    //              Example:
    //              HAL_GPIO_WritePin(MOT0_EN_GPIO_Port, MOT0_EN_Pin, GPIO_PIN_SET);
    //              ... repeat for MOT1..MOT7 ...
    // TODO STEP 4: call ADC_DMA_BSP_Init() and Encoder_BSP_Init().
}

void MotorBackend_SetTarget(motor_id_t id, float position, uint8_t speed_percent)
{
    // TODO STEP 3: convert position + speed_percent to PWM duty + direction.
    //
    // Minimal DC-motor skeleton (replace with FOC if using BLDC):
    //
    //   float   duty      = (float)speed_percent / 100.0f;
    //   float   current   = Encoder_BSP_GetPosition(id);
    //   GPIO_PinState dir = (position >= current) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    //
    //   HAL_GPIO_WritePin(MOTOR_DIR_PORT[id], MOTOR_DIR_PIN[id], dir);
    //   HRTIM_BSP_SetDuty(id, duty);
    (void)id;
    (void)position;
    (void)speed_percent;
}

void MotorBackend_Flush(void)
{
    // HW backend writes directly to HRTIM compare registers — nothing to flush.
}

void MotorBackend_StopAll(void)
{
    // TODO STEP 2/3: set all PWM duty to 0 and/or de-assert EN pins.
    //   HRTIM_BSP_SetAllDuty(0.0f);
    //   HAL_GPIO_WritePin(MOT_EN_GPIO_Port, MOT_EN_Pin, GPIO_PIN_RESET);
}

void MotorBackend_OnFeedback(const motor_feedback_t *feedback)
{
    // Called from the ADC DMA ISR (or a dedicated feedback task) with measured
    // current and encoder position. No changes needed here.
    MotorSafety_OnFeedback(feedback);
}

#endif /* !MOTOR_BACKEND_SIM */
