#include "comm.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "motor_safety.h"

// ─── Comm-stack integration ───────────────────────────────────────────────────
//
// The comm-stack submodule (fw/comm-stack, repo: N-Pulse/CommStack) must
// provide the following two functions via rx.h:
//
//   bool ReceiveMessage(uint8_t *buf, uint8_t max_len, uint8_t *out_len)
//     Blocks on a FreeRTOS semaphore signalled by the UART DMA/IT RX ISR.
//     Fills buf with the raw protobuf frame, sets *out_len, returns true when
//     a complete frame is available.
//
//   void HandleReceivedMessage(const uint8_t *buf, uint8_t len)
//     Decodes the protobuf SelectMode message and calls
//     enqueue_intent_from_proto() (defined below) with the decoded intent_id_t.
//
// To activate once the submodule is initialised:
//   1. Run:  git submodule update --init --recursive
//   2. Add   COMM_STACK_AVAILABLE   to the project's preprocessor symbols
//            (STM32CubeIDE: Project → Properties → C/C++ Build → Settings →
//             MCU GCC Compiler → Preprocessor → Defined symbols).
//   3. Add   fw/comm-stack/inc   (or wherever rx.h lives) to the include paths.
//   4. The stubs in the #ifndef block below will be excluded automatically.
// ─────────────────────────────────────────────────────────────────────────────

#define RX_BUFFER_SIZE 128U

#ifndef COMM_STACK_AVAILABLE

static bool ReceiveMessage(uint8_t *buf, uint8_t max_len, uint8_t *out_len)
{
    // TODO: remove this stub and include fw/comm-stack/rx.h instead.
    // The real implementation should block on a semaphore given by the UART ISR
    // once a full protobuf frame has been received into a DMA ring buffer.
    (void)buf;
    (void)max_len;
    *out_len = 0;
    vTaskDelay(pdMS_TO_TICKS(10)); // yield so we don't busy-spin
    return false;
}

static void HandleReceivedMessage(const uint8_t *buf, uint8_t len)
{
    // TODO: remove this stub and include fw/comm-stack/rx.h instead.
    // The real implementation decodes the proto SelectMode message and calls:
    //   enqueue_intent_from_proto((intent_id_t)decoded_mode);
    (void)buf;
    (void)len;
}

#else // COMM_STACK_AVAILABLE

#ifdef __cplusplus
extern "C" {
#endif
#include "rx.h"
#ifdef __cplusplus
}
#endif

#endif // COMM_STACK_AVAILABLE

// ─────────────────────────────────────────────────────────────────────────────

#define COMMS_QUEUE_LENGTH  8U
#define COMMS_HEARTBEAT_MS  100U

static QueueHandle_t  s_intent_queue;
static TimerHandle_t  s_heartbeat_timer;
static bool           s_heartbeat_started = false;

static void Comms_HeartbeatTimeout(TimerHandle_t timer)
{
    (void)timer;
    s_heartbeat_started = false;
    /* Disabled: this watchdog was designed for periodic-intent ROS streams.
     * For interactive SELECT_MODE commands, a single intent followed by no
     * further intent for 100ms must NOT trigger a safety stop. */
    /* MotorSafety_RequestStop(); */
}

void Comms_Init(void)
{
    s_intent_queue    = xQueueCreate(COMMS_QUEUE_LENGTH, sizeof(intent_t));
    s_heartbeat_timer = xTimerCreate("HB",
                                     pdMS_TO_TICKS(COMMS_HEARTBEAT_MS),
                                     pdFALSE,
                                     NULL,
                                     Comms_HeartbeatTimeout);
    configASSERT(s_intent_queue    != NULL);
    configASSERT(s_heartbeat_timer != NULL);
}

// Called by the comm-stack (HandleReceivedMessage in rx.cpp) when a SelectMode
// proto message is decoded. This is the bridge from the comm layer to the app.
void enqueue_intent_from_proto(intent_id_t id)
{
    intent_t intent = { .id = id };

    if (xQueueSendToBack(s_intent_queue, &intent, 0) == pdPASS)
    {
        if (!s_heartbeat_started)
        {
            xTimerStart(s_heartbeat_timer, 0);
            s_heartbeat_started = true;
        }
        else
        {
            xTimerReset(s_heartbeat_timer, 0);
        }
    }
}

// Non-blocking read called by App_Task (ControlTask, 100 Hz).
bool Comms_GetNextIntent(intent_t *out_intent)
{
    if (out_intent == NULL) return false;
    return xQueueReceive(s_intent_queue, out_intent, 0) == pdPASS;
}

static uint8_t s_rx_buffer[RX_BUFFER_SIZE];

static void Comms_Task(void *argument)
{
    (void)argument;

    while (1)
    {
        uint8_t rx_len = 0;
        // Blocks until ReceiveMessage returns a complete proto frame.
        // In stub mode this yields every 10 ms and always returns false.
        if (ReceiveMessage(s_rx_buffer, RX_BUFFER_SIZE, &rx_len))
        {
            // Decodes the frame and calls enqueue_intent_from_proto() internally.
            HandleReceivedMessage(s_rx_buffer, rx_len);
        }
    }
}

void Comms_TaskCreate(void)
{
    BaseType_t status = xTaskCreate(Comms_Task,
                                    "comms",
                                    256,
                                    NULL,
                                    tskIDLE_PRIORITY + 2,
                                    NULL);
    configASSERT(status == pdPASS);
}
