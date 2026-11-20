#include "haptics.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

__attribute__((weak)) bool Haptics_TransportStart(void) { return false; }
__attribute__((weak)) bool Haptics_TransportReadByte(uint8_t *byte,uint32_t *timestamp_ms) {
    (void)byte; (void)timestamp_ms; return false;
}
__attribute__((weak)) bool Haptics_TransportTrySend(const uint8_t *frame,size_t length) {
    (void)frame; (void)length; return false;
}
static bool send_frame(const uint8_t *frame,size_t length,void *context) {
    (void)context; return Haptics_TransportTrySend(frame,length);
}
static void haptics_task(void *context) {
    (void)context;
    for(;;) {
        /* TelemetryTask is the sole task owner of force acquisition. */
        uint8_t byte; uint32_t timestamp;
        for(unsigned i=0;i<HAPTICS_FRAME_MAX;++i) {
            if(!Haptics_TransportReadByte(&byte,&timestamp)) break;
            Haptics_FeedByte(byte,timestamp);
        }
        Haptics_Service(HAL_GetTick(),send_frame,NULL);
        /* All protocol and scheduling time values use integer milliseconds. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void Haptics_TaskCreate(void) {
#ifndef MOTOR_BACKEND_SIM
    /* The physical board adapter owns USART2; SIM retains disconnected hooks. */
    if(!Haptics_TransportStart()) return;
    BaseType_t result=xTaskCreate(haptics_task,"haptics",768,NULL,tskIDLE_PRIORITY+1U,NULL);
    configASSERT(result==pdPASS);
#else
    (void)haptics_task;
#endif
}
