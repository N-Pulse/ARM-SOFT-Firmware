#include "rx.h"
#include "intent_router.h"
#include "cmsis_os2.h" // Using CMSIS-RTOS2/FreeRTOS

osMessageQueueId_t s_intent_queue;
osTimerId_t s_heartbeat_timer;

void Heartbeat_Callback(void *argument) {
    // If no message in 2 seconds, send "UNKNOWN" to stop motors
    intent_t safety = { .id = ACTION_UNKNOWN };
    osMessageQueuePut(s_intent_queue, &safety, 0, 0);
}

void Comm_Init(void) {
    s_intent_queue = osMessageQueueNew(10, sizeof(intent_t), NULL);
    s_heartbeat_timer = osTimerNew(Heartbeat_Callback, osTimerPeriodic, NULL, NULL);
}

void Start_Comms_Task(void *argument) {
    uint8_t raw_buf[RX_BUFFER_SIZE];
    uint8_t raw_len;

    for (;;) {
        // Step 1: Check for framed packet
        if (ReceiveMessage(raw_buf, &raw_len)) {
            
            // Step 2: Use the Parser
            rx_result_t result = HandleDeviceMessage(raw_buf, raw_len);

            // Step 3: Handle the result
            if (result.type == RX_TYPE_ACTION) {
                intent_t intent = { .id = (intent_id_t)result.data.action_id };
                osMessageQueuePut(s_intent_queue, &intent, 0, 0);
                
                // Refresh the safety watchdog
                osTimerStart(s_heartbeat_timer, 2000); 
            }
        }
        osDelay(1); // Don't hog the CPU
    }
}