#define main startup_suite
#include "test_encoder.c"
#undef main
#include "../../fw/bsp/encoder/encoder_bsp.c"
#include <float.h>
#include <math.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(Encoder_BSP_Init());
    if (argc > 1 && !strcmp(argv[1], "all")) {
        encoder_calibration_t c = {1000, 4, 1.0f, 1, 0};
        for (unsigned q = 1; q <= 4; q *= 2) for (int dir = -1; dir <= 1; dir += 2)
        for (int off = -100; off <= 100; off += 100) {
            assert(Encoder_BSP_Reset(MOTOR_INDEX));
            c.quadrature_factor=q; c.direction=dir; c.zero_offset_counts=off;
            assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
            TIM8->CNT=(uint16_t)(TIM8->CNT + 200);
            double expected = (200.0-off)*6.283185307179586/(1000.0*q)*dir;
            encoder_snapshot_t sample; assert(Encoder_BSP_GetSnapshot(MOTOR_INDEX,&sample));
            assert(sample.raw_count == 200 && fabs(sample.position_rad-expected)<1e-6);
            assert(Encoder_BSP_Reset(MOTOR_INDEX));
            assert(Encoder_BSP_GetPosition(MOTOR_INDEX)==0.0f);
        }
        c=(encoder_calibration_t){1000,4,1.0f,1,0};
        float ratios[]={1.40129846e-45f, NAN, INFINITY, -INFINITY, 0.0f, -1.0f, FLT_MAX, FLT_MIN};
        for(unsigned i=0;i<sizeof(ratios)/sizeof(ratios[0]);++i) {
            c.gear_ratio=ratios[i]; bool accepted=Encoder_BSP_SetCalibration(MOTOR_INDEX,&c);
            /* FLT_MIN*4000 is finite, with a representable normal scale. */
            assert(accepted == (ratios[i]==FLT_MIN));
        }
        c=(encoder_calibration_t){1000,4,1.0f,1,0};
        assert(Encoder_BSP_Reset(MOTOR_INDEX)); assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        TIM8->CNT=(uint16_t)(TIM8->CNT+100); float stable=Encoder_BSP_GetPosition(MOTOR_INDEX);
        c.gear_ratio=NAN; assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        assert(Encoder_BSP_IsCalibrated(MOTOR_INDEX)); assert(Encoder_BSP_GetPosition(MOTOR_INDEX)==stable);
        c=(encoder_calibration_t){UINT32_MAX,4,FLT_MAX,1,0}; assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        c=(encoder_calibration_t){1,1,FLT_MIN,1,0}; assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        c=(encoder_calibration_t){1000,4,1.0f,0,0}; assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        c.direction=1; c.quadrature_factor=3; assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        c.quadrature_factor=4; c.counts_per_revolution=0; assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        c=(encoder_calibration_t){1000,4,1.0f,1,INT64_MAX-1};
        assert(Encoder_BSP_Reset(MOTOR_INDEX)); assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        s_raw_count[MOTOR_INDEX]=INT64_MAX;
        assert(fabsf(Encoder_BSP_GetPosition(MOTOR_INDEX)-s_scale[MOTOR_INDEX]) < 1e-8f);
        TIM8->CNT=(uint16_t)(TIM8->CNT+1);
        assert(isnan(Encoder_BSP_GetPosition(MOTOR_INDEX))); assert(!Encoder_BSP_IsCalibrated(MOTOR_INDEX));
        assert(Encoder_BSP_GetRawCount(MOTOR_INDEX)==INT64_MAX);
        assert(!Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        assert(Encoder_BSP_Reset(MOTOR_INDEX)); c.zero_offset_counts=INT64_MIN+1;
        assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c)); s_raw_count[MOTOR_INDEX]=INT64_MIN;
        assert(fabsf(Encoder_BSP_GetPosition(MOTOR_INDEX)+s_scale[MOTOR_INDEX]) < 1e-8f);
        TIM8->CNT=(uint16_t)(TIM8->CNT-1);
        assert(isnan(Encoder_BSP_GetPosition(MOTOR_INDEX))); assert(!Encoder_BSP_IsCalibrated(MOTOR_INDEX));
        assert(Encoder_BSP_Reset(MOTOR_INDEX)); c.zero_offset_counts=INT64_MIN;
        assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c)); s_raw_count[MOTOR_INDEX]=INT64_MAX;
        assert(isfinite(Encoder_BSP_GetPosition(MOTOR_INDEX)));
        assert(Encoder_BSP_Reset(MOTOR_INDEX));
        c=(encoder_calibration_t){1000,4,FLT_MIN,1,INT64_MIN};
        assert(Encoder_BSP_SetCalibration(MOTOR_INDEX,&c));
        assert(isnan(Encoder_BSP_GetPosition(MOTOR_INDEX))); assert(!Encoder_BSP_IsCalibrated(MOTOR_INDEX));
        assert(Encoder_BSP_ResetAll()); assert(Encoder_BSP_Init()); assert(lp_starts==1);
        for(unsigned i=0;i<21;++i) assert(starts[i]<=1);
        puts("PASS BSP numeric matrix: 18 ordinary reference cases, invalid/extreme products, int64 offsets/differences/accumulation, sticky faults and no encoder restart");
        return 0;
    }
    encoder_calibration_t c = {1000, 4, 1.0f, 1, 100};
    if (argc > 1 && !strcmp(argv[1], "product")) {
        c.gear_ratio = FLT_MAX;
        bool ok = Encoder_BSP_SetCalibration(MOTOR_INDEX, &c);
        printf("REPRO extreme product accepted=%d calibrated=%d\n", ok, Encoder_BSP_IsCalibrated(MOTOR_INDEX));
        assert(!ok); return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "offset")) c.zero_offset_counts = INT64_MIN;
    assert(Encoder_BSP_SetCalibration(MOTOR_INDEX, &c));
    TIM8->CNT = 100;
    float before = Encoder_BSP_GetPosition(MOTOR_INDEX);
    if (argc > 1 && !strcmp(argv[1], "offset")) { assert(isfinite(before) && before > 0.0f); return 0; }
    Encoder_BSP_Reset(MOTOR_INDEX);
    float after = Encoder_BSP_GetPosition(MOTOR_INDEX);
    printf("REPRO reset before=%g after=%g\n", (double)before, (double)after);
    assert(after == 0.0f); return 0;
}
