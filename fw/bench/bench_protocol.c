#include "bench_protocol.h"
#include "bench.h"
#include "bench_hw.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 192U
#define PACKET_SIZE 2048U
#define REPLY_SLOTS 4U
#define LINE_TIMEOUT_MS 250U
#define MAX_STREAM_HZ 20U

static char line[LINE_SIZE];
static size_t line_len;
static bool discard, saw_cr;
static uint32_t line_started, last_stream;
static uint32_t stream_period = 50U, telemetry_dropped;
static uint32_t bandwidth_period = 50U;
static bool streaming;
static bench_fault_t reported_fault;
static char replies[REPLY_SLOTS][PACKET_SIZE];
static size_t reply_lengths[REPLY_SLOTS];
static unsigned reply_head, reply_count;

static void flush_replies(void)
{
    while (reply_count && BenchHW_Write(replies[reply_head], reply_lengths[reply_head])) {
        reply_head = (reply_head + 1U) % REPLY_SLOTS;
        --reply_count;
    }
}

static bool queue_packet(const char *packet)
{
    size_t n = strlen(packet);
    if (n >= PACKET_SIZE || reply_count == REPLY_SLOTS) return false;
    unsigned slot = (reply_head + reply_count) % REPLY_SLOTS;
    memcpy(replies[slot], packet, n + 1U);
    reply_lengths[slot] = n;
    ++reply_count;
    flush_replies();
    return true;
}

/* All command and message values here are constant ASCII, never raw input. */
static bool reply(const char *command, bool ok, const char *message)
{
    char packet[384];
    int n = snprintf(packet, sizeof packet,
        "{\"type\":\"reply\",\"command\":\"%s\",\"ok\":%s,\"message\":\"%s\"}\n",
        command, ok ? "true" : "false", message);
    return n > 0 && (size_t)n < sizeof packet && queue_packet(packet);
}

static const char *json_bool(bool value) { return value ? "true" : "false"; }
static void number(char *text, size_t size, float value, bool valid)
{
    if (!valid || !isfinite(value)) { (void)snprintf(text, size, "null"); return; }
    (void)snprintf(text, size, "%.9g", (double)value);
}

/* Avoid relying on the embedded nano printf implementation's 64-bit modifier. */
static void count_string(char *text, int64_t value)
{
    char reverse[24];
    unsigned n = 0U, out = 0U;
    uint64_t magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
    do { reverse[n++] = (char)('0' + magnitude % 10U); magnitude /= 10U; } while (magnitude);
    if (value < 0) text[out++] = '-';
    while (n) text[out++] = reverse[--n];
    text[out] = '\0';
}

