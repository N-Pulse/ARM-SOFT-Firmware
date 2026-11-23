#include "wrist.h"
#include "wrist_protocol.h"
#include "bench_hw.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick, raw[2], async_fault;
static float pwm[2];
static bool gate, running[2] = {true, true};
static char packets[20000];
static size_t packet_len;
uint32_t BenchHW_Counter(unsigned i) { return raw[i]; }
bool BenchHW_EncoderRunning(unsigned i) { return running[i]; }
bool BenchHW_Arm(uint32_t now) { (void)now; gate = true; return true; }
void BenchHW_Disable(void) { gate = false; pwm[0] = pwm[1] = 0; }
void BenchHW_SetMotors(float a, float b) { assert(gate); pwm[0] = a; pwm[1] = b; }
float BenchHW_AppliedPWM(unsigned i) { return pwm[i]; }
void BenchHW_Heartbeat(uint32_t now) { (void)now; }
void BenchHW_ControlAlive(uint32_t now) { (void)now; }
uint32_t BenchHW_TakeAsyncFault(void) { uint32_t r = async_fault; async_fault = 0; return r; }
bool BenchHW_Write(const char *s, size_t n)
{
    assert(packet_len+n < sizeof packets);
    memcpy(packets+packet_len, s, n); packet_len += n; packets[packet_len] = 0;
    return true;
}
static void step(bool ping)
{ tick += 10; if (ping) Wrist_Ping(tick); Wrist_Update(tick); }
static void command(const char *s)
{ while (*s) Protocol_Byte((uint8_t)*s++, tick); Protocol_Byte('\n', tick); }
static void ready(void)
{
    tick = raw[0] = raw[1] = 0; running[0] = running[1] = true; async_fault = 0;
    Wrist_Init(0); step(true);
    assert(!Wrist_Arm());
    bench_encoder_config_t c = {90, 4, 1, 1, BENCH_ENCODER_MOTOR}; /* 1 degree/count */
    assert(Wrist_Calibrate(0, &c)); assert(Wrist_Calibrate(1, &c)); assert(Wrist_Zero());
    wrist_limits_t l = {-90, 90, 45, 10000, 1000};
    motor_pid_config_t p = {1, 1, 0, 5, .02f, .05f};
    wrist_safety_t safety = {20000, 1, 1, 100, 5, 2, 50};
    for (unsigned i = 0; i < 2; ++i) {
        assert(Wrist_Limits(i, &l)); assert(Wrist_PID(i, &p, 5, .5f)); assert(Wrist_Safety(i, &safety));
    }
    assert(Wrist_Arm());
}
static void holding_and_atomicity(void)
{
    ready(); assert(pwm[0] == 0 && pwm[1] == 0);
    assert(!Wrist_Move(10, 100)); assert(!Wrist_Status()->active);
    assert(Wrist_Move(10, -10)); raw[0] = 10; raw[1] = 65526;
    for (int j = 0; j < 15; ++j) step(true);
    assert(Wrist_Status()->holding && Wrist_Status()->armed && Wrist_Status()->reached);
    assert(fabsf(Wrist_Status()->motors[1].position+10) < .001f);
    for (int j = 0; j < 200; ++j) step(true); /* Holding outlives movement timeout. */
    assert(!Wrist_Status()->fault);
    assert(Wrist_Move(10.25f, -10.25f));
    for (int j = 0; j < 100; ++j) step(true);
    assert(Wrist_Status()->holding && pwm[0] > .1f && pwm[1] < -.1f);
    assert(!Wrist_Status()->fault); /* Stationary support torque is not a stall. */
    assert(!Wrist_Zero());
    raw[0] += 3; step(true);
    for (int j = 0; j < 6; ++j) step(true);
    assert(Wrist_Status()->fault && !strcmp(Wrist_Status()->fault, "hold_error"));
    assert(!gate && pwm[0] == 0 && pwm[1] == 0);
    assert(Wrist_ClearFault()); assert(!Wrist_Arm());
    assert(!Wrist_Status()->motors[0].encoder.sample.zeroed && !Wrist_Status()->motors[1].limits_set);
}
static void fault_paths(void)
{
    ready(); assert(Wrist_Move(10, -10));
    for (int j = 0; j < 15; ++j) step(true);
    assert(Wrist_Status()->fault && !strcmp(Wrist_Status()->fault, "stall")); assert(!gate);
    ready(); running[1] = false; step(true);
    assert(Wrist_Status()->fault_motor == 2 && !gate);
    ready(); raw[1] = 32768; step(true);
    assert(Wrist_Status()->fault && !gate); /* Ambiguous half-range rejected. */
    ready(); raw[1] = 65535; step(true); assert(!Wrist_Status()->fault);
    assert(fabsf(Wrist_Status()->motors[1].position+1) < .001f);
    raw[1] = 0; step(true); assert(fabsf(Wrist_Status()->motors[1].position) < .001f);
    ready(); for (int j = 0; j < 51; ++j) step(false);
    assert(Wrist_Status()->fault && !strcmp(Wrist_Status()->fault, "communication_timeout"));
    ready(); async_fault = BENCH_ASYNC_ESTOP; step(true); assert(!gate && Wrist_Status()->fault);
    ready(); tick += 60; Wrist_Update(tick); assert(!gate && !strcmp(Wrist_Status()->fault, "loop_timeout"));
    ready(); Wrist_Stop();
    bench_encoder_config_t c = {100, 4, 203, -1, BENCH_ENCODER_MOTOR};
    assert(Wrist_Calibrate(0, &c));
    assert(!Wrist_Status()->motors[0].encoder.sample.zeroed && !Wrist_Status()->motors[1].encoder.sample.zeroed);
    assert(!Wrist_Status()->motors[1].limits_set);
}
static void resolution_boundary(void)
{
    ready(); /* Keep the original 90 PPR / x4 / 1:1 / stall_deg=1 fixture. */
    for (unsigned i = 0; i < 2; ++i) {
        Wrist_Stop();
        wrist_safety_t safety = Wrist_Status()->motors[i].safety;
        safety.stall_deg = nextafterf(1.0f, 0.0f); /* Actually below one count. */
        assert(Wrist_Safety(i, &safety));
        assert(!Wrist_Arm());
        assert(!strcmp(Wrist_Error(), "safety_incompatible_with_pid_or_resolution"));
        assert(!gate && !Wrist_Status()->armed);
        safety.stall_deg = 1;
        assert(Wrist_Safety(i, &safety)); assert(Wrist_Arm());
    }
}
static void assert_cutoff(const char *why, unsigned motor)
{
    const wrist_status_t *status = Wrist_Status();
    assert(status->fault && !strcmp(status->fault, why));
    assert(status->fault_motor == (int)motor+1);
    assert(!status->armed && !status->active && !gate);
    assert(pwm[0] == 0 && pwm[1] == 0);
    assert(status->motors[0].applied == 0 && status->motors[1].applied == 0);
}
/* 0: identical; 1: jitter; 2: only the other motor; 3: incremental changes.
 * Real controller/PID/encoders, mocked stationary counters and valid heartbeat. */
