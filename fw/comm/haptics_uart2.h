#ifndef HAPTICS_UART2_H
#define HAPTICS_UART2_H
#include "stm32g4xx_hal.h"
#include <stdbool.h>
/* Dispatch shared HAL callbacks without taking ownership of UART4. */
bool HapticsUART2_RxComplete(UART_HandleTypeDef *uart);
bool HapticsUART2_Error(UART_HandleTypeDef *uart);
#endif
