#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "board_link.h"
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
    BoardLink_Init();   /* read strap pin -> role; bring up USART3 */
    /* Beacon role: F8 = master, F9 = slave. */
    APP_BEACON((BoardLink_Role() == BOARD_ROLE_MASTER) ? 0xF8U : 0xF9U, 0x4DU);
    Motor_InitAll();    /* includes Motor_L298N_Init for locally-owned motors */
    APP_BEACON(0xFBU, 0xEDU);
    Comms_Init();
    /* NB : BoardLink_TaskCreate() (RX framé slave) est démarré dans
     * main.c::CreateTasks() en mode pipeline proto uniquement. En mode
     * MOTOR_CALIB_MODE, on a besoin de USART3 comme tunnel ASCII brut —
     * pas de RX task framé qui drainerait le ring buffer en concurrence. */
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
