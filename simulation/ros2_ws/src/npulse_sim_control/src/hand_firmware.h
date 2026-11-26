#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct npulse_hand_result {
 uint32_t sim_ms,run,causes,feedback_age_ms[8];
 uint8_t phase,armed,enabled_mask;
 double target[8],reference[8],position[8],velocity[8],output[8],kp[8],ki[8],kd[8];
};
int npulse_hand_init(void);
void npulse_hand_stop(void);
void npulse_hand_step(uint32_t ms,const double q[8],const double v[8],const double u[8],
 int enabled,uint64_t session,uint32_t run,uint64_t revision,int action,
 uint8_t target_mask,const double targets[8],uint8_t limit,int preserve_others,
 struct npulse_hand_result *out);
#ifdef __cplusplus
}
#endif
