#include "board_link.h"
#include "stm32g4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <math.h>

/* The USART3 ring buffer is filled by USART3_IRQHandler (in stm32g4xx_it.c)
 * and drained by BoardLink_RxTask below. Same pattern as the LPUART1 ring
 * for the host comm-stack. */
extern volatile uint8_t  g_usart3_rxring[];
extern volatile uint16_t g_usart3_rxring_head;
extern volatile uint16_t g_usart3_rxring_tail;
#define USART3_RXRING_SIZE  256U

#define BOARD_LINK_SYNC      0xAAU
#define BOARD_LINK_PAYLOAD   4U     /* motor_id + pos_h + pos_l + speed */

#define STRAP_GPIO_PORT      GPIOC
#define STRAP_GPIO_PIN       GPIO_PIN_0   /* Arduino A5, CN8-6 */

/* USART3 pins on Nucleo-G474RE Morpho header. */
#define LINK_GPIO_PORT       GPIOB
#define LINK_TX_PIN          GPIO_PIN_10
#define LINK_RX_PIN          GPIO_PIN_11
#define LINK_GPIO_AF         GPIO_AF7_USART3

static UART_HandleTypeDef s_huart3;
static board_role_t       s_role = BOARD_ROLE_MASTER;

/* Motherboard owns 3 motors (master): WRIST_X, THUMB, LITTLE.
 * Daughterboard owns the other 5 (slave): INDEX, MIDDLE, RING, WRIST_Y, PALM. */
static bool is_master_motor(motor_id_t id)
{
    return (id == MOTOR_WRIST_X) ||
           (id == MOTOR_THUMB)   ||
           (id == MOTOR_LITTLE);
}

bool BoardLink_IsLocalMotor(motor_id_t id)
{
    return (s_role == BOARD_ROLE_MASTER) ?  is_master_motor(id)
                                         : !is_master_motor(id);
}

board_role_t BoardLink_Role(void)
{
    return s_role;
}

static void link_strap_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = STRAP_GPIO_PIN;
    HAL_GPIO_Init(STRAP_GPIO_PORT, &g);

    /* Settle time for the pull-up to drive the line. */
    for (volatile int i = 0; i < 1000; ++i) { __NOP(); }

    s_role = (HAL_GPIO_ReadPin(STRAP_GPIO_PORT, STRAP_GPIO_PIN) == GPIO_PIN_RESET)
             ? BOARD_ROLE_SLAVE : BOARD_ROLE_MASTER;
}

static void link_uart_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = LINK_GPIO_AF;
    g.Pin       = LINK_TX_PIN | LINK_RX_PIN;
    HAL_GPIO_Init(LINK_GPIO_PORT, &g);

    s_huart3.Instance                    = USART3;
    s_huart3.Init.BaudRate               = 115200U;
    s_huart3.Init.WordLength             = UART_WORDLENGTH_8B;
    s_huart3.Init.StopBits               = UART_STOPBITS_1;
    s_huart3.Init.Parity                 = UART_PARITY_NONE;
    s_huart3.Init.Mode                   = UART_MODE_TX_RX;
    s_huart3.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    s_huart3.Init.OverSampling           = UART_OVERSAMPLING_16;
    s_huart3.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    s_huart3.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    s_huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&s_huart3);

    /* Enable RX-not-empty IRQ and the FIFO, same as LPUART1. */
    s_huart3.Instance->CR1 &= ~USART_CR1_UE;
    s_huart3.Instance->CR1 |= USART_CR1_FIFOEN;
    s_huart3.Instance->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
    s_huart3.Instance->CR1 |= USART_CR1_UE;

    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

void BoardLink_Init(void)
{
    link_strap_init();
    link_uart_init();
}

/* ------------------------------------------------------------------------
 * Master TX: encode + transmit a motor command.
 * ------------------------------------------------------------------------ */

static int16_t rad_to_q15(float rad)
{
    /* Clamp to [-pi, +pi] then scale. */
    const float PI = 3.14159265358979f;
    if (rad >  PI) rad =  PI;
    if (rad < -PI) rad = -PI;
    float scaled = rad * (32767.0f / PI);
    if (scaled >  32767.0f) scaled =  32767.0f;
    if (scaled < -32768.0f) scaled = -32768.0f;
    return (int16_t)scaled;
}

void BoardLink_SendMotorCmd(motor_id_t id, float target_rad, uint8_t speed_percent)
{
    if (s_role != BOARD_ROLE_MASTER) return;

    int16_t q15 = rad_to_q15(target_rad);
    uint8_t frame[2 + BOARD_LINK_PAYLOAD] = {
        BOARD_LINK_SYNC,
        BOARD_LINK_PAYLOAD,
        (uint8_t)id,
        (uint8_t)((q15 >> 8) & 0xFFU),   /* big-endian on the wire */
        (uint8_t)(q15 & 0xFFU),
        speed_percent,
    };
    /* Blocking send — the frame is 6 bytes, ~520 us at 115200 baud. */
    HAL_UART_Transmit(&s_huart3, frame, sizeof(frame), 50U);
}

/* ------------------------------------------------------------------------
 * Slave RX: drain ring, parse frames, dispatch.
 * ------------------------------------------------------------------------ */

static bool rxring_pop(uint8_t *out_b)
{
    if (g_usart3_rxring_head == g_usart3_rxring_tail) return false;
    *out_b = g_usart3_rxring[g_usart3_rxring_tail];
    g_usart3_rxring_tail = (uint16_t)((g_usart3_rxring_tail + 1U)
                                      % USART3_RXRING_SIZE);
    return true;
}

static float q15_to_rad(int16_t q15)
{
    const float PI = 3.14159265358979f;
    return (float)q15 * (PI / 32767.0f);
}

static void dispatch_motor_cmd(uint8_t motor_id, int16_t q15, uint8_t speed_pct)
{
    if (motor_id >= MOTOR_COUNT) return;
    /* Local set on the slave side. Routing logic in Motor_SetTarget knows
     * the slave owns these; it won't bounce back over the link. */
    Motor_SetTarget((motor_id_t)motor_id, q15_to_rad(q15), speed_pct);
}

static void BoardLink_RxTask(void *argument)
{
    (void)argument;
    for (;;) {
        uint8_t b = 0;
        if (!rxring_pop(&b)) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }
        if (b != BOARD_LINK_SYNC) continue;

        uint8_t len = 0;
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(50);
        while (!rxring_pop(&len)) {
            if (xTaskGetTickCount() >= deadline) goto resync;
        }
        if (len != BOARD_LINK_PAYLOAD) continue;

        uint8_t buf[BOARD_LINK_PAYLOAD];
        uint8_t got = 0;
        deadline = xTaskGetTickCount() + pdMS_TO_TICKS(50);
        while (got < BOARD_LINK_PAYLOAD) {
            if (rxring_pop(&buf[got])) got++;
            else if (xTaskGetTickCount() >= deadline) goto resync;
        }

        int16_t q15 = (int16_t)(((uint16_t)buf[1] << 8) | (uint16_t)buf[2]);
        dispatch_motor_cmd(buf[0], q15, buf[3]);
        continue;

    resync:
        /* Mid-frame timeout — fall back to looking for the next SYNC. */
        continue;
    }
}

void BoardLink_TaskCreate(void)
{
    if (s_role != BOARD_ROLE_SLAVE) return;
    BaseType_t status = xTaskCreate(BoardLink_RxTask,
                                    "boardlink",
                                    512,
                                    NULL,
                                    tskIDLE_PRIORITY + 4,
                                    NULL);
    configASSERT(status == pdPASS);
}
