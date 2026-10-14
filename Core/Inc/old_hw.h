#ifndef N_PULSE_OLD_HW_H
#define N_PULSE_OLD_HW_H
#include "stm32g4xx_hal.h"
#include "motor_control.h"

/* This branch targets RET6 only. Motion is opt-in and commissioning-only. */
#if N_PULSE_ENABLE_CURRENT_ACQUISITION
#error "OLD_HW forbids acquisition on unmapped current sensors"
#endif
typedef enum { BOARD_ROLE_UNKNOWN, BOARD_ROLE_MASTER, BOARD_ROLE_SLAVE } board_role_t;
typedef struct {
    TIM_TypeDef *timer;
    uint32_t channel;
    GPIO_TypeDef *enable_port;
    uint16_t enable_pin;
    GPIO_TypeDef *in1_port;
    uint16_t in1_pin;
    GPIO_TypeDef *in2_port;
    uint16_t in2_pin;
} old_hw_motor_pins_t;
extern const old_hw_motor_pins_t g_old_hw_motor_pins[MOTOR_COUNT];
extern volatile board_role_t g_old_hw_role; /* Debugger diagnostic, latched at boot. */
extern volatile uint32_t g_old_hw_rejected_commands;
extern volatile uint32_t g_old_hw_encoder_unavailable_mask;
void OldHW_DetectRole(void);
void OldHW_Init(void);
void OldHW_ClockConfig(void);
void OldHW_HoldOutputsLow(void);
bool OldHW_OutputsDisabled(void);
bool OldHW_EncoderRunning(motor_id_t id);
bool OldHW_HostEnabled(void);
void OldHW_Heartbeat(void);
#endif
