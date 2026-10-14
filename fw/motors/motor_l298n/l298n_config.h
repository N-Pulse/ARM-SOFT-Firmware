#ifndef L298N_CONFIG_H
#define L298N_CONFIG_H
#ifndef N_PULSE_OLD_HW_MOTION_OPT_IN
#define N_PULSE_OLD_HW_MOTION_OPT_IN 0
#endif
#ifndef N_PULSE_THUMB_EXTI_MOTION_QUALIFIED
#define N_PULSE_THUMB_EXTI_MOTION_QUALIFIED 0
#endif
#if N_PULSE_THUMB_EXTI_MOTION_QUALIFIED != 0 && N_PULSE_THUMB_EXTI_MOTION_QUALIFIED != 1
#error Invalid THUMB EXTI motion qualification flag
#endif
#ifndef N_PULSE_L298N_REVERSE_BLANK_MS
#define N_PULSE_L298N_REVERSE_BLANK_MS 2U
#endif
#ifndef N_PULSE_L298N_MAX_ARM_MS
#define N_PULSE_L298N_MAX_ARM_MS 5000U
#endif
#if N_PULSE_OLD_HW_MOTION_OPT_IN != 0 && N_PULSE_OLD_HW_MOTION_OPT_IN != 1
#error Invalid OLD_HW motion opt-in
#endif
#if N_PULSE_OLD_HW_MOTION_OPT_IN && (!N_PULSE_HW_SINGLE_MOTOR_TEST || defined(MOTOR_BACKEND_SIM))
#error OLD_HW motion opt-in requires a dedicated single-motor HW commissioning build
#endif
#if N_PULSE_L298N_REVERSE_BLANK_MS < 2 || N_PULSE_L298N_REVERSE_BLANK_MS > 1000 || N_PULSE_L298N_MAX_ARM_MS < 1 || N_PULSE_L298N_MAX_ARM_MS > 5000
#error Invalid L298N blanking or arm time limit
#endif
#endif
