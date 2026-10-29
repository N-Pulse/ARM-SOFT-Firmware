/* Production pure C modules with a bridge-enable hardware mock. No motor or
 * electrical model is simulated; assertions concern software state only. */
#include "bench.h"
#include "bench_hw.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

static bool gate;
static float applied;
static uint32_t async_fault;
static unsigned scenarios;
void BenchHW_Disable(void) { gate = false; applied = 0; }
bool BenchHW_Arm(uint32_t now) { (void)now; gate = async_fault == 0; return gate; }
void BenchHW_Heartbeat(uint32_t now) { (void)now; }
void BenchHW_ControlAlive(uint32_t now) { (void)now; }
void BenchHW_SetMotor(float p) { applied = gate ? p : 0; }
float BenchHW_AppliedPWM(void) { return applied; }
uint32_t BenchHW_TakeAsyncFault(void) { uint32_t f = async_fault; async_fault = 0; return f; }
static void near(float actual, float expected, float tolerance)
{ assert(isfinite(actual)); assert(fabsf(actual - expected) <= tolerance); }

static void encoder_tests(void)
{
    bench_encoder_t e;
    BenchEncoder_Init(&e, true, 0xFFFFFFFD, 0);
    assert(e.sample.available && !e.sample.calibrated && !e.sample.valid);
    assert(isnan(e.sample.position_rad) && isnan(e.sample.velocity_rad_s));
    assert(BenchEncoder_Update(&e, 2, 10));
    assert(e.sample.counts == 5 && e.sample.direction == 1 && e.sample.timestamp_ms == 10);
    assert(!e.sample.valid && isnan(e.sample.position_rad));
    assert(BenchEncoder_Update(&e, 0xFFFFFFFE, 20));
    assert(e.sample.counts == 1 && e.sample.direction == -1 && e.sample.delta == -4);
    scenarios += 3;

    bench_encoder_config_t c = {100, 4, 2.0f, 1, BENCH_ENCODER_MOTOR};
    assert(BenchEncoder_Configure(&e, &c));
    assert(!e.sample.zeroed);
    assert(BenchEncoder_Zero(&e));
    assert(e.sample.zeroed && e.sample.zero_counts == 1);
    assert(BenchEncoder_Update(&e, 198, 120));
    near(e.sample.position_rad, 1.57079633f, 1.0e-5f);
    near(e.sample.velocity_rad_s, 15.7079633f, 1.0e-4f);
    assert(BenchEncoder_Zero(&e));
    near(e.sample.position_rad, 0, 0);
    near(e.sample.velocity_rad_s, 0, 0);
    scenarios += 3;

    c.location = BENCH_ENCODER_OUTPUT;
    assert(BenchEncoder_Configure(&e, &c));
    assert(BenchEncoder_Zero(&e));
    assert(BenchEncoder_Update(&e, 398, 220));
    near(e.sample.position_rad, 3.14159265f, 1.0e-5f);
    c.direction = -1;
    assert(BenchEncoder_Configure(&e, &c));
    assert(BenchEncoder_Zero(&e));
    assert(BenchEncoder_Update(&e, 598, 320));
    near(e.sample.position_rad, -3.14159265f, 1.0e-5f);
    scenarios += 2;

    c.ppr = 0; assert(!BenchEncoder_Configure(&e, &c));
    c.ppr = 100; c.gear_ratio = 0; assert(!BenchEncoder_Configure(&e, &c));
    c.gear_ratio = NAN; assert(!BenchEncoder_Configure(&e, &c));
    c.gear_ratio = 2; c.quadrature = 1; assert(!BenchEncoder_Configure(&e, &c));
    c.quadrature = 4; c.direction = 0; assert(!BenchEncoder_Configure(&e, &c));
    c.direction = 1; c.location = (bench_encoder_location_t)4; assert(!BenchEncoder_Configure(&e, &c));
    scenarios += 6;

    assert(!BenchEncoder_Update(&e, 600, 320)); /* Repeated timestamp. */
    assert(!e.sample.valid && isnan(e.sample.position_rad));
    BenchEncoder_Init(&e, true, 0, UINT32_MAX - 4U);
    assert(BenchEncoder_Update(&e, 1, 5));
    assert(e.sample.counts == 1 && e.sample.timestamp_ms == 5);
    BenchEncoder_Init(&e, false, 0, 0);
    assert(!BenchEncoder_Update(&e, 1, 10));
    assert(!BenchEncoder_Zero(&e));
    assert(!e.sample.valid && isnan(e.sample.position_rad));
    BenchEncoder_Init(&e, true, 0, 0);
    assert(!BenchEncoder_Update(&e, 0x80000000U, 10));
    assert(!e.sample.valid && e.numeric_fault);
    BenchEncoder_Init(&e, true, 0, 0);
    e.sample.counts = INT64_MAX;
    assert(!BenchEncoder_Update(&e, 1, 10));
    BenchEncoder_Init(&e, true, 0, 0);
    e.sample.counts = INT64_MIN;
    assert(!BenchEncoder_Update(&e, UINT32_MAX, 10));
    scenarios += 6;

    BenchEncoder_Init(&e, true, 0, 0);
    c = (bench_encoder_config_t){100, 4, 1, 1, BENCH_ENCODER_OUTPUT};
    assert(BenchEncoder_Configure(&e, &c));
    e.sample.counts = INT64_MAX - 10;
    assert(BenchEncoder_Zero(&e));
    assert(BenchEncoder_Update(&e, 1, 10));
    near(e.sample.position_rad, 0.0157079633f, 1e-7f);
    scenarios++;
}

