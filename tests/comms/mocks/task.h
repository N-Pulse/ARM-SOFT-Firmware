#ifndef COMMS_MOCK_TASK_H
#define COMMS_MOCK_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
BaseType_t xTaskCreate(void (*fn)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t, TickType_t);
void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t *);
void xTaskNotifyGive(TaskHandle_t);
void vTaskDelay(TickType_t);
#endif

TickType_t xTaskGetTickCount(void);