static bool snapshot(const char *type, bool config)
{
    const bench_status_t *s = Bench_Status();
    char packet[PACKET_SIZE], target[32], position[32], velocity[32], error[32], counts[24], zero[24];
    bool scaled = s->encoder.available && s->encoder.calibrated;
    number(target, sizeof target, s->target_rad, true);
    /* Derived values can be inspected during passive calibration. They do not
     * authorize motion: feedback_valid additionally requires operator verify. */
    number(position, sizeof position, s->encoder.position_rad, scaled);
    number(velocity, sizeof velocity, s->encoder.velocity_rad_s, scaled);
    number(error, sizeof error, s->error_rad, scaled);
    count_string(counts, s->encoder.counts);
    count_string(zero, s->encoder.zero_counts);
    int n = snprintf(packet, sizeof packet,
        "{\"type\":\"%s\",\"t_ms\":%lu,"
        "\"target_rad\":%s,\"position_rad\":%s,\"error_rad\":%s,\"velocity_rad_s\":%s,"
        "\"pid_percent\":%.9g,\"pwm_percent\":%.9g,\"raw\":%lu,\"counts\":%s,"
        "\"delta\":%ld,\"direction\":%d,\"encoder_t_ms\":%lu,"
        "\"dt_s\":%.9g,\"state\":\"%s\",\"armed\":%s,\"encoder_available\":%s,"
        "\"calibrated\":%s,\"feedback_valid\":%s,\"target_reached\":%s,\"fault\":\"%s\",\"telemetry_dropped\":%lu",
        type, (unsigned long)s->timestamp_ms, target, position, error, velocity,
        (double)s->pid_output, (double)s->applied_pwm, (unsigned long)s->encoder.raw, counts,
        (long)s->encoder.delta, s->encoder.direction, (unsigned long)s->encoder.timestamp_ms,
        (double)s->dt_s, Bench_StateName(), json_bool(s->armed), json_bool(s->encoder.available),
        json_bool(s->encoder.calibrated), json_bool(s->encoder.valid), json_bool(s->target_reached),
        Bench_FaultName(), (unsigned long)telemetry_dropped);
    if (n < 0 || (size_t)n >= sizeof packet) return false;
    size_t used = (size_t)n;
    if (config) {
        n = snprintf(packet + used, sizeof packet - used,
            ",\"firmware\":\"stm32-motor-bench/1\",\"zero_counts\":%s,\"zeroed\":%s,"
            "\"encoder_verified\":%s,\"wiring_ack\":%s,\"session_live\":%s,"
            "\"configuration\":{\"ppr\":%lu,\"quadrature\":%lu,\"gear_ratio\":%.9g,"
            "\"encoder_sign\":%d,\"encoder_location\":\"%s\",\"limits_configured\":%s,"
            "\"min_rad\":%.9g,\"max_rad\":%.9g,\"max_step_rad\":%.9g,\"max_velocity_rad_s\":%.9g,"
            "\"max_move_ms\":%lu,\"pid_configured\":%s,\"kp\":%.9g,\"ki\":%.9g,\"kd\":%.9g,"
            "\"integral_limit\":%.9g,\"derivative_tau_s\":%.9g,\"max_pwm_percent\":%.9g,"
            "\"position_tolerance_rad\":%.9g,\"safety_configured\":%s,\"max_count_delta\":%lu,"
            "\"stall_pwm_percent\":%.9g,\"stall_displacement_rad\":%.9g,\"stall_ms\":%lu,"
            "\"settle_velocity_rad_s\":%.9g}}\n",
            zero, json_bool(s->encoder.zeroed), json_bool(s->encoder_verified),
            json_bool(s->wiring_ack), json_bool(s->session_live),
            (unsigned long)s->calibration.ppr, (unsigned long)s->calibration.quadrature,
            (double)s->calibration.gear_ratio, s->calibration.direction,
            s->calibration.location == BENCH_ENCODER_OUTPUT ? "output" : "motor",
            json_bool(s->limits_configured), (double)s->limits.min_rad, (double)s->limits.max_rad,
            (double)s->limits.max_step_rad, (double)s->limits.max_velocity_rad_s,
            (unsigned long)s->limits.max_move_ms, json_bool(s->pid_configured), (double)s->pid.kp,
            (double)s->pid.ki, (double)s->pid.kd, (double)s->pid.integral_limit,
            (double)s->pid.derivative_tau_s, (double)s->pwm_limit, (double)s->tolerance_rad,
            json_bool(s->safety_configured), (unsigned long)s->safety.max_count_delta,
            (double)s->safety.stall_pwm, (double)s->safety.stall_displacement_rad,
            (unsigned long)s->safety.stall_ms, (double)s->safety.settle_velocity_rad_s);
    } else n = snprintf(packet + used, sizeof packet - used, "}\n");
    if (n < 0 || (size_t)n >= sizeof packet - used) return false;
    if (config) return queue_packet(packet);
    /* Reserve serial bandwidth for replies/heartbeats. 8N1 at 115200 baud
     * provides 11520 bytes/s. Never queue old telemetry behind fresh samples. */
    bandwidth_period = (uint32_t)(((used + (size_t)n) * 1000U + 8999U) / 9000U);
    if (reply_count || !BenchHW_Write(packet, used + (size_t)n)) { ++telemetry_dropped; return false; }
    return true;
}

static bool parse_float(const char *token, float *value)
{
    if (!token || !*token) return false;
    const char *p = token;
    if (*p == '+' || *p == '-') ++p;
    unsigned digits = 0U;
    while (*p >= '0' && *p <= '9') { ++p; ++digits; }
    if (*p == '.') { ++p; while (*p >= '0' && *p <= '9') { ++p; ++digits; } }
    if (!digits) return false;
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-') ++p;
        unsigned exponent_digits = 0U;
        while (*p >= '0' && *p <= '9') { ++p; ++exponent_digits; }
        if (!exponent_digits) return false;
    }
    if (*p) return false;
    char *end;
    errno = 0;
    float result = strtof(token, &end);
    if (errno || *end || !isfinite(result)) return false;
    *value = result;
    return true;
}

static bool parse_uint(const char *token, uint32_t *value)
{
    if (!token || !*token) return false;
    for (const char *p = token; *p; ++p) if (*p < '0' || *p > '9') return false;
    char *end;
    errno = 0;
    unsigned long result = strtoul(token, &end, 10);
    if (errno || *end || result > UINT32_MAX) return false;
    *value = (uint32_t)result;
    return true;
}

