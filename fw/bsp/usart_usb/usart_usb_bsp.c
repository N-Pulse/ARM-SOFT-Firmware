#include "usart_usb_bsp.h"
#include "stm32g4xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

// ─── UART assignment (TO FILL IN) ────────────────────────────────────────────
//
// Choose the UART connected to the BLE module / USB-UART bridge.
// Common choices on the STM32G474:
//   USART1 (PA9 TX / PA10 RX)  — high-speed, good for BLE at 921600 baud
//   USART2 (PA2 TX / PA3 RX)   — Nucleo ST-LINK virtual COM port (debug)
//   USART3 (PB10 TX / PB11 RX) — alternate footprint
//
// Steps:
//   1. Open .ioc → Connectivity → USARTx → Mode: Asynchronous.
//   2. Enable DMA RX in circular mode (DMAx Channel y → USARTx_RX).
//   3. Enable the global UART interrupt.
//   4. Set baud rate to match the comm-stack default (check N-Pulse/CommStack).
//   5. Replace huart2 below with the generated handle name.
//   6. In stm32g4xx_it.c (or via __weak override in this file):
//        void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
//            if (huart == &huartX) USART_USB_BSP_RxCpltCallback(s_rx_byte);
//        }
// ─────────────────────────────────────────────────────────────────────────────

// TODO: replace with the correct handle once the UART is selected in .ioc.
// extern UART_HandleTypeDef huart2;

static uint8_t s_rx_byte;

void USART_USB_BSP_Init(void)
{
    // TODO: arm the first interrupt-driven byte receive.
    //   HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1);
}

bool USART_USB_BSP_Transmit(const uint8_t *buf, uint16_t len)
{
    // TODO: transmit synchronously (blocking is fine for small frames).
    //   return HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, 10) == HAL_OK;
    (void)buf;
    (void)len;
    return false;
}

void USART_USB_BSP_RxCpltCallback(uint8_t byte)
{
    // TODO: push the received byte into the comm-stack ring buffer, then re-arm.
    //   CommStack_RxPushByte(byte);          // comm-stack API — check rx.h
    //   HAL_UART_Receive_IT(&huart2, &s_rx_byte, 1);
    (void)byte;
}
