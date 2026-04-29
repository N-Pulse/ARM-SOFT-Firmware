#include "comm.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "motor_safety.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "rx.h" 

#ifdef __cplusplus
}
#endif


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

static uint8_t s_rx_buffer[RX_BUFFER_SIZE];
static uint8_t s_rx_length = 0;


static void Comms_Task(void *argument)
{
    (void)argument;

    while (1)
    {
        
        if (ReceiveMessage(s_rx_buffer, s_rx_length))
        {
            
            HandleReceivedMessage(s_rx_buffer, s_rx_length);
            
            
        }
        
        // Minor yield to prevent watchdog issues if UART is flooded
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}