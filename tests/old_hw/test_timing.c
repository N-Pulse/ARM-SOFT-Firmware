#include "old_hw_timing.h"
#include "motor_lock.h"
#include <assert.h>
#include <stdio.h>
mock_dwt_t mock_dwt; mock_core_debug_t mock_core_debug; GPIO_TypeDef mock_gpio[4];
static uint32_t masked;
uint32_t __get_PRIMASK(void) { return masked; }
void __disable_irq(void) { masked=1; }
void __set_PRIMASK(uint32_t value) { masked=value; }
int main(void)
{
    OldHW_TimingInit(); assert(g_old_hw_timing_enabled && (CoreDebug->DEMCR&CoreDebug_DEMCR_TRCENA_Msk));
    uint32_t first=OldHW_TimingBegin(OLD_TIME_THUMB); assert(GPIOA->BSRR==GPIO_PIN_5);
    DWT->CYCCNT=37; OldHW_TimingEnd(OLD_TIME_THUMB,first);
    assert(g_old_hw_timing[OLD_TIME_THUMB].max_cycles==37 && GPIOA->BSRR==((uint32_t)GPIO_PIN_5<<16));
    DWT->CYCCNT=101; first=OldHW_TimingBegin(OLD_TIME_THUMB); DWT->CYCCNT=116; OldHW_TimingEnd(OLD_TIME_THUMB,first);
    assert(g_old_hw_timing[OLD_TIME_THUMB].count==2 && g_old_hw_timing[OLD_TIME_THUMB].max_gap_cycles==101);
    DWT->CYCCNT=150; uint32_t outer=Motor_Lock(); DWT->CYCCNT=170; uint32_t inner=Motor_Lock();
    DWT->CYCCNT=180; Motor_Unlock(inner); assert(masked && !g_old_hw_primask_max_cycles);
    DWT->CYCCNT=210; Motor_Unlock(outer); assert(!masked && g_old_hw_primask_max_cycles==60 && g_old_hw_primask_count==1);
    DWT->CYCCNT=UINT32_MAX-10; first=OldHW_TimingBegin(OLD_TIME_LINK_IRQ);
    DWT->CYCCNT=9; OldHW_TimingEnd(OLD_TIME_LINK_IRQ,first); assert(g_old_hw_timing[OLD_TIME_LINK_IRQ].max_cycles==20);
    g_old_hw_timing_enabled=0; OldHW_TimingBegin(OLD_TIME_LINK_IRQ); assert(g_old_hw_timing[OLD_TIME_LINK_IRQ].count==1);
    puts("PASS bounded timing counters, nested PRIMASK, PA5 marker and cycle wrap; modeled cycles, no physical timing; exit=0");
    return 0;
}
