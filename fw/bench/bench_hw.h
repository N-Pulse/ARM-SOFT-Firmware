#ifndef BENCH_HW_H
#define BENCH_HW_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
    BENCH_ASYNC_ESTOP = 1U, BENCH_ASYNC_RX = 2U,
    BENCH_ASYNC_LOOP = 4U, BENCH_ASYNC_LINK = 8U,
    BENCH_ASYNC_UART = 16U, BENCH_ASYNC_ENCODER = 32U
};
/* Standalone NUCLEO-G474RE. No other board, motor index, ADC or RTOS. */
void BenchHW_Init(void);
uint32_t BenchHW_Counter(void);
bool BenchHW_EncoderRunning(void);
bool BenchHW_ReadByte(uint8_t *byte);
bool BenchHW_Write(const char *data, size_t length);
bool BenchHW_Arm(uint32_t now_ms);
void BenchHW_Disable(void);
void BenchHW_SetMotor(float signed_percent);
float BenchHW_AppliedPWM(void);
void BenchHW_Heartbeat(uint32_t now_ms);
void BenchHW_ControlAlive(uint32_t now_ms);
uint32_t BenchHW_TakeAsyncFault(void);
/* Register-only shutdown, valid even before HAL initialization. */
void BenchHW_CutoffRegisters(void);
void BenchHW_TickISR(uint32_t now_ms);
void BenchHW_UART_ISR(void);
_Noreturn void BenchHW_Fatal(void);
#endif
