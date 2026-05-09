#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "finger_test.h"
#include "main.h"

extern UART_HandleTypeDef hcom_uart[COMn];

#define APP_BEACON(id, crc) \
    do { \
        uint8_t _b[] = {0xAAU, 0x01U, (id), 0x00U, (crc)}; \
        HAL_UART_Transmit(&hcom_uart[COM1], _b, sizeof(_b), 50U); \
    } while(0)

/**
 * @brief Initializes all high-level application modules.
 * Called once from StartDefaultTask after the FreeRTOS scheduler is running.
 */
void App_Init(void)
{
    APP_BEACON(0xFCU, 0x14U);
    Motor_InitAll();
    APP_BEACON(0xFBU, 0xEDU);
    Finger_Test_Init();
    APP_BEACON(0xFAU, 0x77U);   /* FA = single-finger L298N test bench up */
    Comms_Init();
}

/**
 * @brief Main application logic loop, called from ControlTask at 100 Hz.
 * Drains the FreeRTOS intent queue (filled by Comms_Task) and routes each
 * intent to motor commands via IntentRouter_Handle().
 */
void App_Task(void)
{
    intent_t intent;
    if (Comms_GetNextIntent(&intent))
    {
        IntentRouter_Handle(&intent);
    }
}
