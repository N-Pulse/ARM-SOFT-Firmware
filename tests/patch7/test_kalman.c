#include "motor_kalman.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Fictitious SI parameters for numerical tests ONLY. */
static motor_kalman_config_t synthetic(void) {
    return (motor_kalman_config_t){.inertia_kg_m2=1,.damping_nm_s=0,.torque_nm_a=1,
        .q_position=.0001,.q_velocity=.01,.r_position=.001,
        .initial_position_variance=1,.initial_velocity_variance=1,.innovation_limit_rad=.5,
        .missing_current_q_factor=10,.max_dt_s=.1,.consecutive_limit=3,
        .current_max_age_ms=50,.encoder_max_age_ms=100,.encoder_to_motor_ratio=1};
}
static motor_kalman_t fresh(double position) {
    motor_kalman_t s; motor_kalman_config_t c=synthetic();
    assert(MotorKalman_Init(&s,&c)); assert(!MotorKalman_IsValid(&s));
    assert(MotorKalman_Reset(&s,position,0,true,true,true)); return s;
}
static void stability(const motor_kalman_t *s) {
    assert(isfinite(s->position) && isfinite(s->velocity));
    assert(s->p00>=0 && s->p11>=0 && s->p01*s->p01<=s->p00*s->p11+1e-15);
}
int main(void) {
    motor_kalman_t s=fresh(2);
    for(unsigned ms=10;ms<=10000;ms+=10) {
        assert(MotorKalman_Predict(&s,ms,0,true,0)); assert(MotorKalman_Correct(&s,2,true,ms)); stability(&s);
    }
    assert(fabs(s.position-2)<1e-10 && fabs(s.velocity)<1e-10);
    puts("PASS Kalman constant position");
    s=fresh(0);
    for(unsigned ms=10;ms<=5000;ms+=10) {
        assert(MotorKalman_Predict(&s,ms,NAN,false,0)); assert(MotorKalman_Correct(&s,ms*.001,true,ms));
    }
    assert(fabs(s.position-5)<.005 && fabs(s.velocity-1)<.01 && (s.status&KALMAN_DEGRADED));
    puts("PASS Kalman constant velocity / encoder-only mode");
    s=fresh(0);
    for(unsigned ms=10;ms<=5000;ms+=10) {
        double t=ms*.001;
        assert(MotorKalman_Predict(&s,ms,.3,true,0)); assert(MotorKalman_Correct(&s,.15*t*t,true,ms));
    }
    assert(fabs(s.position-3.75)<1e-8 && fabs(s.velocity-1.5)<1e-8);
    puts("PASS Kalman accelerating motor / current input");
    s=fresh(0); double measured_error=0,estimated_error=0;
    for(unsigned ms=10;ms<=10000;ms+=10) {
        double t=ms*.001,truth=.2*t,noise=.04*sin(ms*.07)+.03*cos(ms*.013);
        assert(MotorKalman_Predict(&s,ms,0,false,0));
        assert(MotorKalman_Correct(&s,truth+noise,true,ms));
        if(ms>2000) { measured_error+=noise*noise; estimated_error+=(s.position-truth)*(s.position-truth); }
        stability(&s);
    }
    assert(estimated_error<measured_error*.5);
    puts("PASS Kalman noisy encoder: estimation reduces synthetic RMS error");
    s=fresh(0);
    for(unsigned ms=10;ms<=10000;ms+=10) {
        double t=ms*.001;
        assert(MotorKalman_Predict(&s,ms,.2+.1*sin(ms*.09),true,0));
        assert(MotorKalman_Correct(&s,.1*t*t+.01*sin(ms*.07),true,ms)); stability(&s);
    }
    assert(fabs(s.position-10)<.03 && fabs(s.velocity-2)<.08);
    puts("PASS Kalman current noise with encoder corrections");
    s=fresh(0); s.config.damping_nm_s=2; s.velocity=1;
    assert(MotorKalman_Predict(&s,10,100,true,51)); assert(s.velocity==1 && (s.status&KALMAN_DEGRADED));
    assert(MotorKalman_Predict(&s,20,INFINITY,true,0)); assert(s.velocity==1 && s.missing_current==2);
    assert(MotorKalman_Predict(&s,30,NAN,false,0)); assert(s.velocity==1);
    puts("PASS Kalman missing/stale/nonfinite current is unknown input, not zero amps");
    s=fresh(0);
    for(unsigned ms=10;ms<=30;ms+=10) {
        assert(MotorKalman_Predict(&s,ms,0,true,0));
        assert(!MotorKalman_Correct(&s,100,true,ms)); assert(s.position==0 && !MotorKalman_IsValid(&s));
    }
    assert(s.outliers==3 && s.consecutive==3 && (s.status&KALMAN_INCONSISTENT));
    assert(!MotorKalman_Predict(&s,40,0,true,0));
    assert(!MotorKalman_Reset(&s,100,40,false,true,true));
    assert(!MotorKalman_Reset(&s,100,40,true,false,true));
    assert(!MotorKalman_Reset(&s,100,40,true,true,false));
    assert(!MotorKalman_Reset(&s,NAN,40,true,true,true));
    assert(MotorKalman_Reset(&s,100,40,true,true,true)); assert(MotorKalman_IsValid(&s) && s.resets==2);
    puts("PASS Kalman encoder outlier, persistent mismatch, explicit controlled reset gates");
    motor_kalman_config_t good=synthetic(),bad;
#define BAD(field,value) do { bad=good; bad.field=value; assert(!MotorKalman_Validate(&bad)); } while(0)
    BAD(inertia_kg_m2,0); BAD(inertia_kg_m2,NAN); BAD(damping_nm_s,-1); BAD(torque_nm_a,INFINITY);
    BAD(q_position,-1); BAD(q_cross,1); BAD(r_position,0); BAD(initial_velocity_variance,-1);
    BAD(innovation_limit_rad,0); BAD(consecutive_limit,0); BAD(max_dt_s,0); BAD(missing_current_q_factor,.5);
    BAD(current_max_age_ms,0); BAD(encoder_max_age_ms,UINT32_MAX);
    BAD(encoder_to_motor_ratio,0);
    assert(!MotorKalman_Init(&s,&bad)); assert(s.status==KALMAN_UNCONFIGURED);
    puts("PASS Kalman parameter validation");
    s=fresh(0); assert(!MotorKalman_Predict(&s,0,0,true,0)); assert(s.predictions==0);
    for(unsigned ms=1;ms<100;ms+=7) {
        assert(MotorKalman_Predict(&s,ms,0,true,0)); assert(MotorKalman_Correct(&s,0,true,ms));
        assert(!MotorKalman_Correct(&s,0,true,ms));
    }
    assert(!MotorKalman_Predict(&s,500,0,true,0) && (s.status&KALMAN_TIME_ERROR));
    assert(MotorKalman_Reset(&s,0,UINT32_MAX-5,true,true,true));
    assert(MotorKalman_Predict(&s,4,0,true,0)); assert(MotorKalman_Correct(&s,0,true,4));
    assert(!MotorKalman_Predict(&s,3,0,true,0)); assert(s.status&KALMAN_TIME_ERROR);
    puts("PASS Kalman zero/irregular/backward time, duplicate correction, uint32 wrap");
    s=fresh(0); assert(MotorKalman_Predict(&s,10,0,true,0));
    assert(!MotorKalman_Correct(&s,NAN,true,10)); assert(s.status&KALMAN_ENCODER_MISSING);
    assert(!MotorKalman_Correct(&s,0,true,11));
    s=fresh(0); s.p00=NAN; assert(!MotorKalman_Predict(&s,10,0,true,0)); assert(s.status&KALMAN_NUMERIC_ERROR);
    s=fresh(0);
    for(unsigned ms=1;ms<=100000;++ms) {
        assert(MotorKalman_Predict(&s,ms,0,true,0)); assert(MotorKalman_Correct(&s,.00001*sin(ms),true,ms)); stability(&s);
    }
    puts("PASS Kalman invalid/stale encoder and 100000-step covariance stability");
    motor_kalman_t eight[8];
    for(unsigned i=0;i<8;++i) eight[i]=fresh(i);
    for(unsigned i=0;i<8;++i) {
        assert(MotorKalman_Predict(&eight[i],10,i*.1,true,0));
        assert(fabs(eight[i].position-(i+i*.000005))<1e-10);
        assert(eight[i].predictions==1);
    }
    puts("PASS Kalman eight independent instances; module links without actuation code");
    return 0;
}
