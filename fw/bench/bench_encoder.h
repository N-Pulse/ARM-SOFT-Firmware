#ifndef BENCH_ENCODER_H
#define BENCH_ENCODER_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { BENCH_ENCODER_MOTOR = 0, BENCH_ENCODER_OUTPUT = 1 } bench_encoder_location_t;
typedef struct {
    uint32_t ppr; /* Full electrical cycles of ONE channel per encoder-shaft turn. */
    uint32_t quadrature; /* Must be 4: TIM2 encoder mode counts every A/B edge. */
    float gear_ratio; /* Motor turns per output turn; only applied at MOTOR. */
    int direction; /* +1 or -1. */
    bench_encoder_location_t location;
} bench_encoder_config_t;
typedef struct {
    uint32_t raw, timestamp_ms;
    int64_t counts, zero_counts;
    int32_t delta;
    int direction;
    float position_rad, velocity_rad_s;
    bool available, calibrated, valid, zeroed;
} bench_encoder_sample_t;
typedef struct {
    bench_encoder_config_t config;
    bench_encoder_sample_t sample;
    double radians_per_count;
    uint32_t previous_raw, previous_ms;
    bool numeric_fault;
} bench_encoder_t;

void BenchEncoder_Init(bench_encoder_t *e, bool available, uint32_t raw, uint32_t now_ms);
bool BenchEncoder_Configure(bench_encoder_t *e, const bench_encoder_config_t *config);
bool BenchEncoder_Update(bench_encoder_t *e, uint32_t raw, uint32_t now_ms);
bool BenchEncoder_Zero(bench_encoder_t *e);
#endif
