#include "FreeRTOS.h"
void vTaskDelay(TickType_t);
void vTaskDelete(TaskHandle_t);
TaskHandle_t xTaskCreateStatic(void (*fn)(void *),const char *,uint32_t,void *,uint32_t,StackType_t *,StaticTask_t *);
