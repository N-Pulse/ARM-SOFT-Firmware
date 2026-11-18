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
int main(void)
{
    holding_and_atomicity(); fault_paths(); protocol();
    puts("Dual controller and protocol software tests passed.");
    return 0;
}
