#ifndef OLD_DUAL_FREERTOS_H
#define OLD_DUAL_FREERTOS_H
#include <assert.h>
#include <stdint.h>
typedef uint32_t StackType_t;
typedef uint32_t TickType_t;
typedef struct {uint32_t unused;} StaticTask_t;
typedef void *TaskHandle_t;
#define pdMS_TO_TICKS(ms) (ms)
#define tskIDLE_PRIORITY 0U
#define configASSERT(x) assert(x)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#endif
