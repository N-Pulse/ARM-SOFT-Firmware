#include "main.h"
#include "bench_hw.h"
#include "stm32g4xx_it.h"
void NMI_Handler(void) { BenchHW_Fatal(); }
void HardFault_Handler(void) { BenchHW_Fatal(); }
void MemManage_Handler(void) { BenchHW_Fatal(); }
void BusFault_Handler(void) { BenchHW_Fatal(); }
void UsageFault_Handler(void) { BenchHW_Fatal(); }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }
void SysTick_Handler(void)
{
    HAL_IncTick();
    BenchHW_TickISR(HAL_GetTick());
}
void LPUART1_IRQHandler(void) { BenchHW_UART_ISR(); }
