#ifndef N_PULSE_FATAL_STOP_H
#define N_PULSE_FATAL_STOP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    N_PULSE_FATAL_ERROR = 1, N_PULSE_FATAL_HARDFAULT,
    N_PULSE_FATAL_MEMMANAGE, N_PULSE_FATAL_BUSFAULT, N_PULSE_FATAL_USAGEFAULT,
    N_PULSE_FATAL_ASSERT, N_PULSE_FATAL_MALLOC, N_PULSE_FATAL_STACK,
    N_PULSE_FATAL_NMI, N_PULSE_FATAL_HAL_ASSERT, N_PULSE_FATAL_UNHANDLED_IRQ
} n_pulse_fatal_cause_t;
/* Best-effort RAM diagnostic, written AFTER the hardware cut. Reset clears it. */
extern volatile uint32_t g_n_pulse_fatal_cause;
/* Leaves PRIMASK set. Separate from the terminal loop to test production writes. */
void N_PULSE_FatalCutoff(n_pulse_fatal_cause_t cause);
void N_PULSE_FatalHalt(n_pulse_fatal_cause_t cause) __attribute__((noreturn));
void N_PULSE_FatalUnhandledIRQ(void) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
#endif
