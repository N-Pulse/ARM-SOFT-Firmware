#include "board_uart_compat.h"

#if N_PULSE_OLD_HW
extern UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef *BoardUART_GetHandle(void) { return &hlpuart1; }
#else
extern UART_HandleTypeDef huart4;
UART_HandleTypeDef *BoardUART_GetHandle(void) { return &huart4; }
#endif

void BoardUART_Compat_Init(void)
{
    /* Handle already initialized by main/OldHW_Init. Never copy a HAL handle. */
}
