#ifndef USART_USB_BSP_H
#define USART_USB_BSP_H

#include <stdint.h>
#include <stdbool.h>

// UART BSP — physical transport for the comm-stack (protobuf messages).
//
// TODO: pick the UART instance wired to the BLE module / USB-UART bridge on
//       your PCB, then implement usart_usb_bsp.c.
// The comm-stack's ReceiveMessage() will call USART_USB_BSP_Init() and rely
// on USART_USB_BSP_RxCpltCallback() to be wired into the HAL UART ISR.

void USART_USB_BSP_Init(void);
bool USART_USB_BSP_Transmit(const uint8_t *buf, uint16_t len);

// Wire this to HAL_UART_RxCpltCallback() in stm32g4xx_it.c (or via __weak override).
void USART_USB_BSP_RxCpltCallback(uint8_t byte);

#endif /* USART_USB_BSP_H */
