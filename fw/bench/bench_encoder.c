#include "bench_encoder.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static bool convert(bench_encoder_t *e)
{
    e->sample.position_rad = NAN;
    e->sample.velocity_rad_s = NAN;
    e->sample.valid = false;
    if (!e->sample.available || e->numeric_fault) return false;
    if (!e->sample.calibrated) return true;
    /* Unsigned subtraction gives an exact magnitude even across signed zero.
     * Subtract before conversion so a nearby origin remains precise at large
     * accumulated counts, and never subtract two int64 values with overflow. */
    bool positive = e->sample.counts >= e->sample.zero_counts;
    uint64_t distance = positive ? (uint64_t)e->sample.counts - (uint64_t)e->sample.zero_counts :
        (uint64_t)e->sample.zero_counts - (uint64_t)e->sample.counts;
    double p = (double)distance * e->radians_per_count * (positive ? 1.0 : -1.0);
    if (!isfinite(p) || fabs(p) > 1.0e7) return false;
    e->sample.position_rad = (float)p;
    e->sample.velocity_rad_s = 0.0f;
    e->sample.valid = true;
    return true;
}

void BenchEncoder_Init(bench_encoder_t *e, bool available, uint32_t raw, uint32_t now_ms)
{
    memset(e, 0, sizeof(*e));
    e->sample.available = available;
    e->sample.raw = e->previous_raw = raw;
    e->sample.timestamp_ms = e->previous_ms = now_ms;
    e->sample.position_rad = e->sample.velocity_rad_s = NAN;
}

bool BenchEncoder_Configure(bench_encoder_t *e, const bench_encoder_config_t *c)
{
    if (!c || c->ppr == 0 || c->ppr > 10000000U || c->quadrature != 4U ||
        !isfinite(c->gear_ratio) || c->gear_ratio < 1.0f || c->gear_ratio > 100000.0f ||
        (c->direction != 1 && c->direction != -1) ||
        (c->location != BENCH_ENCODER_MOTOR && c->location != BENCH_ENCODER_OUTPUT)) return false;
    double denominator = (double)c->ppr * c->quadrature;
    if (c->location == BENCH_ENCODER_MOTOR) denominator *= c->gear_ratio;
    double scale = 6.28318530717958647692 / denominator * c->direction;
    if (!isfinite(scale) || fabs(scale) < 1.0e-14) return false;
    e->config = *c;
    e->radians_per_count = scale;
    e->sample.calibrated = true;
    e->sample.zeroed = false;
    e->sample.zero_counts = e->sample.counts;
    return convert(e);
}

bool BenchEncoder_Update(bench_encoder_t *e, uint32_t raw, uint32_t now_ms)
{
    uint32_t dt = now_ms - e->previous_ms;
    if (!e->sample.available || dt == 0 || dt > INT32_MAX || e->numeric_fault) {
        e->sample.valid = false;
        e->sample.position_rad = e->sample.velocity_rad_s = NAN;
        return false;
    }
    uint32_t modular = raw - e->previous_raw;
    /* Exactly half the counter range has no unambiguous direction. */
    if (modular == 0x80000000U) {
        e->numeric_fault = true;
        e->sample.valid = false;
        e->sample.position_rad = e->sample.velocity_rad_s = NAN;
        return false;
    }
    int64_t delta = modular <= INT32_MAX ? (int64_t)modular : -(int64_t)(UINT32_MAX - modular + 1U);
    if ((delta > 0 && e->sample.counts > INT64_MAX - delta) ||
        (delta < 0 && e->sample.counts < INT64_MIN - delta)) {
        e->numeric_fault = true;
        e->sample.valid = false;
        e->sample.position_rad = e->sample.velocity_rad_s = NAN;
        return false;
    }
    e->sample.raw = e->previous_raw = raw;
    e->sample.timestamp_ms = e->previous_ms = now_ms;
    e->sample.delta = (int32_t)delta;
    e->sample.direction = delta > 0 ? 1 : delta < 0 ? -1 : 0;
    e->sample.counts += delta;
    if (!convert(e)) return false;
    if (e->sample.calibrated) {
        double v = (double)delta * e->radians_per_count * 1000.0 / dt;
        if (!isfinite(v) || fabs(v) > 1.0e9) {
            e->sample.valid = false;
            e->sample.position_rad = e->sample.velocity_rad_s = NAN;
            return false;
        }
        e->sample.velocity_rad_s = (float)v;
    }
    return true;
}

bool BenchEncoder_Zero(bench_encoder_t *e)
{
    if (!e->sample.available || e->numeric_fault) return false;
    e->sample.zero_counts = e->sample.counts;
    e->sample.zeroed = true;
    return convert(e);
}
