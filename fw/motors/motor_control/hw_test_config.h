#ifndef HW_TEST_CONFIG_H
#define HW_TEST_CONFIG_H
/* Dedicated commissioning image only. These are containment caps, NOT validated
 * physical power/travel limits. Runtime limits start unavailable (zero). */
#ifndef N_PULSE_HW_SINGLE_MOTOR_TEST
#define N_PULSE_HW_SINGLE_MOTOR_TEST 0
#endif
#ifndef N_PULSE_HW_TEST_MOTOR
#define N_PULSE_HW_TEST_MOTOR 1
#endif
#ifndef N_PULSE_HW_TEST_PWM_CAP
#define N_PULSE_HW_TEST_PWM_CAP 5
#endif
#ifndef N_PULSE_HW_TEST_STEP_CAP_RAD
#define N_PULSE_HW_TEST_STEP_CAP_RAD 0.1f
#endif
#if N_PULSE_HW_SINGLE_MOTOR_TEST != 0 && N_PULSE_HW_SINGLE_MOTOR_TEST != 1
#error Invalid single motor test flag
#endif
#if N_PULSE_HW_TEST_MOTOR < 0 || N_PULSE_HW_TEST_MOTOR > 4
#error Single motor commissioning requires an independent finger (IDs 0..4)
#endif
#if N_PULSE_HW_TEST_PWM_CAP < 1 || N_PULSE_HW_TEST_PWM_CAP > 10
#error Commissioning PWM containment cap must be 1..10 percent
#endif
#if N_PULSE_HW_SINGLE_MOTOR_TEST && defined(MOTOR_BACKEND_SIM)
#error Hardware commissioning and SIM are mutually exclusive
#endif
static inline bool HWTest_MotorAllowed(motor_id_t id) {
#if N_PULSE_HW_SINGLE_MOTOR_TEST
    return id == (motor_id_t)N_PULSE_HW_TEST_MOTOR;
#else
    return (unsigned)id < MOTOR_COUNT;
#endif
}
#endif
