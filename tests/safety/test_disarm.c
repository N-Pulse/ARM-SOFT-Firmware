#define main original_suite
#include "test_safety.c"
#undef main
int main(void) {
    reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX) != 0);
    Motor_DRV8848_Disarm();
    printf("REPRO disarm: enabled=%d fault=%d\n", Motor_IsEnabled(MOTOR_INDEX), MotorSafety_IsFaultActive());
    assert_stopped();
    assert(MotorSafety_IsFaultActive());
    assert(!Motor_DRV8848_Arm());
    assert(Motor_RecoverFromFault()); arm(); step(10U, true);
    assert(!Motor_IsEnabled(MOTOR_INDEX));
    assert(Motor_Enable(MOTOR_INDEX)); step(10U, true);
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX) == 0);
    Motor_SetTarget(MOTOR_INDEX, 1.0f, 80U); step(10U, true);
    assert(Motor_DRV8848_DrivingDir(MOTOR_INDEX) != 0);
    puts("PASS public disarm requires recovery, arm, activation, new command");
    reset(); arm(); enable(MOTOR_INDEX); step(10U,true);
    primask=1U; Motor_DRV8848_Disarm(); assert(cleanups==0U);
    assert(!Motor_IsEnabled(MOTOR_INDEX) && !Motor_DRV8848_IsArmed());
    primask=0U; Motor_ServiceStop(); assert_stopped();
    puts("PASS masked public disarm cuts immediately and defers all HAL cleanup");
    for (unsigned n = 1; n <= 6; ++n) {
        reset(); stop_at_start = n;
        assert(!Motor_DRV8848_Arm()); assert(starts == n); assert_stopped();
        assert(!Motor_DRV8848_IsTransitioning());
        assert(Motor_RecoverFromFault()); arm(); Motor_StopAll(); Motor_ServiceStop();
        reset(); fail_at_start = n;
        assert(!Motor_DRV8848_Arm()); assert(starts == n); assert_stopped();
        assert(MotorSafety_GetStopCauses() & MOTOR_STOP_DRIVER);
        assert(cleanups == 6U); fail_at_start = 0;
        assert(Motor_RecoverFromFault()); assert_stopped();
    }
    puts("PASS STOP and partial HAL failure at all six arm stages; delay covered by lot2");
    for (unsigned n = 1; n <= 6; ++n) {
        reset(); arm(); enable(MOTOR_INDEX); step(10U, true);
        probe_cleanup_at = n; Motor_DRV8848_Disarm(); assert_stopped();
        assert(cleanups == 6U); probe_cleanup_at = 0;
        assert(Motor_RecoverFromFault()); arm();
        unsigned done = cleanups;
        Motor_ServiceStop(); assert(cleanups == done && Motor_DRV8848_IsArmed());
        Motor_DRV8848_Disarm(); Motor_DRV8848_Disarm(); assert_stopped();
        reset(); arm(); fail_cleanup_at = n; Motor_DRV8848_Disarm();
        assert_stopped(); assert(MotorSafety_GetStopCauses() & MOTOR_STOP_DRIVER);
        fail_cleanup_at = cleanups + n; assert(!Motor_RecoverFromFault());
        fail_cleanup_at = 0; assert(Motor_RecoverFromFault()); assert_stopped();
    }
    puts("PASS cleanup ownership blocks arm/recovery, repeated disarm and failed cleanup retries");
    for (unsigned n=1;n<=6;++n) {
        reset(); probe_cleanup_at=n; cleanup_probe_without_stop=true;
        assert(Motor_DRV8848_CleanupStoppedInternal());
        assert(!MotorSafety_IsFaultActive()); probe_cleanup_at=0;
        arm(); unsigned done=cleanups;
        Motor_ServiceStop(); assert(cleanups==done && Motor_DRV8848_IsArmed());
    }
    puts("PASS cleanup exclusivity even without a fault latch; no old cleanup after accepted arm");
    reset(); arm(); for(unsigned i=0;i<MOTOR_COUNT;++i) enable((motor_id_t)i); step(10U,true);
    Motor_DRV8848_Disarm(); assert_stopped();
    puts("PASS direct public disarm includes both wrists and all eight enabled controllers");
    reset(); arm(); enable(MOTOR_INDEX); injection = 6; step(10U, true); assert_stopped();
    puts("PASS public disarm after production PID preparation rejects commit");
    return 0;
}
