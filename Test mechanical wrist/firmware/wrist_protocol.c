#include "wrist_protocol.h"
#include "wrist.h"
#include "bench_hw.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 192U
#define PACKET_SIZE 4096U
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
static const char *reported_fault;
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
    const wrist_status_t *s = Wrist_Status();
    char packet[PACKET_SIZE];
    int n = snprintf(packet, sizeof packet,
        "{\"type\":\"%s\",\"firmware\":\"stm32-wrist-bench/1\",\"t_ms\":%lu,"
        "\"state\":\"%s\",\"armed\":%s,\"target_reached\":%s,\"dt_s\":%.9g,"
        "\"fault\":\"%s\",\"fault_motor\":%d,\"telemetry_dropped\":%lu,\"motors\":[",
        type, (unsigned long)s->now, Wrist_State(), json_bool(s->armed), json_bool(s->reached),
        (double)s->dt, s->fault ? s->fault : "none", s->fault_motor, (unsigned long)telemetry_dropped);
    if (n < 0 || (size_t)n >= sizeof packet) return false;
    size_t used = (size_t)n;
    for (unsigned i = 0; i < 2; ++i) {
        const wrist_motor_t *m = &s->motors[i];
        char target[32], position[32], velocity[32], error[32], counts[24];
        number(target, sizeof target, m->target, true);
        number(position, sizeof position, m->position, m->encoder.sample.valid);
        number(velocity, sizeof velocity, m->velocity, m->encoder.sample.valid);
        number(error, sizeof error, m->error, m->encoder.sample.valid);
        count_string(counts, m->encoder.sample.counts);
        n = snprintf(packet+used, sizeof packet-used,
            "%s{\"id\":%u,\"target_deg\":%s,\"position_deg\":%s,\"velocity_deg_s\":%s,\"error_deg\":%s,"
            "\"pid_percent\":%.9g,\"pwm_percent\":%.9g,\"raw\":%lu,\"counts\":%s,\"delta\":%ld,"
            "\"encoder_available\":%s,\"calibrated\":%s,\"zeroed\":%s,\"feedback_valid\":%s",
            i ? "," : "", i+1U, target, position, velocity, error, (double)m->demand, (double)m->applied,
            (unsigned long)m->encoder.sample.raw, counts, (long)m->encoder.sample.delta,
            json_bool(m->encoder.sample.available), json_bool(m->encoder.sample.calibrated),
            json_bool(m->encoder.sample.zeroed), json_bool(m->encoder.sample.valid));
        if (n < 0 || (size_t)n >= sizeof packet-used) return false;
        used += (size_t)n;
        if (config) {
            const bench_encoder_config_t *c = &m->encoder.config;
            const motor_pid_config_t *p = &m->pid.config;
            n = snprintf(packet+used, sizeof packet-used,
                ",\"configuration\":{\"ppr\":%lu,\"quadrature\":%lu,\"gear_ratio\":%.9g,\"encoder_sign\":%d,"
                "\"encoder_location\":\"%s\",\"limits_set\":%s,\"pid_set\":%s,\"safety_set\":%s,"
                "\"limits\":[%.9g,%.9g,%.9g,%.9g,%lu],\"pid\":[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g],"
                "\"safety\":[%lu,%.9g,%.9g,%lu,%.9g,%.9g,%lu]}",
                (unsigned long)c->ppr, (unsigned long)c->quadrature, (double)c->gear_ratio, c->direction,
                m->encoder.sample.calibrated ? (c->location == BENCH_ENCODER_MOTOR ? "motor" : "output") : "unknown",
                json_bool(m->limits_set), json_bool(m->pid_set), json_bool(m->safety_set),
                (double)m->limits.min_deg, (double)m->limits.max_deg, (double)m->limits.step_deg,
                (double)m->limits.velocity_deg_s, (unsigned long)m->limits.move_ms,
                (double)p->kp, (double)p->ki, (double)p->kd, (double)p->integral_limit, (double)p->derivative_tau_s,
                (double)m->pwm_cap, (double)m->tolerance, (unsigned long)m->safety.delta,
                (double)m->safety.stall_pwm, (double)m->safety.stall_deg, (unsigned long)m->safety.stall_ms,
                (double)m->safety.settle_deg_s, (double)m->safety.hold_error_deg, (unsigned long)m->safety.hold_error_ms);
            if (n < 0 || (size_t)n >= sizeof packet-used) return false;
            used += (size_t)n;
        }
        if (used+1U >= sizeof packet) return false;
        packet[used++] = '}';
    }
    if (used+4U >= sizeof packet) return false;
    memcpy(packet+used, "]}\n", 4U);
    used += 3U;
    if (config) return queue_packet(packet);
    bandwidth_period = (uint32_t)((used*1000U+8999U)/9000U);
    if (reply_count || !BenchHW_Write(packet, used)) { ++telemetry_dropped; return false; }
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
        "limits", "pid", "safety", "arm", "move", "jog", "clear_fault", "stream", "stop", "disarm", "estop"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (strcmp(token, names[i]) == 0) return names[i];
    return "invalid";
}
static bool motor_id(const char *text, unsigned *id)
{
    if (strcmp(text, "1") == 0) { *id = 0; return true; }
    if (strcmp(text, "2") == 0) { *id = 1; return true; }
    return false;
}
static bool movement(char **v, unsigned n, bool relative)
{
    const wrist_status_t *s = Wrist_Status();
    float a = s->active ? s->motors[0].target : s->motors[0].position;
    float b = s->active ? s->motors[1].target : s->motors[1].position;
    float x, y; unsigned i;
    if (n == 4 && strcmp(v[1], "motor") == 0 && motor_id(v[2], &i) && parse_float(v[3], &x)) {
        if (i == 0) a = relative ? a+x : x; else b = relative ? b+x : x;
    } else if (n == 4 && strcmp(v[1], "motors") == 0 && parse_float(v[2], &x) && parse_float(v[3], &y)) {
        a = relative ? a+x : x; b = relative ? b+y : y;
    } else if (n == 3 && parse_float(v[2], &x) &&
               (strcmp(v[1], "flex") == 0 || strcmp(v[1], "rotation") == 0)) {
        float flex = (a-b)/2.0f, rotation = (a+b)/2.0f;
        if (strcmp(v[1], "flex") == 0) flex = relative ? flex+x : x;
        else rotation = relative ? rotation+x : x;
        a = rotation+flex; b = rotation-flex;
    } else if (n == 4 && strcmp(v[1], "wrist") == 0 && parse_float(v[2], &x) && parse_float(v[3], &y)) {
        float flex = relative ? (a-b)/2.0f+x : x;
        float rotation = relative ? (a+b)/2.0f+y : y;
        a = rotation+flex; b = rotation-flex;
    } else return false;
    return Wrist_Move(a, b);
}
static void command(uint32_t now_ms)
{
    char *v[12]; unsigned n = 0;
    char *cursor = line;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        if (!*cursor) break;
        if (n == 12) { reply(canonical_name(v[0]), false, "Too many arguments"); return; }
        v[n++] = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t') ++cursor;
        if (*cursor) *cursor++ = 0;
    }
    if (!n) return;
    const char *name = canonical_name(v[0]);
    if (n == 1 && strcmp(name, "estop") == 0) { Wrist_Estop(); reply(name, true, "Both motors stopped"); return; }
    if (n == 1 && (strcmp(name, "stop") == 0 || strcmp(name, "disarm") == 0)) {
        Wrist_Stop(); reply(name, true, "Both motors disabled and disarmed"); return;
    }
    flush_replies();
    if (reply_count > REPLY_SLOTS-2U) { Wrist_Stop(); reply("busy", false, "TX congested; both motors stopped"); return; }
    bool ok = false;
    const char *message = NULL;
    float f[8]; uint32_t u[3]; unsigned i;
    if (strcmp(name, "ping") == 0 && n == 1) { Wrist_Ping(now_ms); ok = true; }
    else if ((strcmp(name, "status") == 0 || strcmp(name, "encoder") == 0 || strcmp(name, "fault") == 0) && n == 1) {
        ok = snapshot("status", true);
    } else if (strcmp(name, "help") == 0 && n == 1) {
        ok = true; message = "status encoder zero calibrate MOTOR PPR 4 GEAR SIGN LOCATION; limits/pid/safety MOTOR ...; arm; move/jog motor ID DEG | motors M1 M2 | flex DEG | rotation DEG | wrist FLEX ROTATION; stop estop clear_fault stream on [HZ] stream off; degrees=gearbox outputs";
    } else if (strcmp(name, "zero") == 0 && n == 1) ok = Wrist_Zero();
    else if (strcmp(name, "arm") == 0 && n == 1) ok = Wrist_Arm();
    else if (strcmp(name, "clear_fault") == 0 && n == 1) ok = Wrist_ClearFault();
    else if (strcmp(name, "calibrate") == 0 && n == 7 && motor_id(v[1], &i) && parse_uint(v[2], &u[0]) &&
        parse_uint(v[3], &u[1]) && parse_float(v[4], &f[0]) &&
        (strcmp(v[5], "1") == 0 || strcmp(v[5], "-1") == 0) &&
        (strcmp(v[6], "motor") == 0 || strcmp(v[6], "output") == 0)) {
        bench_encoder_config_t c = {u[0], u[1], f[0], strcmp(v[5], "1") == 0 ? 1 : -1,
            strcmp(v[6], "motor") == 0 ? BENCH_ENCODER_MOTOR : BENCH_ENCODER_OUTPUT};
        ok = Wrist_Calibrate(i, &c);
    } else if (strcmp(name, "limits") == 0 && n == 7 && motor_id(v[1], &i) &&
        parse_float(v[2], &f[0]) && parse_float(v[3], &f[1]) && parse_float(v[4], &f[2]) &&
        parse_float(v[5], &f[3]) && parse_uint(v[6], &u[0])) {
        wrist_limits_t c = {f[0], f[1], f[2], f[3], u[0]}; ok = Wrist_Limits(i, &c);
    } else if (strcmp(name, "pid") == 0 && n == 9 && motor_id(v[1], &i) &&
        parse_float(v[2], &f[0]) && parse_float(v[3], &f[1]) && parse_float(v[4], &f[2]) &&
        parse_float(v[5], &f[3]) && parse_float(v[6], &f[4]) && parse_float(v[7], &f[5]) && parse_float(v[8], &f[6])) {
        motor_pid_config_t c = {f[0], f[1], f[2], f[3], f[4], WRIST_MAX_CONTROL_MS/1000.0f};
        ok = Wrist_PID(i, &c, f[5], f[6]);
    } else if (strcmp(name, "safety") == 0 && n == 9 && motor_id(v[1], &i) &&
        parse_uint(v[2], &u[0]) && parse_float(v[3], &f[0]) && parse_float(v[4], &f[1]) &&
        parse_uint(v[5], &u[1]) && parse_float(v[6], &f[2]) && parse_float(v[7], &f[3]) && parse_uint(v[8], &u[2])) {
        wrist_safety_t c = {u[0], f[0], f[1], u[1], f[2], f[3], u[2]}; ok = Wrist_Safety(i, &c);
    } else if (strcmp(name, "move") == 0 || strcmp(name, "jog") == 0) ok = movement(v, n, strcmp(name, "jog") == 0);
    else if (strcmp(name, "stream") == 0 && n == 2 && strcmp(v[1], "off") == 0) { streaming = false; ok = true; }
    else if (strcmp(name, "stream") == 0 && (n == 2 || n == 3) && strcmp(v[1], "on") == 0 &&
        (n == 2 || (parse_uint(v[2], &u[0]) && u[0] >= 1 && u[0] <= MAX_STREAM_HZ))) {
        stream_period = n == 2 ? 100U : (1000U+u[0]-1U)/u[0]; streaming = true; ok = true;
    }
    if (!message) message = ok ? "OK" : "Rejected: check syntax, calibration, zero, limits, PID, safety and state";
    if (!ok && strcmp(Wrist_Error(), "ok") != 0) message = Wrist_Error();
    if (!reply(name, ok, message)) Wrist_Stop();
}

void Protocol_Init(uint32_t now_ms)
{
    line_len = 0U; discard = false; saw_cr = false;
    line_started = now_ms; last_stream = now_ms; stream_period = 50U;
    streaming = false; telemetry_dropped = 0U; reply_head = reply_count = 0U;
    bandwidth_period = 50U; reported_fault = NULL;
    reply("hello", true, "stm32-wrist-bench/1; two motors disabled; hold mode; help lists commands");
}

void Protocol_Byte(uint8_t byte, uint32_t now_ms)
{
    if (byte == '!') {
        Wrist_Estop(); line_len = 0U; discard = true; saw_cr = false;
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

void Protocol_Service(uint32_t now_ms)
{
    flush_replies();
    const char *fault = Wrist_Status()->fault;
    if (!fault) reported_fault = NULL;
    else if (fault != reported_fault && reply("fault", false, fault)) reported_fault = fault;
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
