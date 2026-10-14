#ifndef OLD_HW_TIMING_H
#define OLD_HW_TIMING_H
#include <stdint.h>
#ifndef N_PULSE_TIMING_DIAGNOSTICS
#define N_PULSE_TIMING_DIAGNOSTICS 0
#endif
#ifndef N_PULSE_TIMING_MARKER
#define N_PULSE_TIMING_MARKER 3
#endif
enum { OLD_TIME_CONTROL,OLD_TIME_SAFETY,OLD_TIME_BOARD,OLD_TIME_THUMB,
       OLD_TIME_HOST_IRQ,OLD_TIME_LINK_IRQ,OLD_TIME_STOP,OLD_TIME_COUNT };
#if N_PULSE_TIMING_DIAGNOSTICS
#if !N_PULSE_OLD_HW || defined(MOTOR_BACKEND_SIM)
#error Timing image requires physical OLD_HW; never SIM
#endif
#if N_PULSE_TIMING_MARKER < 0 || N_PULSE_TIMING_MARKER >= 7
#error Timing marker must be 0..6
#endif
typedef struct {uint32_t count,max_cycles,max_gap_cycles,last_enter;} old_hw_timing_sample_t;
extern volatile old_hw_timing_sample_t g_old_hw_timing[OLD_TIME_COUNT];
extern volatile uint32_t g_old_hw_primask_max_cycles,g_old_hw_primask_count;
extern volatile uint32_t g_old_hw_timing_enabled;
void OldHW_TimingInit(void);
uint32_t OldHW_TimingBegin(unsigned id);
void OldHW_TimingEnd(unsigned id,uint32_t start);
void OldHW_TimingLockBegin(uint32_t previous);
void OldHW_TimingLockEnd(uint32_t previous);
#endif
#endif
