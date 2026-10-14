#include "old_hw_timing.h"
#if N_PULSE_TIMING_DIAGNOSTICS
#include "main.h"
volatile old_hw_timing_sample_t g_old_hw_timing[OLD_TIME_COUNT];
volatile uint32_t g_old_hw_primask_max_cycles,g_old_hw_primask_count,g_old_hw_timing_enabled;
static uint32_t outer_start;
void OldHW_TimingInit(void)
{
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT=0; DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
    g_old_hw_timing_enabled=(DWT->CTRL&DWT_CTRL_CYCCNTENA_Msk)!=0;
}
uint32_t OldHW_TimingBegin(unsigned id)
{
    if(!g_old_hw_timing_enabled || id>=OLD_TIME_COUNT) return 0;
    uint32_t now=DWT->CYCCNT; volatile old_hw_timing_sample_t *s=&g_old_hw_timing[id];
    if(s->count) { uint32_t gap=now-s->last_enter; if(gap>s->max_gap_cycles) s->max_gap_cycles=gap; }
    s->last_enter=now; if(s->count!=UINT32_MAX) ++s->count;
    if(id==N_PULSE_TIMING_MARKER) GPIOA->BSRR=GPIO_PIN_5;
    return now;
}
void OldHW_TimingEnd(unsigned id,uint32_t start)
{
    if(!g_old_hw_timing_enabled || id>=OLD_TIME_COUNT) return;
    uint32_t duration=DWT->CYCCNT-start;
    if(duration>g_old_hw_timing[id].max_cycles) g_old_hw_timing[id].max_cycles=duration;
    if(id==N_PULSE_TIMING_MARKER) GPIOA->BSRR=(uint32_t)GPIO_PIN_5<<16;
}
void OldHW_TimingLockBegin(uint32_t previous)
{
    if(!previous && g_old_hw_timing_enabled) {
        outer_start=DWT->CYCCNT;
        if(g_old_hw_primask_count!=UINT32_MAX) ++g_old_hw_primask_count;
    }
}
void OldHW_TimingLockEnd(uint32_t previous)
{
    if(!previous && g_old_hw_timing_enabled) {
        uint32_t elapsed=DWT->CYCCNT-outer_start;
        if(elapsed>g_old_hw_primask_max_cycles) g_old_hw_primask_max_cycles=elapsed;
    }
}
#endif
