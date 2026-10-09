#include "board_uart_compat.h"

extern UART_HandleTypeDef huart4;

UART_HandleTypeDef *BoardUART_GetHandle(void) { return &huart4; }

void BoardUART_Compat_Init(void)
{
    /*
     * UART4 is the provisional replacement for the old Nucleo COM1 handle.
     * The external connector assignment still needs electrical confirmation.
     */
    /* Compatibility initialization only. Never copy a HAL handle. */
}
