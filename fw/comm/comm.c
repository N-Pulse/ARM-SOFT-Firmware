#include "comm.h"
#include "intent_router.h"
#include "rx.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

extern UART_HandleTypeDef hcom_uart[COMn];

/* Debug beacon: emit a 5-byte frame [0xAA][0x01][id][0x00][crc] after we
 * successfully queue an intent. Lets the host confirm decoding worked,
 * without depending on the motor pipeline. */
static void emit_debug_beacon(uint8_t id, uint8_t crc)
{
    uint8_t b[] = { 0xAAU, 0x01U, id, 0x00U, crc };
    HAL_UART_Transmit(&hcom_uart[COM1], b, sizeof(b), 50U);
}

#define COMMS_QUEUE_LENGTH    8U
#define COMMS_WATCHDOG_MS     2000U  /* lost-link → ACTION_UNKNOWN */

static QueueHandle_t  s_intent_queue;
static TimerHandle_t  s_heartbeat_timer;

static void Comms_HeartbeatTimeout(TimerHandle_t timer)
{
    (void)timer;
    /* No traffic for COMMS_WATCHDOG_MS → drop a safety intent so motors stop. */
    intent_t safety = { .id = ACTION_UNKNOWN };
    BaseType_t higher_woken = pdFALSE;
    xQueueSendFromISR(s_intent_queue, &safety, &higher_woken);
    portYIELD_FROM_ISR(higher_woken);
}

void Comms_Init(void)
{
    s_intent_queue    = xQueueCreate(COMMS_QUEUE_LENGTH, sizeof(intent_t));
    s_heartbeat_timer = xTimerCreate("HB",
                                     pdMS_TO_TICKS(COMMS_WATCHDOG_MS),
                                     pdFALSE,           /* one-shot */
                                     NULL,
                                     Comms_HeartbeatTimeout);
    configASSERT(s_intent_queue    != NULL);
    configASSERT(s_heartbeat_timer != NULL);
}

/* Called by ControlTask (App_Task) at 100Hz to drain pending intents. */
bool Comms_GetNextIntent(intent_t *out_intent)
{
    if (out_intent == NULL) return false;
    return xQueueReceive(s_intent_queue, out_intent, 0) == pdPASS;
}

/* Legacy entry: called from the old motor_backend_sim parser path (SimTxTask).
 * Kept so both the comm-stack RX path and the existing sim parser feed the
 * same queue. Safe to call from a FreeRTOS task context (not ISR). */
void enqueue_intent_from_proto(intent_id_t id)
{
    intent_t intent = { .id = id };
    if (s_intent_queue != NULL)
    {
        (void)xQueueSendToBack(s_intent_queue, &intent, 0);
    }
}

/* Comms_Task: pulls bytes from UART via the comm-stack, decodes proto,
 * pushes intents to the FreeRTOS queue. */
static void Comms_Task(void *argument)
{
    (void)argument;
    static uint8_t s_rx_buffer[RX_BUFFER_SIZE];

    for (;;)
    {
        uint8_t rx_len = 0;
        if (ReceiveMessage(s_rx_buffer, &rx_len))
        {
            rx_result_t result = HandleDeviceMessage(s_rx_buffer, rx_len);

            if (result.type == RX_TYPE_ACTION)
            {
                intent_t intent = { .id = (intent_id_t)result.data.action_id };
                (void)xQueueSendToBack(s_intent_queue, &intent, 0);
                /* Confirm to host that decoding + queueing worked (0xC1 = INTENT_QUEUED). */
                emit_debug_beacon(0xC1U, 0xEEU);
                /* (re)start watchdog: if no new traffic for COMMS_WATCHDOG_MS,
                 * an ACTION_UNKNOWN safety intent will be enqueued. */
                xTimerReset(s_heartbeat_timer, 0);
            }
            else if (result.type == RX_TYPE_HELLO)
            {
                /* Confirm Hello received (0xC2 = HELLO_RECEIVED). */
                emit_debug_beacon(0xC2U, 0x3AU);
                /* TODO: respond with a Config message (tx.cpp::SendClassificationData). */
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(2));  /* yield when nothing pending */
        }
    }
}

void Comms_TaskCreate(void)
{
    BaseType_t status = xTaskCreate(Comms_Task,
                                    "comms",
                                    512,
                                    NULL,
                                    tskIDLE_PRIORITY + 2,
                                    NULL);
    configASSERT(status == pdPASS);
}
