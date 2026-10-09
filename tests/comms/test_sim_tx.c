/* Historical v1 fixture: intentionally superseded by tests/simulation native
 * saturation/in-flight/timeout cases for the incompatible v2 protocol.
 * Kept for history; the runner executes its v2 replacements. */
#if 0
/* Production SIM staging/encoding only; no UART hardware or SIM RX. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#define MOTOR_BACKEND_SIM 1
#include "../../fw/motors/motor_control/motor_backend_sim.c"
static uint32_t mask, stop_epoch;
static bool allowed = true, full;
static uint8_t offered[130];
static size_t offered_size;
static unsigned offers, notifications, iterations;
static jmp_buf task_exit;
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask = 1U; }
void __set_PRIMASK(uint32_t old) { mask = old; }
uint32_t MotorSafety_EpochLocked(void) { assert(mask); return stop_epoch; }
bool MotorSafety_CanArmLocked(void) { assert(mask); return allowed; }
bool Motor_OutputAllowedLocked(motor_id_t id) { assert(mask && (unsigned)id < MOTOR_COUNT); return allowed; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, unsigned stack,
                       void *arg, unsigned priority, TaskHandle_t *handle)
{
    assert(fn == sim_worker && !strcmp(name, "simtx") && priority < 3U);
    (void)stack; (void)arg; *handle = (void *)1; return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t handle) { assert(handle && !mask); ++notifications; }
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait)
{
    assert(clear && wait == 10U && !mask);
    if (++iterations > 1U) longjmp(task_exit, 1);
    return 1U;
}
bool UART4Transport_QueueFrameAtEpoch(const uint8_t *frame, size_t length, uint32_t epoch)
{
    assert(!mask && length <= sizeof(offered));
    if (full || epoch != stop_epoch) return false;
    memcpy(offered, frame, length); offered_size = length; ++offers;
    return true;
}
static void print_frame(const char *name)
{
    printf("%s=", name);
    for (size_t i = 0; i < offered_size; ++i) printf("%02x", offered[i]);
    putchar('\n');
}
int main(void)
{
    MotorBackend_Init(); assert(!offers && !pending_count && !preview_count);
    MotorBackend_ApplyOutput(MOTOR_THUMB, 0.0f, 40.0f);
    MotorBackend_Flush(); assert(notifications == 1U);
    send_pending(); assert(offers == 1U && !pending_count); print_frame("TARGET");
    MotorBackend_SendPreview(MOTOR_THUMB, 0.0f, 50U);
    if (setjmp(task_exit) == 0) sim_worker(NULL);
    assert(offers == 2U && !preview_count); print_frame("PREVIEW");
    MotorBackend_ApplyOutput(MOTOR_WRIST_Y, 0.0f, -80.0f);
    MotorBackend_SendPreview(MOTOR_WRIST_X, 0.0f, 10U);
    uint32_t key = Motor_Lock();
    ++stop_epoch; allowed = false; MotorBackend_InhibitOutputsLocked();
    Motor_Unlock(key);
    assert(!pending_count && !preview_count); send_pending(); assert(offers == 2U);
    MotorBackend_StopAll(); assert(offers == 3U); print_frame("STOP");
    /* The shared transport owns rejection/error diagnostics; no UART retry or
     * blocking work occurs locally when its queue is saturated. */
    full = true; MotorBackend_StopAll(); assert(offers == 3U && !mask);
    puts("SIM TX staging: complete frames, stop discards pending wrists/previews, saturation returns without waiting passed.");
    return 0;
}

#endif
