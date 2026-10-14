#include "motor_kalman.h"
#include <math.h>
#include <string.h>
static bool positive(double x) { return isfinite(x) && x>0; }
static bool covariance(double a,double b,double d) {
    return isfinite(a) && isfinite(b) && isfinite(d) && a>=0 && d>=0 &&
        a<=1e30 && d<=1e30 && b*b<=a*d+1e-20;
}
bool MotorKalman_Validate(const motor_kalman_config_t *c) {
    return c && positive(c->encoder_to_motor_ratio) && positive(c->inertia_kg_m2) && isfinite(c->damping_nm_s) && c->damping_nm_s>=0 &&
        isfinite(c->torque_nm_a) && c->torque_nm_a>=0 &&
        isfinite(c->damping_nm_s/c->inertia_kg_m2) && isfinite(c->torque_nm_a/c->inertia_kg_m2) &&
        covariance(c->q_position,c->q_cross,c->q_velocity) && positive(c->r_position) &&
        positive(c->initial_position_variance) && c->initial_position_variance<=1e30 &&
        positive(c->initial_velocity_variance) && c->initial_velocity_variance<=1e30 &&
        positive(c->innovation_limit_rad) && positive(c->missing_current_q_factor) && c->missing_current_q_factor>=1 &&
        positive(c->max_dt_s) && c->max_dt_s<=1 && c->consecutive_limit>0 &&
        c->current_max_age_ms>0 && c->current_max_age_ms<0x80000000U &&
        c->encoder_max_age_ms>0 && c->encoder_max_age_ms<0x80000000U;
}
bool MotorKalman_Init(motor_kalman_t *s,const motor_kalman_config_t *c) {
    if(!s) return false;
    memset(s,0,sizeof(*s)); s->position=s->velocity=NAN; s->status=KALMAN_UNCONFIGURED;
    if(!MotorKalman_Validate(c)) return false;
    s->config=*c; s->configured=true; s->status=KALMAN_NEEDS_REFERENCE; return true;
}
bool MotorKalman_Reset(motor_kalman_t *s,double position,uint32_t ms,bool disabled,bool stationary,bool reference) {
    if(!s || !s->configured || !disabled || !stationary || !reference || !isfinite(position)) return false;
    s->position=position; s->velocity=0; s->p00=s->config.initial_position_variance;
    s->p11=s->config.initial_velocity_variance; s->p01=0; s->innovation=0;
    s->timestamp_ms=ms; s->encoder_ms=ms; s->encoder_seen=true; s->initialized=true;
    s->status=KALMAN_READY|KALMAN_DEGRADED; s->consecutive=0; ++s->resets; return true;
}
bool MotorKalman_IsValid(const motor_kalman_t *s) {
    return s && s->initialized && (s->status&KALMAN_READY) &&
        !(s->status&(KALMAN_OUTLIER|KALMAN_INCONSISTENT|KALMAN_NUMERIC_ERROR|KALMAN_TIME_ERROR|KALMAN_ENCODER_MISSING));
}
static bool usable(const motor_kalman_t *s) {
    return s && s->configured && s->initialized &&
        !(s->status&(KALMAN_INCONSISTENT|KALMAN_NUMERIC_ERROR|KALMAN_TIME_ERROR));
}
bool MotorKalman_Predict(motor_kalman_t *s,uint32_t ms,double current,bool current_valid,uint32_t age) {
    if(!usable(s)) return false;
    uint32_t elapsed=ms-s->timestamp_ms;
    if(!elapsed) return false; /* Duplicate tick: no prediction, no invented dt. */
    double dt=elapsed*.001;
    if(elapsed>=0x80000000U || dt>s->config.max_dt_s) { s->status|=KALMAN_TIME_ERROR; return false; }
    if(!covariance(s->p00,s->p01,s->p11) || !isfinite(s->position) || !isfinite(s->velocity)) {
        s->status|=KALMAN_NUMERIC_ERROR; return false;
    }
    bool input=current_valid && age<=s->config.current_max_age_ms && isfinite(current);
    double a=1,b=dt,bt=0,bw=0,qfactor=1;
    if(input) {
        double damping=s->config.damping_nm_s/s->config.inertia_kg_m2;
        double drive=s->config.torque_nm_a/s->config.inertia_kg_m2;
        /* Exact constant-input discretization, stable at zero damping. */
        double z=damping*dt;
        if(z<1e-6) {
            a=exp(-z); b=dt*(1-z*.5+z*z/6);
            bt=drive*dt*dt*(.5-z/6+z*z/24);
        } else {
            a=exp(-z); b=-expm1(-z)/damping; bt=drive*(dt-b)/damping;
        }
        bw=drive*b; s->status&=~KALMAN_DEGRADED;
    } else {
        /* Unknown input: constant velocity + inflated Q, NOT zero amperes
         * passed through the current/damping dynamics. */
        qfactor=s->config.missing_current_q_factor;
        s->status|=KALMAN_DEGRADED; ++s->missing_current;
    }
    double position=s->position+b*s->velocity+(input?bt*current:0);
    double velocity=a*s->velocity+(input?bw*current:0);
    double p00=s->p00+2*b*s->p01+b*b*s->p11+s->config.q_position*dt*qfactor;
    double p01=a*(s->p01+b*s->p11)+s->config.q_cross*dt*qfactor;
    double p11=a*a*s->p11+s->config.q_velocity*dt*qfactor;
    if(!isfinite(position) || !isfinite(velocity) || !covariance(p00,p01,p11)) {
        s->status|=KALMAN_NUMERIC_ERROR; return false;
    }
    s->position=position; s->velocity=velocity; s->p00=p00; s->p01=p01; s->p11=p11;
    s->timestamp_ms=ms; ++s->predictions;
    if((uint32_t)(ms-s->encoder_ms)>s->config.encoder_max_age_ms) s->status|=KALMAN_ENCODER_MISSING;
    return true;
}
bool MotorKalman_Correct(motor_kalman_t *s,double position,bool valid,uint32_t ms) {
    if(!usable(s)) return false;
    uint32_t age=s->timestamp_ms-ms;
    if(!valid || !isfinite(position) || age>s->config.encoder_max_age_ms) {
        s->status|=KALMAN_ENCODER_MISSING; return false;
    }
    /* No duplicate, backward or future observations. A late sample cannot
     * correct a state predicted to a different time without a delay model. */
    if(ms!=s->timestamp_ms || (s->encoder_seen && (int32_t)(ms-s->encoder_ms)<=0)) return false;
    s->innovation=position-s->position;
    if(!isfinite(s->innovation) || fabs(s->innovation)>s->config.innovation_limit_rad) {
        ++s->outliers; if(s->consecutive<UINT32_MAX) ++s->consecutive;
        s->status|=KALMAN_OUTLIER;
        if(s->consecutive>=s->config.consecutive_limit) s->status|=KALMAN_INCONSISTENT;
        s->encoder_ms=ms; s->encoder_seen=true; return false;
    }
    if(!covariance(s->p00,s->p01,s->p11)) { s->status|=KALMAN_NUMERIC_ERROR; return false; }
    double innovation_variance=s->p00+s->config.r_position;
    if(!positive(innovation_variance)) { s->status|=KALMAN_NUMERIC_ERROR; return false; }
    double k0=s->p00/innovation_variance,k1=s->p01/innovation_variance;
    double x=s->position+k0*s->innovation,v=s->velocity+k1*s->innovation;
    /* Joseph covariance update preserves PSD under repeated correction. */
    double d=1-k0;
    double p00=d*d*s->p00+k0*k0*s->config.r_position;
    double p01=d*(s->p01-k1*s->p00)+k0*k1*s->config.r_position;
    double p11=s->p11-2*k1*s->p01+k1*k1*(s->p00+s->config.r_position);
    if(!isfinite(x) || !isfinite(v) || !covariance(p00,p01,p11)) { s->status|=KALMAN_NUMERIC_ERROR; return false; }
    s->position=x; s->velocity=v; s->p00=p00; s->p01=p01; s->p11=p11;
    s->encoder_ms=ms; s->encoder_seen=true; s->consecutive=0; ++s->corrections;
    s->status&=~(KALMAN_OUTLIER|KALMAN_ENCODER_MISSING); return true;
}
