#ifndef BOARD_UART_COMPAT_H
#define BOARD_UART_COMPAT_H

#include "stm32g4xx_hal.h"

enum
{
    COM1 = 0,
    COMn = 1
};

/* Single HAL instance, also used by IRQ and transport callbacks. */
extern UART_HandleTypeDef huart4;
UART_HandleTypeDef *BoardUART_GetHandle(void);

void BoardUART_Compat_Init(void);

#endif /* BOARD_UART_COMPAT_H */
