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

/* Ring buffer filled by LPUART1_IRQHandler in stm32g4xx_it.c. */
extern volatile uint8_t  g_rxring[];
extern volatile uint16_t g_rxring_head;
extern volatile uint16_t g_rxring_tail;
#define RXRING_SIZE  256U

static bool raw_uart_read_byte(uint8_t *out_b)
{
    if (g_rxring_head == g_rxring_tail) return false;  /* empty */
    *out_b = g_rxring[g_rxring_tail];
    g_rxring_tail = (uint16_t)((g_rxring_tail + 1U) % RXRING_SIZE);
    return true;
}

/* Comms_Task: pulls bytes from UART, decodes proto, pushes intents to queue. */
static void Comms_Task(void *argument)
{
    (void)argument;
    static uint8_t s_rx_buffer[RX_BUFFER_SIZE];

    /* CA = task is alive (one-shot). */
    emit_debug_beacon(0xCAU, 0x60U);

    for (;;)
    {
        uint8_t b = 0;

        /* Wait for sync byte 0xAA (no TX inside this loop — would overrun RX). */
        if (!raw_uart_read_byte(&b)) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        if (b != 0xAAU) continue;  /* not the sync, drop and look again */

        /* Got sync — read length + payload in tight loop, NO TX in-between
         * (TX would consume bandwidth and let the FIFO overrun on bursts). */
        uint8_t rx_len = 0;
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(100);
        while (!raw_uart_read_byte(&rx_len)) {
            if (xTaskGetTickCount() >= deadline) goto resync;
        }
        if (rx_len == 0U || rx_len > RX_BUFFER_SIZE) continue;

        uint8_t got = 0;
        deadline = xTaskGetTickCount() + pdMS_TO_TICKS(100);
        while (got < rx_len) {
            if (raw_uart_read_byte(&s_rx_buffer[got])) {
                got++;
            } else if (xTaskGetTickCount() >= deadline) {
                goto resync;
            }
        }

        /* Frame complete — only NOW do we emit a beacon (TX is slow, would
         * overrun a back-to-back incoming burst). */
        emit_debug_beacon(0xC0U, 0xA2U);  /* C0 = full frame captured */

        rx_result_t result = HandleDeviceMessage(s_rx_buffer, rx_len);

        if (result.type == RX_TYPE_NONE) {
            emit_debug_beacon(0xC3U, 0x76U);   /* C3 = parse returned NONE */
        }
        else if (result.type == RX_TYPE_ACTION) {
            intent_t intent = { .id = (intent_id_t)result.data.action_id };
            (void)xQueueSendToBack(s_intent_queue, &intent, 0);
            emit_debug_beacon(0xC1U, 0xEEU);   /* C1 = intent queued */
            /* Watchdog disabled — was firing ACTION_UNKNOWN after 2s of silence
             * which then puts SafetyTask into ALL_STOP spam.  Re-enable later
             * when the EMG side actually streams periodic intents. */
            /* xTimerReset(s_heartbeat_timer, 0); */
        }
        else if (result.type == RX_TYPE_HELLO) {
            emit_debug_beacon(0xC2U, 0x3AU);   /* C2 = hello decoded */
        }
        continue;

    resync:
        /* Timeout mid-frame: log and look for the next sync byte. */
        emit_debug_beacon(0xCFU, 0x01U);   /* CF = packetizer timeout */
    }
}

void Comms_TaskCreate(void)
{
    /* Priority above ControlTask (3) so RX byte polling isn't starved. */
    BaseType_t status = xTaskCreate(Comms_Task,
                                    "comms",
                                    512,
                                    NULL,
                                    tskIDLE_PRIORITY + 4,
                                    NULL);
    configASSERT(status == pdPASS);
}