static const char *canonical_name(const char *token)
{
    static const char *names[] = {"help", "status", "encoder", "fault", "ping", "zero", "calibrate",
        "limits", "pid", "safety", "verify", "ack", "arm", "move", "jog", "pulse", "clear_fault",
        "stream", "stop", "disarm", "estop"};
    for (size_t i = 0U; i < sizeof names / sizeof names[0]; ++i)
        if (strcmp(token, names[i]) == 0) return names[i];
    return "invalid";
}

static void command(uint32_t now_ms)
{
    char *argv[12];
    unsigned argc = 0U;
    char *cursor = line;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        if (!*cursor) break;
        if (argc == 12U) { reply(canonical_name(argv[0]), false, "Too many arguments"); return; }
        argv[argc++] = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t') ++cursor;
        if (*cursor) *cursor++ = '\0';
    }
    if (!argc) return;
    const char *name = argv[0];
    bool ok = false, known = true;
    const char *message = NULL;
    float f[8];
    uint32_t u[3];
    if (strcmp(name, "estop") == 0 && argc == 1U) {
        Bench_EStop(); reply("estop", true, "Emergency stop latched"); return;
    }
    if ((strcmp(name, "stop") == 0 || strcmp(name, "disarm") == 0) && argc == 1U) {
        Bench_Stop(); reply(name, true, "Motor disabled and disarmed"); return;
    }
    /* Backpressure never authorizes an operation whose reply cannot be queued. */
    flush_replies();
    if (reply_count > REPLY_SLOTS - 2U) {
        Bench_Stop(); reply("busy", false, "TX congested; motor disarmed; command rejected"); return;
    }
    if (strcmp(name, "ping") == 0 && argc == 1U) { Bench_Ping(now_ms); ok = true; }
    else if ((strcmp(name, "status") == 0 || strcmp(name, "encoder") == 0 || strcmp(name, "fault") == 0) && argc == 1U) {
        ok = snapshot("status", true);
        message = ok ? "Snapshot emitted" : "Snapshot unavailable";
    } else if (strcmp(name, "help") == 0 && argc == 1U) {
        ok = true;
        message = "status encoder ping zero calibrate limits pid safety verify encoder ack wiring arm disarm move jog pulse stop estop fault clear_fault stream on [1..20] stream off; see docs/BENCH_PROTOCOL.md";
    } else if (strcmp(name, "zero") == 0 && argc == 1U) ok = Bench_Zero();
    else if (strcmp(name, "calibrate") == 0 && argc == 6U &&
        parse_uint(argv[1], &u[0]) && parse_uint(argv[2], &u[1]) && parse_float(argv[3], &f[0]) &&
        (strcmp(argv[4], "1") == 0 || strcmp(argv[4], "-1") == 0) &&
        (strcmp(argv[5], "motor") == 0 || strcmp(argv[5], "output") == 0)) {
        bench_encoder_config_t c = {u[0], u[1], f[0], strcmp(argv[4], "1") == 0 ? 1 : -1,
            strcmp(argv[5], "motor") == 0 ? BENCH_ENCODER_MOTOR : BENCH_ENCODER_OUTPUT};
        ok = Bench_Calibrate(&c);
    } else if (strcmp(name, "limits") == 0 && argc == 6U &&
        parse_float(argv[1], &f[0]) && parse_float(argv[2], &f[1]) && parse_float(argv[3], &f[2]) &&
        parse_float(argv[4], &f[3]) && parse_uint(argv[5], &u[0])) {
        bench_limits_t c = {f[0], f[1], f[2], f[3], u[0]};
        ok = Bench_SetLimits(&c);
    } else if (strcmp(name, "pid") == 0 && argc == 8U &&
        parse_float(argv[1], &f[0]) && parse_float(argv[2], &f[1]) && parse_float(argv[3], &f[2]) &&
        parse_float(argv[4], &f[3]) && parse_float(argv[5], &f[4]) && parse_float(argv[6], &f[5]) &&
        parse_float(argv[7], &f[6])) {
        motor_pid_config_t c = {f[0], f[1], f[2], f[3], f[4], 0.05f};
        ok = Bench_SetPID(&c, f[5], f[6]);
    } else if (strcmp(name, "safety") == 0 && argc == 6U &&
        parse_uint(argv[1], &u[0]) && parse_float(argv[2], &f[0]) && parse_float(argv[3], &f[1]) &&
        parse_uint(argv[4], &u[1]) && parse_float(argv[5], &f[2])) {
        bench_safety_config_t c = {u[0], f[0], f[1], u[1], f[2]};
        ok = Bench_SetSafety(&c);
    } else if (strcmp(name, "verify") == 0 && argc == 2U && strcmp(argv[1], "encoder") == 0) ok = Bench_VerifyEncoder();
    else if (strcmp(name, "ack") == 0 && argc == 2U && strcmp(argv[1], "wiring") == 0) ok = Bench_AckWiring();
    else if (strcmp(name, "arm") == 0 && argc == 1U) ok = Bench_Arm(false);
    else if (strcmp(name, "move") == 0 && argc == 2U && parse_float(argv[1], &f[0])) ok = Bench_Move(f[0]);
    else if (strcmp(name, "jog") == 0 && argc == 2U && parse_float(argv[1], &f[0])) ok = Bench_Jog(f[0]);
    else if (strcmp(name, "pulse") == 0 && argc == 3U && parse_float(argv[1], &f[0]) && parse_uint(argv[2], &u[0])) ok = Bench_Pulse(f[0], u[0]);
    else if (strcmp(name, "clear_fault") == 0 && argc == 1U) ok = Bench_ClearFault();
    else if (strcmp(name, "stream") == 0 && argc == 2U && strcmp(argv[1], "off") == 0) { streaming = false; ok = true; }
    else if (strcmp(name, "stream") == 0 && (argc == 2U || argc == 3U) && strcmp(argv[1], "on") == 0 &&
        (argc == 2U || (parse_uint(argv[2], &u[0]) && u[0] >= 1U && u[0] <= MAX_STREAM_HZ))) {
        stream_period = argc == 2U ? 50U : (1000U + u[0] - 1U) / u[0];
        streaming = true; last_stream = now_ms; ok = true;
    } else { known = false; message = "Unsupported command, wrong argument count or invalid numeric value"; }
    if (!known) reply(canonical_name(name), false, message);
    else reply(name, ok, message ? message : ok ? "OK" : Bench_LastError());
}

