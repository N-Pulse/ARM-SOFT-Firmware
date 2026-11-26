#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct npulse_sync_result {
 uint32_t sim_ms, run, causes, feedback_age_ms;
 uint8_t phase, armed, enabled;
 double target, reference, position, velocity, output, kp, ki, kd;
};
int npulse_sync_init(void);
void npulse_sync_stop(void);
void npulse_sync_step(uint32_t sim_ms,const double position[8],const double velocity[8],
 const double applied[8],int enabled,uint64_t operator_session,uint32_t operator_run,
 uint64_t target_revision,double index_target,uint8_t limit,struct npulse_sync_result *result);
#ifdef __cplusplus
}
#endif
