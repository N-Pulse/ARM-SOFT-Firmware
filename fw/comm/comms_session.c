#include "comms_session.h"
#include "comms_config.h"
#include "motor_safety.h"
#include "motor_lock.h"
#include <string.h>

static intent_t intents[COMMS_INTENT_CAPACITY];
static unsigned head, count;
static bool enabled, session, lost;
static uint32_t fresh_ms, peer_id;
static comms_diagnostics_t diag;

void CommsSession_Init(bool hw)
{
    uint32_t key = Motor_Lock();
    head = count = 0U; enabled = hw; session = lost = false;
    fresh_ms = peer_id = 0U;
    memset(&diag, 0, sizeof(diag));
    Motor_Unlock(key);
}
void CommsSession_OnStopLocked(void)
{
    diag.discarded += count;
    head = count = 0U;
}
static bool fresh(uint32_t now)
{
    return enabled && session && !lost &&
           (uint32_t)(now - fresh_ms) <= COMMS_LINK_TIMEOUT_MS;
}
void CommsSession_PollLocked(uint32_t now)
{
    if (enabled && session && !lost && !fresh(now)) {
        lost = true;
        ++diag.link_timeouts;
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK, MOTOR_COUNT);
    }
}
void CommsSession_TransportErrorLocked(void)
{
    ++diag.transport_errors;
    if (enabled && session) {
        lost = true;
        MotorSafety_RequestStopCauseLocked(MOTOR_STOP_LINK, MOTOR_COUNT);
    }
    CommsSession_OnStopLocked();
}
bool CommsSession_CanRecoverLocked(uint32_t now)
{
    /* Once a HW session has begun, any recovery requires a healthy link. */
    return !enabled || !session || fresh(now);
}
bool CommsSession_OnMessage(const rx_result_t *msg, uint32_t ms,
                            uint32_t epoch, bool *started)
{
    if (started) *started = false;
    uint32_t key = Motor_Lock();
    ++diag.received;
    uint32_t now = HAL_GetTick();
    CommsSession_PollLocked(now);
    if (!enabled || !msg || msg->type == RX_TYPE_NONE) {
        ++diag.rejected; Motor_Unlock(key); return false;
    }
    ++diag.valid;
    if (epoch != MotorSafety_EpochLocked() || (uint32_t)(now - ms) > COMMS_LINK_TIMEOUT_MS) {
        ++diag.stale; Motor_Unlock(key); return false;
    }
    if (msg->type == RX_TYPE_HELLO && msg->proto_version == 1U) {
        if (session && msg->data.device_id != peer_id) {
            ++diag.rejected; Motor_Unlock(key); return false;
        }
        bool begin = !session || lost;
        if (begin) CommsSession_OnStopLocked();
        session = true; lost = false; peer_id = msg->data.device_id; fresh_ms = ms;
        ++diag.hello; ++diag.accepted;
        if (started) *started = begin;
        Motor_Unlock(key); return true;
    }
    if (msg->type != RX_TYPE_ACTION || msg->data.action_id > ACTION_ROTATE_WRIST_L ||
        !fresh(now) || !MotorSafety_CanArmLocked()) {
        ++diag.rejected; Motor_Unlock(key); return false;
    }
    ++diag.accepted;
    if (count == COMMS_INTENT_CAPACITY) {
        ++diag.queue_full; Motor_Unlock(key); return false;
    }
    intents[(head + count) % COMMS_INTENT_CAPACITY] =
        (intent_t){ .id = (intent_id_t)msg->data.action_id, .epoch = epoch };
    ++count; ++diag.queued;
    fresh_ms = ms; /* ONLY queued, admissible commands renew the session. */
    Motor_Unlock(key); return true;
}
bool CommsSession_GetIntent(intent_t *out)
{
    if (!out) return false;
    uint32_t key = Motor_Lock();
    CommsSession_PollLocked(HAL_GetTick());
    bool result = false;
    if (count) {
        intent_t item = intents[head]; head = (head + 1U) % COMMS_INTENT_CAPACITY; --count;
        if (item.epoch == MotorSafety_EpochLocked() && MotorSafety_CanArmLocked() && fresh(HAL_GetTick())) {
            *out = item; result = true;
        } else ++diag.stale;
    }
    Motor_Unlock(key); return result;
}
void CommsSession_GetDiagnostics(comms_diagnostics_t *out)
{
    if (!out) return;
    uint32_t key = Motor_Lock(); *out = diag; Motor_Unlock(key);
}