static void pid_tests(void)
{
    motor_pid_t p;
    motor_pid_config_t c = {2, 0, 0, 5, 0.1f, 0.2f};
    float out;
    assert(MotorPID_Init(&p, &c));
    assert(MotorPID_Update(&p, 0, 0, 0.1f, 5, &out)); near(out, 0, 0);
    assert(MotorPID_Update(&p, 1, 0, 0.1f, 5, &out)); near(out, 2, 0);
    assert(MotorPID_Update(&p, -1, 0, 0.1f, 5, &out)); near(out, -2, 0);
    assert(MotorPID_Update(&p, 10, 0, 0.1f, 5, &out)); near(out, 5, 0);
    assert(MotorPID_Update(&p, -10, 0, 0.1f, 5, &out)); near(out, -5, 0);
    scenarios += 5;

    c.kp = 0; c.ki = 1; c.integral_limit = 0.15f;
    assert(MotorPID_Init(&p, &c));
    assert(MotorPID_Update(&p, 1, 0, 0.1f, 5, &out)); near(out, 0.1f, 1e-6f);
    assert(MotorPID_Update(&p, 1, 0, 0.1f, 5, &out)); near(out, 0.15f, 1e-6f);
    c.kp = 100; c.integral_limit = 5;
    assert(MotorPID_Init(&p, &c));
    for (int i = 0; i < 10; ++i) assert(MotorPID_Update(&p, 1, 0, 0.1f, 5, &out));
    near(p.integral, 0, 0); /* Saturated positive error cannot wind up. */
    assert(MotorPID_Update(&p, 0, 0, 0.1f, 5, &out)); near(out, 0, 0);
    scenarios += 3;

    c.kp = c.ki = 0; c.kd = 1;
    assert(MotorPID_Init(&p, &c));
    assert(MotorPID_Update(&p, 0, 0, 0.1f, 25, &out)); near(out, 0, 0);
    assert(MotorPID_Update(&p, 0, 1, 0.1f, 25, &out)); near(out, -5, 1e-5f);
    assert(MotorPID_Update(&p, 0, 1, 0.1f, 25, &out)); near(out, -2.5f, 1e-5f);
    MotorPID_Reset(&p);
    assert(!p.has_measurement); near(p.integral, 0, 0);
    assert(MotorPID_Update(&p, 100, 1, 0.1f, 25, &out)); near(out, 0, 0); /* No target derivative kick. */
    scenarios += 3;

    assert(!MotorPID_Update(&p, NAN, 0, 0.1f, 5, &out)); near(out, 0, 0);
    assert(!MotorPID_Update(&p, 0, 0, 0.21f, 5, &out)); near(out, 0, 0);
    assert(!p.has_measurement);
    assert(!MotorPID_Update(&p, 0, 0, 0, 5, &out));
    assert(!MotorPID_Update(&p, 0, 0, 0.1f, 101, &out));
    c.kp = 3.0e38f; c.ki = 0; c.kd = 0;
    assert(MotorPID_Init(&p, &c));
    assert(!MotorPID_Update(&p, 3.0e38f, 0, 0.1f, 5, &out)); near(out, 0, 0);
    c.kp = NAN; assert(!MotorPID_Init(&p, &c));
    scenarios += 6;
}

