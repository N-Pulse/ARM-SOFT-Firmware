#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor_sim_feedback.h"
#include "motor_backend.h"
static unsigned deliveries;
void MotorBackend_OnFeedback(const motor_feedback_t *fb)
{ assert((unsigned)fb->id < MOTOR_COUNT); ++deliveries; }
int main(void)
{
    /* Exact allocations make short-buffer accesses visible under ASan. */
    uint8_t *p = malloc(5U); memset(p, 0, 5U); p[0] = 52U;
    assert(!process_joint_feedback(p, 5U)); free(p);
    p = malloc(3U); memset(p, 0, 3U); p[0] = 86U;
    assert(!process_fsr_feedback(p, 3U)); free(p);
    assert(!process_joint_feedback(NULL, 0)); assert(!process_fsr_feedback(NULL, 0));
    uint8_t joints[41] = {8}; uint8_t forces[25] = {8};
    for (unsigned i = 0; i < 8U; ++i) { joints[1U+i*5U] = i; forces[1U+i*3U] = i; }
    assert(process_joint_feedback(joints, sizeof(joints))); assert(deliveries == 8U);
    assert(process_fsr_feedback(forces, sizeof(forces))); assert(deliveries == 16U);
    joints[1U + 7U*5U] = 8U; forces[1U] = 255U;
    assert(!process_joint_feedback(joints, sizeof(joints)));
    assert(!process_fsr_feedback(forces, sizeof(forces))); assert(deliveries == 16U);
    /* Every 8-bit count against every supported payload length. */
    for (unsigned count = 0; count <= 255U; ++count) {
        for (size_t len = 1U; len <= 96U; ++len) {
            p = calloc(len, 1U); p[0] = (uint8_t)count;
            bool joint = process_joint_feedback(p, len);
            bool fsr = process_fsr_feedback(p, len);
            assert(joint == (count <= 8U && len == 1U + count*5U));
            assert(fsr == (count <= 8U && len == 1U + count*3U));
            free(p);
        }
    }
    puts("SIM bounds: joint_count=52, count=86, IDs, truncated/capacity payloads and 24576 count/length pairs passed.");
    return 0;
}
