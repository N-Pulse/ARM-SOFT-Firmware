#ifndef HW_TEST_CONFIG_H
#define HW_TEST_CONFIG_H
/* Dedicated commissioning image only. These are containment caps, NOT validated
 * physical power/travel limits. Runtime limits start unavailable (zero). */
#ifndef N_PULSE_HW_SINGLE_MOTOR_TEST
#define N_PULSE_HW_SINGLE_MOTOR_TEST 0
#endif
#ifndef N_PULSE_HW_SELECTABLE_FINGER
#define N_PULSE_HW_SELECTABLE_FINGER 0
#endif
#if N_PULSE_HW_SELECTABLE_FINGER != 0 && N_PULSE_HW_SELECTABLE_FINGER != 1
#error Invalid selectable finger flag
#endif
#if N_PULSE_HW_SELECTABLE_FINGER && (!N_PULSE_HW_SINGLE_MOTOR_TEST || !N_PULSE_DUAL_BOARD_LINK || !N_PULSE_OLD_HW)
#error Selectable fingers require single-motor commissioning and the MASTER link
#endif
#ifndef N_PULSE_HW_TEST_MOTOR
#define N_PULSE_HW_TEST_MOTOR 1
#endif
#ifndef N_PULSE_HW_TEST_PWM_CAP
#define N_PULSE_HW_TEST_PWM_CAP 5
#endif
#ifndef N_PULSE_HW_FINGER_DIAGNOSTIC
#define N_PULSE_HW_FINGER_DIAGNOSTIC 0
#endif
#if N_PULSE_HW_FINGER_DIAGNOSTIC && (!N_PULSE_HW_SELECTABLE_FINGER || !N_PULSE_OLD_HW_MOTION_OPT_IN)
#error Finger diagnostics require the explicit common motion test image
#endif
/* Re-enable these independently with build.ps1 -Diagnostic -Check... . */
#ifndef N_PULSE_DIAG_CHECK_GUARD
#define N_PULSE_DIAG_CHECK_GUARD 0
#endif
#ifndef N_PULSE_DIAG_CHECK_TRAVEL
#define N_PULSE_DIAG_CHECK_TRAVEL 0
#endif
#ifndef N_PULSE_DIAG_CHECK_STEP
#define N_PULSE_DIAG_CHECK_STEP 0
#endif
#ifndef N_PULSE_DIAG_CHECK_PREFLIGHT
#define N_PULSE_DIAG_CHECK_PREFLIGHT 0
#endif
#ifndef N_PULSE_DIAG_CHECK_ENCODER
#define N_PULSE_DIAG_CHECK_ENCODER 0
#endif
#define HW_DIAGNOSTIC_TIMED (N_PULSE_HW_FINGER_DIAGNOSTIC && !N_PULSE_DIAG_CHECK_ENCODER)
/* Same empirical time conversion as simulation_pipeline; never a measurement. */
#define HW_DIAGNOSTIC_RATE_RAD_S 2.09439510239f
#define HW_DIAGNOSTIC_MAX_RUN_MS 2000U
#if (N_PULSE_HW_FINGER_DIAGNOSTIC != 0 && N_PULSE_HW_FINGER_DIAGNOSTIC != 1) || \
    (N_PULSE_DIAG_CHECK_GUARD != 0 && N_PULSE_DIAG_CHECK_GUARD != 1) || \
    (N_PULSE_DIAG_CHECK_TRAVEL != 0 && N_PULSE_DIAG_CHECK_TRAVEL != 1) || \
    (N_PULSE_DIAG_CHECK_STEP != 0 && N_PULSE_DIAG_CHECK_STEP != 1) || \
    (N_PULSE_DIAG_CHECK_PREFLIGHT != 0 && N_PULSE_DIAG_CHECK_PREFLIGHT != 1) || \
    (N_PULSE_DIAG_CHECK_ENCODER != 0 && N_PULSE_DIAG_CHECK_ENCODER != 1)
#error Diagnostic checks must be 0 or 1
#endif
#ifndef N_PULSE_HW_TEST_STEP_CAP_RAD
#define N_PULSE_HW_TEST_STEP_CAP_RAD 0.1f
#endif
/* Common bench profile: PWM percent per radian. Its small waypoints must
 * actually reach the operator's low PWM ceiling before the stall deadline.
 * This is a bench setting, not a physically tuned production controller. */
#define HW_TEST_POSITION_KP 1000.0f
#if N_PULSE_HW_SINGLE_MOTOR_TEST != 0 && N_PULSE_HW_SINGLE_MOTOR_TEST != 1
#error Invalid single motor test flag
#endif
#if N_PULSE_HW_TEST_MOTOR < 0 || N_PULSE_HW_TEST_MOTOR > 4
#error Single motor commissioning requires an independent finger (IDs 0..4)
#endif
#if N_PULSE_HW_TEST_PWM_CAP < 1 || N_PULSE_HW_TEST_PWM_CAP > (N_PULSE_HW_FINGER_DIAGNOSTIC ? 40 : 10)
#error PWM cap must be 1..10 percent, or 1..40 in the explicit diagnostic image
#endif
#if N_PULSE_HW_SINGLE_MOTOR_TEST && defined(MOTOR_BACKEND_SIM)
#error Hardware commissioning and SIM are mutually exclusive
#endif
static inline bool HWTest_ThumbMotionAllowed(void) {
    /* -Fingers is an explicitly enabled bench image. Permit THUMB testing
     * there without declaring its EXTI acquisition physically qualified.
     * Fixed-finger/ordinary images retain the qualification gate. */
#if N_PULSE_THUMB_EXTI_MOTION_QUALIFIED || (N_PULSE_HW_SELECTABLE_FINGER && N_PULSE_OLD_HW_MOTION_OPT_IN)
    return true;
#else
    return false;
#endif
}
static inline bool HWTest_MotorAllowed(motor_id_t id) {
#if N_PULSE_HW_SELECTABLE_FINGER
    extern motor_id_t HWCommission_SelectedMotor(void);
    return (unsigned)id<5U && id==HWCommission_SelectedMotor();
#elif N_PULSE_HW_SINGLE_MOTOR_TEST
    return id == (motor_id_t)N_PULSE_HW_TEST_MOTOR;
#else
    return (unsigned)id < MOTOR_COUNT;
#endif
}
#endif