void BenchProtocol_Init(uint32_t now_ms)
{
    line_len = 0U; discard = false; saw_cr = false;
    line_started = now_ms; last_stream = now_ms; stream_period = 50U;
    streaming = false; telemetry_dropped = 0U; reply_head = reply_count = 0U;
    bandwidth_period = 50U; reported_fault = BENCH_FAULT_NONE;
    reply("hello", true, "stm32-motor-bench/1; standalone NUCLEO-G474RE; motor disabled; help lists commands");
}

void BenchProtocol_Byte(uint8_t byte, uint32_t now_ms)
{
    if (byte == '!') {
        Bench_EStop(); line_len = 0U; discard = true; saw_cr = false;
        reply("estop", true, "Emergency stop latched"); return;
    }
    if (line_len && (uint32_t)(now_ms - line_started) > LINE_TIMEOUT_MS) {
        discard = true; line_len = 0U;
        reply("invalid", false, "Partial command timed out; discard until newline");
    }
    if (byte == '\n') {
        if (discard) { discard = false; line_len = 0U; saw_cr = false; return; }
        line[line_len] = '\0';
        command(now_ms);
        line_len = 0U; saw_cr = false;
        return;
    }
    if (discard) return;
    if (byte == '\r') { if (saw_cr) discard = true; else saw_cr = true; return; }
    if (saw_cr || (byte < ' ' && byte != '\t') || byte > '~') {
        discard = true; line_len = 0U;
        reply("invalid", false, "Invalid ASCII line; discard until newline"); return;
    }
    if (!line_len) line_started = now_ms;
    if (line_len >= LINE_SIZE - 1U) {
        discard = true; line_len = 0U;
        reply("invalid", false, "Line too long; discard until newline"); return;
    }
    line[line_len++] = (char)byte;
}

void BenchProtocol_Service(uint32_t now_ms)
{
    flush_replies();
    bench_fault_t fault = Bench_Status()->fault;
    if (fault == BENCH_FAULT_NONE) reported_fault = BENCH_FAULT_NONE;
    else if (fault != reported_fault && reply("fault", false, Bench_FaultName())) reported_fault = fault;
    if (line_len && !discard && (uint32_t)(now_ms - line_started) > LINE_TIMEOUT_MS) {
        discard = true; line_len = 0U;
        reply("invalid", false, "Partial command timed out; discard until newline");
    }
    uint32_t period = stream_period > bandwidth_period ? stream_period : bandwidth_period;
    if (streaming && (uint32_t)(now_ms - last_stream) >= period) {
        last_stream = now_ms;
        (void)snapshot("telemetry", false);
    }
}
