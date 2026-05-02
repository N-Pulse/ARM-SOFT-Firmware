#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "main.h"

extern UART_HandleTypeDef hcom_uart[COMn];

#define APP_BEACON(id, crc) \
    do { \
        uint8_t _b[] = {0xAAU, 0x01U, (id), 0x00U, (crc)}; \
        HAL_UART_Transmit(&hcom_uart[COM1], _b, sizeof(_b), 20U); \
    } while(0)

/**
 * @brief Initializes all high-level application modules.
 * Called once during system startup after the HAL and RTOS are ready.
 */
void App_Init(void)
{
    APP_BEACON(0xFCU, 0x14U); /* 0xFC: App_Init entered, about to call Motor_InitAll */
    Motor_InitAll();
    APP_BEACON(0xFBU, 0xEDU); /* 0xFB: Motor_InitAll returned */
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