#include "bench.h"
#include "bench_hw.h"
#include "bench_protocol.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static char journal[131072];
static size_t journal_length;
static bool writable = true, gate;
static float motor;
static uint32_t now_ms, raw;

bool BenchHW_Write(const char *data, size_t length)
{
    if (!writable) return false;
    assert(length < 2048U);
    assert(journal_length + length < sizeof journal);
    memcpy(journal + journal_length, data, length);
    journal_length += length;
    journal[journal_length] = '\0';
    return true;
}
bool BenchHW_Arm(uint32_t now) { (void)now; gate = true; return true; }
void BenchHW_Disable(void) { gate = false; motor = 0.0f; }
void BenchHW_SetMotor(float percent) { motor = gate ? percent : 0.0f; }
float BenchHW_AppliedPWM(void) { return motor; }
void BenchHW_Heartbeat(uint32_t now) { (void)now; }
void BenchHW_ControlAlive(uint32_t now) { (void)now; }
uint32_t BenchHW_TakeAsyncFault(void) { return 0U; }

static void bytes(const char *text)
{
    for (size_t i = 0U; text[i]; ++i) BenchProtocol_Byte((uint8_t)text[i], now_ms);
}
static void send(const char *text) { bytes(text); BenchProtocol_Byte('\n', now_ms); }
static void reset(void)
{
    now_ms = raw = 0U;
    writable = true;
    Bench_Init(raw, now_ms, true);
    BenchProtocol_Init(now_ms);
}
static void prepare(void)
{
    reset();
    send("calibrate 100 4 1 1 output");
    send("zero");
    raw = 1U; now_ms = 10U; Bench_Update(raw, now_ms);
    send("verify encoder");
    send("limits -1 1 0.1 10 1000");
    send("safety 1000 1 0.02 100 0.1");
    send("pid 1 0 0 1 0.02 5 0.005");
    send("ack wiring");
    send("ping");
    assert(Bench_Status()->encoder.valid);
    assert(Bench_Status()->limits_configured);
    assert(Bench_Status()->pid_configured);
    assert(Bench_Status()->safety_configured);
}

static void test_framing_and_numeric(void)
{
    reset();
    send("status");
    assert(strstr(journal, "\"target_rad\":null"));
    assert(strstr(journal, "\"position_rad\":null"));
    send("arm");
    send("move 0.01");
    assert(!Bench_Status()->armed && motor == 0.0f);
    const char *bad[] = {"calibrate 0 4 1 1 motor", "calibrate 100 4 nan 1 motor",
        "calibrate 100 4 inf 1 motor", "calibrate 100 4 0x1p0 1 motor",
        "calibrate 4294967296 4 1 1 motor", "calibrate -1 4 1 1 motor",
        "calibrate 100 4 1 1 motor excess", "calibrate 100 4 1e999 1 motor",
        "calibrate 100 4 1x 1 motor", "jog NaN", "jog +", "jog .", "jog 1e"};
    for (unsigned i = 0U; i < sizeof bad / sizeof bad[0]; ++i) {
        send(bad[i]);
        assert(!Bench_Status()->encoder.calibrated);
        assert(!Bench_Status()->armed && motor == 0.0f);
    }
    send("calibrate 100 4 2.0271657754e2 -1 motor");
    assert(Bench_Status()->encoder.calibrated);
    assert(Bench_Status()->calibration.direction == -1);
    assert(fabsf(Bench_Status()->calibration.gear_ratio - 202.71657754f) < 0.001f);
    assert(!Bench_Status()->encoder.valid);
    send("zero");
    send("encoder");
    assert(strstr(journal, "\"configuration\""));
    send("help");
    send("stream on 21");
    send("stream on 20");
    now_ms = 100U; Bench_Update(0U, now_ms); BenchProtocol_Service(now_ms);
    assert(strstr(journal, "\"type\":\"telemetry\""));
    send("stream off");
}

static void test_timeout_overflow_and_control(void)
{
    prepare();
    bytes("ar");
    now_ms = 300U; BenchProtocol_Service(now_ms);
    send("m");
    assert(!Bench_Status()->armed);
    for (unsigned i = 0U; i < 300U; ++i) BenchProtocol_Byte('a', now_ms);
    send("arm"); /* Tail of the overlong line must remain discarded. */
    assert(!Bench_Status()->armed);
    bytes("arm"); BenchProtocol_Byte(0U, now_ms); BenchProtocol_Byte('\n', now_ms);
    assert(!Bench_Status()->armed);
    bytes("ar\rm\n");
    assert(!Bench_Status()->armed);
    now_ms = 310U; Bench_Update(raw, now_ms); send("ping");
    bytes("arm\r\n");
    assert(Bench_Status()->armed && motor == 0.0f);
    send("move 0.05");
    now_ms = 320U; Bench_Update(raw, now_ms);
    assert(Bench_Status()->state == BENCH_MOVING && motor > 0.0f);
    float target = Bench_Status()->target_rad;
    send("move nan"); send("move 0.09 trailing"); send("jog 100");
    assert(Bench_Status()->target_rad == target);
    send("pid 100 0 0 1 0.02 25 .005");
    assert(Bench_Status()->pwm_limit == 5.0f);
    send("stop");
    assert(!Bench_Status()->armed && motor == 0.0f);
    send("jog .01"); assert(!Bench_Status()->armed && motor == 0.0f);
    send("arm"); send("jog .01");
    now_ms = 330U; Bench_Update(raw, now_ms); assert(motor > 0.0f);
    bytes("move 0."); BenchProtocol_Byte('!', now_ms);
    assert(Bench_Status()->state == BENCH_FAULT && motor == 0.0f);
    send("arm"); /* Remainder discarded after priority stop. */
    BenchProtocol_Service(now_ms);
    assert(strstr(journal, "\"command\":\"fault\",\"ok\":false,\"message\":\"estop\""));
    send("clear_fault");
    assert(Bench_Status()->state == BENCH_DISARMED && motor == 0.0f);
    send("arm"); assert(!Bench_Status()->armed);
}

static void test_tx_backpressure(void)
{
    prepare();
    send("arm"); send("jog .01");
    now_ms = 20U; Bench_Update(raw, now_ms); assert(motor > 0.0f);
    writable = false;
    for (unsigned i = 0U; i < 8U; ++i) send("status");
    assert(!Bench_Status()->armed && motor == 0.0f);
    send("estop"); assert(Bench_Status()->fault == BENCH_FAULT_ESTOP);
    writable = true; BenchProtocol_Service(now_ms);
    send("status");
}

int main(void)
{
    test_framing_and_numeric();
    test_timeout_overflow_and_control();
    test_tx_backpressure();
    assert(fwrite(journal, 1U, journal_length, stdout) == journal_length);
    return 0;
}
