#ifndef COMMS_MOCK_FREERTOS_H
#define COMMS_MOCK_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define tskIDLE_PRIORITY 0U
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configASSERT(x) assert(x)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define portYIELD_FROM_ISR(x) ((void)(x))
#endif
