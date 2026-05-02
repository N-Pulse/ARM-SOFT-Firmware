#include "cmsis_os2.h"    // Use CMSIS-RTOS2 for FreeRTOS
#include "intent_router.h"

/* The queue handle is defined in comm.c and accessed here */
extern osMessageQueueId_t s_intent_queue;

/**
 * @brief Main Application Task.
 * This task blocks until an intent is received, ensuring low CPU usage.
 */
void Start_App_Task(void *argument) {
    intent_t current_intent;

    /* Ensure the queue exists before starting the loop */
    while (s_intent_queue == NULL) {
        osDelay(10);
    }

    for (;;) {
        /* Wait forever (osWaitForever) for an intent from the comms task */
        osStatus_t status = osMessageQueueGet(s_intent_queue, &current_intent, NULL, osWaitForever);

        if (status == osOK) {
            /* 
             * Execute the motor logic. 
             * Note: In a real system, you might add logic here to check
             * if the hand is currently in a "Global Stop" state.
             */
            IntentRouter_Handle(&current_intent);
        }
    }
}

