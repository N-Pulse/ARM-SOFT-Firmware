#include "app.h"
#include "motor_control.h"
#include "comm.h"
#include "intent_router.h"
#include "main.h"

extern UART_HandleTypeDef hcom_uart[COMn];

/* Send a beacon byte safely — caller must mask SysTick to prevent
 * pre-scheduler xPortSysTickHandler from corrupting FreeRTOS state. */
static inline void app_tx_byte_safe(uint8_t b)
{
    USART_TypeDef *u = LPUART1;
    uint32_t tries = 0;
    while ((u->ISR & (1U << 7)) == 0U) {
        if (++tries > 1000000U) return;
        __asm__ volatile ("nop");
    }
    u->TDR = b;
}

#define APP_BEACON(id, crc) \
    do { \
        SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; \
        app_tx_byte_safe(0xAAU); \
        app_tx_byte_safe(0x01U); \
        app_tx_byte_safe((id)); \
        app_tx_byte_safe(0x00U); \
        app_tx_byte_safe((crc)); \
        SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk; \
    } while(0)

/**
 * @brief Initializes all high-level application modules.
 * Called once during system startup after the HAL and RTOS are ready.
 */
void App_Init(void)
{
    APP_BEACON(0xFCU, 0x14U);
    Motor_InitAll();
    APP_BEACON(0xFBU, 0xEDU);
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