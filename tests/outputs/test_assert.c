#include <assert.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include "FreeRTOSConfig.h"
static jmp_buf terminal;
static n_pulse_fatal_cause_t captured;
void N_PULSE_FatalHalt(n_pulse_fatal_cause_t cause)
{
    captured=cause;
    longjmp(terminal,1);
}
int main(void)
{
    assert(configUSE_MALLOC_FAILED_HOOK==1 && configCHECK_FOR_STACK_OVERFLOW==2);
    int once=0;
    configASSERT(++once==1); assert(once==1);
    if (1) configASSERT(1); else assert(0); /* Safe statement macro. */
    if (setjmp(terminal)==0) { configASSERT(0); assert(0); }
    assert(captured==N_PULSE_FATAL_ASSERT);
    puts("PASS production configASSERT routes to fatal halt; hooks enabled, expression evaluated once");
    return 0;
}
