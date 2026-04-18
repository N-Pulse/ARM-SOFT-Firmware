#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"

/**
 * @brief Initializes all high-level application modules.
 * Called once during system startup after the HAL and RTOS are ready.
 */
void App_Init(void)
{
    /* Initialize motor controllers and safety limits */
    Motor_InitAll();

    /* Initialize UART communication and the FreeRTOS intent queue */
    Comms_Init();
}

/**
 * @brief Main application logic loop.
 * This is typically called from a dedicated FreeRTOS task.
 */
void App_Task(void)
{
    intent_t intent;

    /* * Non-blocking check for new messages from the UART parser.
     * If a valid packet (Gesture or Direct Positions) was received, 
     * it is passed to the Router.
     */
    if (Comms_GetNextIntent(&intent)) 
    {
        IntentRouter_Handle(&intent);
    }
}