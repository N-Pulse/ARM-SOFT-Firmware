#ifndef BENCH_PROTOCOL_H
#define BENCH_PROTOCOL_H
#include <stdint.h>

/* One foreground owner calls all functions. No UART or blocking I/O here. */
void BenchProtocol_Init(uint32_t now_ms);
void BenchProtocol_Byte(uint8_t byte, uint32_t now_ms);
void BenchProtocol_Service(uint32_t now_ms);
#endif