static void init(void)
{
    gate = false; applied = 0; async_fault = 0;
    Bench_Init(0, 0, true);
}
static void configure(void)
{
    bench_encoder_config_t c = {1000, 4, 1, 1, BENCH_ENCODER_OUTPUT};
    bench_limits_t l = {-1, 1, 0.2f, 10, 2000};
    bench_safety_config_t safe = {10000, 1, 0.002f, 100, 0.05f};
    motor_pid_config_t p = {20, 0, 0, 5, 0.02f, 0.05f};
    assert(Bench_Calibrate(&c));
    assert(!Bench_VerifyEncoder()); /* Calibration alone is not verified wiring. */
    Bench_Update(1, 10); /* Operator manually observes a count after calibration. */
    assert(Bench_Zero());
    assert(Bench_VerifyEncoder());
    assert(Bench_SetLimits(&l));
    assert(Bench_SetSafety(&safe));
    assert(Bench_SetPID(&p, 5, 0.002f));
    assert(Bench_AckWiring());
    Bench_Ping(10);
}
static void ready(void) { init(); configure(); assert(Bench_Arm(false)); }
static void fault_is(bench_fault_t f)
{
    assert(Bench_Status()->fault == f);
    assert(Bench_Status()->state == BENCH_FAULT && !Bench_Status()->armed);
    assert(!gate); near(applied, 0, 0);
    assert(!Bench_Move(0.01f));
    assert(!Bench_Arm(true));
}
static void core_tests(void)
{
    init(); assert(!gate); near(applied, 0, 0);
    assert(!Bench_Status()->armed && !Bench_Status()->encoder.valid);
    assert(!Bench_Move(0.1f) && !Bench_Jog(0.1f) && !Bench_Pulse(1, 100));
    Bench_Ping(0); assert(!Bench_Arm(true));
    assert(Bench_Status()->state == BENCH_DISARMED);
    scenarios += 3;

    init(); configure();
    Bench_Disarm();
    assert(Bench_Zero()); assert(!Bench_Status()->limits_configured);
    assert(!Bench_Arm(true)); /* Zero invalidates position-limit coordinate. */
    scenarios++;

    ready(); assert(Bench_Move(0.1f));
    Bench_Update(1, 20); near(applied, 2, 1e-5f);
    motor_pid_config_t p = {20, 0, 0, 5, 0.02f, 0.05f};
    assert(!Bench_SetPID(&p, 25, 0.002f)); /* Configuration is locked while armed. */
    Bench_Disarm(); assert(!gate); near(applied, 0, 0);
    assert(!Bench_SetPID(&p, 25.1f, 0.002f));
    assert(!Bench_SetPID(&p, NAN, 0.002f));
    assert(!Bench_Move(0.1f));
    scenarios += 5;

    ready(); assert(!Bench_Move(0.21f)); assert(!Bench_Move(NAN)); assert(!Bench_Jog(-0.3f));
    assert(!Bench_Pulse(6, 100)); assert(!Bench_Pulse(2, 501)); near(applied, 0, 0);
    scenarios += 5;

    ready(); assert(Bench_Move(0.05f));
    Bench_Update(33, 20); /* 32 counts = 0.05026548 rad; not settled while moving. */
    assert(Bench_Status()->state == BENCH_MOVING);
    for (uint32_t t = 30; t <= 130; t += 10) Bench_Update(33, t);
    assert(Bench_Status()->state == BENCH_DISARMED && Bench_Status()->target_reached);
    near(Bench_Status()->target_rad, 0.05f, 1e-6f);
    near(applied, 0, 0);
    Bench_Stop(); assert(!Bench_Status()->target_reached);
    scenarios += 2;

    ready(); assert(Bench_Move(-0.05f)); Bench_Update(1U - 32U, 20);
    assert(Bench_Status()->encoder.position_rad < 0);
    assert(Bench_Status()->pid_output < 0.01f); /* Real signed feedback used. */
    Bench_Disarm(); scenarios++;

    ready(); assert(Bench_Move(0.1f));
    for (uint32_t t = 20; t <= 120; t += 10) Bench_Update(1, t);
    fault_is(BENCH_FAULT_STALL); scenarios++;
    assert(Bench_ClearFault()); assert(!gate && !Bench_Status()->target_reached);
    assert(!Bench_Status()->encoder_verified && !Bench_Status()->encoder.zeroed);
    assert(!Bench_Status()->limits_configured); assert(!Bench_Arm(true)); scenarios++;

    ready(); /* Deliberately stationary armed encoder is NOT a stall. */
    for (uint32_t t = 20; t <= 500; t += 10) Bench_Update(1, t);
    assert(Bench_Status()->state == BENCH_ARMED && Bench_Status()->fault == BENCH_FAULT_NONE);
    Bench_Update(1, 510); assert(Bench_Status()->state == BENCH_ARMED);
    Bench_Update(1, 520); fault_is(BENCH_FAULT_LINK); scenarios += 2;

    ready(); Bench_Update(1, 70); fault_is(BENCH_FAULT_LOOP); scenarios++;
    ready(); Bench_SetEncoderAvailable(false); fault_is(BENCH_FAULT_ENCODER); scenarios++;
    ready(); assert(Bench_Move(0.1f)); Bench_Update(1, 20);
    Bench_EStop(); fault_is(BENCH_FAULT_ESTOP); scenarios++;
    ready(); async_fault = 1U; gate = false; applied = 0; Bench_Update(1, 20);
    fault_is(BENCH_FAULT_ESTOP); scenarios++;
    ready(); async_fault = 32U; gate = false; Bench_Update(1, 20);
    fault_is(BENCH_FAULT_ENCODER); scenarios++;

    init(); configure();
    bench_safety_config_t safe = Bench_Status()->safety;
    safe.max_count_delta = 3;
    assert(Bench_SetSafety(&safe)); assert(Bench_Arm(true));
    Bench_Update(5, 20); fault_is(BENCH_FAULT_DELTA); scenarios++;

    init(); configure();
    bench_limits_t l = Bench_Status()->limits;
    l.max_velocity_rad_s = 0.1f;
    assert(Bench_SetLimits(&l)); assert(Bench_Arm(true));
    Bench_Update(2, 20); fault_is(BENCH_FAULT_VELOCITY); scenarios++;

    init(); configure(); l = Bench_Status()->limits;
    l.min_rad = -0.01f; l.max_rad = 0.01f; l.max_step_rad = 0.005f;
    assert(Bench_SetLimits(&l)); assert(Bench_Arm(true));
    Bench_Update(20, 20); fault_is(BENCH_FAULT_POSITION); scenarios++;

    init(); configure(); l = Bench_Status()->limits; l.max_move_ms = 100;
    assert(Bench_SetLimits(&l)); p.kp = 1;
    assert(Bench_SetPID(&p, 5, 0.002f)); assert(Bench_Arm(true));
    assert(Bench_Move(0.1f));
    for (uint32_t t = 20; t <= 110; t += 10) Bench_Update(1, t);
    fault_is(BENCH_FAULT_MOTION_TIMEOUT); scenarios++;

    ready(); assert(Bench_Pulse(2, 100));
    Bench_Update(1, 20); near(applied, 2, 0);
    for (uint32_t t = 30; t <= 110; t += 10) Bench_Update(1, t);
    assert(Bench_Status()->state == BENCH_DISARMED && !Bench_Status()->target_reached);
    near(applied, 0, 0); scenarios++;

    init(); configure(); p.kp = 0; p.ki = 5;
    assert(Bench_SetPID(&p, 5, 0.002f)); assert(Bench_Arm(true));
    assert(Bench_Move(0.1f)); Bench_Update(1, 20); near(applied, 0.005f, 1e-6f);
    Bench_Disarm(); assert(Bench_Arm(false)); assert(Bench_Move(0.1f));
    Bench_Update(1, 30); near(applied, 0.005f, 1e-6f); scenarios++;

    init(); configure(); Bench_Ping(19); /* Ping received between two control samples. */
    assert(Bench_Arm(true)); scenarios++;

    init(); configure();
    Bench_SetEncoderAvailable(false); Bench_SetEncoderAvailable(true); Bench_Update(2, 20);
    assert(!Bench_Status()->encoder.zeroed && !Bench_Status()->limits_configured);
    assert(!Bench_VerifyEncoder()); /* Lost acquisition invalidates the relative origin. */
    assert(!Bench_Arm(true)); scenarios++;
}

int main(void)
{
    encoder_tests(); pid_tests(); core_tests();
    printf("PASS: %u encoder/PID/control/safety software scenarios (hardware mock).\n", scenarios);
    return 0;
}
