#include "comm.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "motor_safety.h"

#define UART_START_BYTE 0xAA
#define COMMS_QUEUE_LENGTH 8
#define COMMS_HEARTBEAT_MS 100U

static QueueHandle_t s_intent_queue;
static TimerHandle_t s_heartbeat_timer;
static bool          s_heartbeat_started = false;

typedef enum {
    STATE_WAIT_START,
    STATE_GET_TYPE,
    STATE_GET_STRENGTH,
    STATE_GET_POSITIONS,
    STATE_GET_CRC
} uart_state_t;

// Prototype for the hardware-specific read (to be linked to HAL_UART_Receive)
extern uint8_t UART_ReadByte(void); 

static void Comms_HeartbeatTimeout(TimerHandle_t timer) {
    s_heartbeat_started = false;
    MotorSafety_RequestStop();
}

void Comms_Init(void) {
    s_intent_queue = xQueueCreate(COMMS_QUEUE_LENGTH, sizeof(intent_t));
    s_heartbeat_timer = xTimerCreate("HB", pdMS_TO_TICKS(COMMS_HEARTBEAT_MS), pdFALSE, NULL, Comms_HeartbeatTimeout);
}

static void enqueue_intent(const intent_t *intent) {
    if (xQueueSendToBack(s_intent_queue, intent, 0) == pdPASS) {
        if (!s_heartbeat_started) {
            xTimerStart(s_heartbeat_timer, 0);
            s_heartbeat_started = true;
        } else {
            xTimerReset(s_heartbeat_timer, 0);
        }
    }
}

void Comms_Task(void *argument) {
    uart_state_t state = STATE_WAIT_START;
    intent_t incoming;
    uint8_t pos_idx = 0;
    uint8_t crc = 0;

    while (1) {
        uint8_t byte = UART_ReadByte(); // Blocks or waits for notification
        
        switch (state) {
            case STATE_WAIT_START:
                if (byte == UART_START_BYTE) {
                    state = STATE_GET_TYPE;
                    crc = 0; // Reset checksum
                }
                break;

            case STATE_GET_TYPE:
                incoming.id = (intent_id_t)byte;
                crc ^= byte;
                state = (incoming.id == INTENT_DIRECT_CONTROL) ? STATE_GET_POSITIONS : STATE_GET_STRENGTH;
                pos_idx = 0;
                break;

            case STATE_GET_STRENGTH:
                incoming.strength = byte;
                crc ^= byte;
                state = STATE_GET_CRC;
                break;

            case STATE_GET_POSITIONS:
                // Handle Little-Endian Q15 (2 bytes per motor * 5 motors = 10 bytes)
                ((uint8_t*)incoming.positions)[pos_idx++] = byte;
                crc ^= byte;
                if (pos_idx >= 10) state = STATE_GET_CRC;
                break;

            case STATE_GET_CRC:
                if (byte == crc) {
                    enqueue_intent(&incoming);
                }
                state = STATE_WAIT_START;
                break;
        }
    }
}