static void stall_stream(unsigned mode, unsigned blocked)
{
    ready();
    float targets[2] = {0, 0}; targets[blocked] = 10;
    assert(Wrist_Move(targets[0], targets[1]));
    uint32_t begin = tick;
    for (unsigned j = 0; j < 50 && !Wrist_Status()->fault; ++j) {
        if (mode == 1) targets[blocked] = 10 + (j % 2) * .01f;
        if (mode == 2) targets[1-blocked] = (j % 2) * 2.0f;
        if (mode == 3) targets[blocked] += .01f;
        assert(Wrist_Move(targets[0], targets[1]));
        step(true);
        if (!Wrist_Status()->fault) {
            assert(Wrist_Status()->armed && gate);
            assert(fabsf(Wrist_Status()->motors[blocked].error) > .5f);
            assert(pwm[blocked] > Wrist_Status()->motors[blocked].safety.stall_pwm);
            assert(raw[blocked] == 0);
        }
    }
    printf("stall stream mode=%u M%u: elapsed=%lu ms, armed=%d, fault=%s\n",
           mode, blocked+1, (unsigned long)(tick-begin), Wrist_Status()->armed,
           Wrist_Status()->fault ? Wrist_Status()->fault : "none");
    fflush(stdout);
    assert_cutoff("stall", blocked);
    /* First output at +10 ms, first applied-PWM observation at +20 ms. */
    assert(tick-begin == 120);
}
static void repeated_stall(void) { stall_stream(0, 0); stall_stream(0, 1); }
static void jitter_stall(void) { stall_stream(1, 0); stall_stream(1, 1); }
static void other_motor_stall(void) { stall_stream(2, 0); stall_stream(2, 1); }
static void incremental_stall(void) { stall_stream(3, 0); stall_stream(3, 1); }
static void progress_then_stall(void)
{
    for (unsigned motor = 0; motor < 2; ++motor) {
        ready(); assert(Wrist_Move(motor == 0 ? 10 : 0, motor == 1 ? 10 : 0));
        uint32_t begin = tick;
        for (unsigned j = 1; j <= 12; ++j) {
            if (j % 4 == 0) ++raw[motor];
            step(true);
            assert(Wrist_Status()->armed && !Wrist_Status()->fault);
        }
        /* Progress renewed the measurement window. Now immobilize the motor. */
        for (unsigned j = 0; j < 15 && !Wrist_Status()->fault; ++j) step(true);
        assert_cutoff("stall", motor);
        assert(tick-begin == 220);
    }
}
static void repeated_pid_and_settling(void)
{
    ready(); assert(Wrist_Move(2, 0)); step(true); step(true);
    motor_pid_t before = Wrist_Status()->motors[0].pid;
    assert(before.has_measurement && before.integral > 0);
    assert(Wrist_Move(2, 0));
    const motor_pid_t *after = &Wrist_Status()->motors[0].pid;
    assert(after->integral == before.integral && after->has_measurement);
    assert(after->previous_position == before.previous_position);
    assert(after->filtered_velocity == before.filtered_velocity);
    /* Changing M2 also preserves the running M1 PID. */
    assert(Wrist_Move(2, .1f));
    assert(after->integral == before.integral && after->has_measurement);
    raw[0] = 2;
    for (unsigned j = 0; j < 15; ++j) { assert(Wrist_Move(2, .1f)); step(true); }
    assert(Wrist_Status()->holding && Wrist_Status()->reached);
    for (unsigned j = 0; j < 120; ++j) { assert(Wrist_Move(2, .1f)); step(true); }
    assert(Wrist_Status()->armed && Wrist_Status()->holding && !Wrist_Status()->fault);
    assert(pwm[0] > 0 && pwm[1] > 0); /* Stationary support torque allowed. */
}
static void timeout_stream(void)
{
    for (unsigned mode = 0; mode < 3; ++mode) for (unsigned blocked = 0; blocked < 2; ++blocked) {
        ready(); Wrist_Stop();
        for (unsigned i = 0; i < 2; ++i) {
            wrist_limits_t l = Wrist_Status()->motors[i].limits; l.move_ms = 200;
            motor_pid_config_t p = {.01f, 0, 0, 0, .02f, .05f};
            assert(Wrist_Limits(i, &l)); assert(Wrist_PID(i, &p, 5, .5f));
        }
        assert(Wrist_Arm());
        float targets[2] = {0, 0}; targets[blocked] = 10;
        assert(Wrist_Move(targets[0], targets[1]));
        uint32_t begin = tick;
        for (unsigned j = 0; j < 30 && !Wrist_Status()->fault; ++j) {
            if (mode == 1) targets[blocked] = 10 + (j % 2) * .01f;
            if (mode == 2) targets[1-blocked] = (j % 2) * .1f;
            assert(Wrist_Move(targets[0], targets[1])); step(true);
        }
        assert_cutoff("motion_timeout", blocked);
        assert(tick-begin == 200);
    }
}
static void hold_stream(void)
{
    for (unsigned mode = 0; mode < 3; ++mode) for (unsigned blocked = 0; blocked < 2; ++blocked) {
        ready(); assert(Wrist_Move(0, 0));
        for (unsigned j = 0; j < 15; ++j) step(true);
        assert(Wrist_Status()->holding);
        raw[blocked] = 3; step(true); /* Begin a measured loss of holding. */
        uint32_t begin = tick;
        float targets[2] = {0, 0};
        for (unsigned j = 0; j < 15 && !Wrist_Status()->fault; ++j) {
            if (mode == 1) targets[blocked] = (j % 2) * .01f;
            if (mode == 2) targets[1-blocked] = 2 + (j % 2) * .01f;
            assert(Wrist_Move(targets[0], targets[1])); step(true);
        }
        assert_cutoff("hold_error", blocked);
        assert(tick-begin == 50);
    }
}
static void legitimate_targets(void)
{
    ready(); assert(Wrist_Move(2, -2)); raw[0] = 2; raw[1] = 65534;
    for (unsigned j = 0; j < 15; ++j) step(true);
    assert(Wrist_Status()->holding);
    for (unsigned j = 0; j < 120; ++j) step(true);
    /* An established target permits a fresh movement, after the old deadline. */
    assert(Wrist_Move(4, -2)); raw[0] = 4;
    for (unsigned j = 0; j < 15; ++j) step(true);
    assert(Wrist_Status()->armed && Wrist_Status()->holding && Wrist_Status()->reached);
    assert(Wrist_Move(4, -4)); raw[1] = 65532;
    for (unsigned j = 0; j < 15; ++j) step(true);
    assert(Wrist_Status()->armed && Wrist_Status()->holding && Wrist_Status()->reached);
    /* A new target can also be accepted during a movement that is progressing. */
    assert(Wrist_Move(6, -4)); raw[0] = 5; step(true);
    assert(Wrist_Move(7, -4)); raw[0] = 7;
    for (unsigned j = 0; j < 15; ++j) step(true);
    assert(Wrist_Status()->armed && Wrist_Status()->holding && !Wrist_Status()->fault);
}
static void protocol(void)
{
    ready(); Protocol_Init(tick);
    command("move flex 10"); assert(fabsf(Wrist_Status()->motors[0].target-10) < .001f);
    assert(fabsf(Wrist_Status()->motors[1].target+10) < .001f);
    command("move rotation 5"); assert(Wrist_Status()->motors[0].target == 15 && Wrist_Status()->motors[1].target == -5);
    command("move wrist 2 3"); assert(Wrist_Status()->motors[0].target == 5 && Wrist_Status()->motors[1].target == 1);
    command("jog motor 2 -1"); assert(Wrist_Status()->motors[1].target == 0);
    command("move motors 2 nan"); assert(Wrist_Status()->motors[0].target == 5);
    command("move motor 3 2"); assert(Wrist_Status()->motors[0].target == 5);
    for (int j = 0; j < 240; ++j) Protocol_Byte('x', tick);
    command("move motors 0 0"); assert(Wrist_Status()->motors[0].target == 5);
    Protocol_Byte('m', tick); tick += 251; command("ove motors 0 0");
    assert(Wrist_Status()->motors[0].target == 5);
    command("stop"); assert(!gate);
    command("status");
    command("calibrate 1 90 4 1 1 motor"); command("calibrate 2 90 4 1 1 motor");
    command("zero"); command("limits 1 -90 90 45 10000 1000");
    command("pid 1 1 0 0 0 .02 5 .5"); command("safety 1 20000 1 1 100 5 2 50");
    command("status"); command("stream on 20"); Protocol_Service(tick+100);
    command("estop"); command("status");
    fputs(packets, stdout);
}
int main(int argc, char **argv)
{
    const struct { const char *name; void (*run)(void); } tests[] = {
        {"resolution_boundary", resolution_boundary},
        {"repeated_stall", repeated_stall}, {"jitter_stall", jitter_stall},
        {"other_motor_stall", other_motor_stall}, {"incremental_stall", incremental_stall},
        {"progress_then_stall", progress_then_stall},
        {"repeated_pid_and_settling", repeated_pid_and_settling},
        {"timeout_stream", timeout_stream}, {"hold_stream", hold_stream},
        {"legitimate_targets", legitimate_targets},
        {"holding_and_atomicity", holding_and_atomicity}, {"fault_paths", fault_paths}, {"protocol", protocol}
    };
    bool found = false;
    for (unsigned i = 0; i < sizeof tests / sizeof tests[0]; ++i) {
        if (argc > 1 && strcmp(argv[1], tests[i].name)) continue;
        tests[i].run(); found = true;
        printf("%s: passed\n", tests[i].name);
    }
    assert(found);
    puts("Dual controller and protocol software tests passed.");
    return 0;
}
