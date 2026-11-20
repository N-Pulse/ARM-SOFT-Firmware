#include "status_leds_codec.h"
#include "stm32g4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
USART_TypeDef mock_usart3;
UART_HandleTypeDef huart3={.Instance=USART3};
static uint32_t now;
static unsigned sends,aborts;
static const uint8_t *active;
static bool abort_fails;
uint32_t HAL_GetTick(void) { return now; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 160000000U; }
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *h) {
    assert(h==&huart3 && h->Init.BaudRate==4000000 && h->Init.WordLength==8 &&
        h->Init.StopBits==1 && h->Init.Parity==0 && h->Init.OverSampling==16 &&
        h->Init.ClockPrescaler==1 && h->AdvancedInit.TxPinLevelInvert==1);
    h->gState=HAL_UART_STATE_READY; h->Instance->BRR=40; return HAL_OK;
}
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *d) {
    assert(d->Instance==DMA1_Channel1 && d->Init.Request==DMA_REQUEST_USART3_TX &&
        d->Init.MemInc==1 && d->Init.PeriphInc==0 && d->Init.Mode==DMA_NORMAL);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *h,const uint8_t *p,uint16_t n) {
    assert(h==&huart3 && h->gState==HAL_UART_STATE_READY && n==STATUS_LEDS_WIRE_BYTES);
    active=p; ++sends; h->gState=HAL_UART_STATE_BUSY_TX; h->ErrorCode=0; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *h) {
    assert(h==&huart3); ++aborts;
    if(abort_fails) return HAL_ERROR;
    h->gState=HAL_UART_STATE_READY; return HAL_OK;
}
void HAL_NVIC_SetPriority(int irq,uint32_t p,uint32_t sub) {
    assert((irq==DMA1_Channel1_IRQn || irq==USART3_IRQn) && p==6 && sub==0);
}
void HAL_NVIC_ClearPendingIRQ(int irq) { assert(irq==DMA1_Channel1_IRQn || irq==USART3_IRQn); }
void HAL_NVIC_EnableIRQ(int irq) { assert(irq==DMA1_Channel1_IRQn || irq==USART3_IRQn); }
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *d) { assert(d==huart3.hdmatx); }
void HAL_UART_IRQHandler(UART_HandleTypeDef *h) { assert(h==&huart3); }
static void expect(status_led_color_t arm) {
    status_led_color_t c[3]={arm,STATUS_LED_RED,STATUS_LED_RED};
    uint8_t expected[STATUS_LEDS_WIRE_BYTES]; StatusLeds_Encode(c,expected);
    assert(!memcmp(expected,active,sizeof(expected)));
}
int main(void) {
    now=100; StatusLeds_BoardInit(); StatusLeds_BoardService(); assert(!sends);
    now=101; StatusLeds_BoardService(); assert(!sends);
    now=102; StatusLeds_BoardService(); assert(sends==1); expect(STATUS_LED_RED);
    StatusLeds_BoardWrite(STATUS_LED_CONTROLLER,STATUS_LED_GREEN);
    StatusLeds_BoardService(); assert(sends==1); expect(STATUS_LED_RED);
    now=103; huart3.gState=HAL_UART_STATE_READY; StatusLeds_BoardService();
    now=104; StatusLeds_BoardService(); assert(sends==1);
    now=105; StatusLeds_BoardService(); assert(sends==2); expect(STATUS_LED_GREEN);
    StatusLeds_BoardWrite(STATUS_LED_CONTROLLER,STATUS_LED_RED);
    StatusLeds_BoardWrite(STATUS_LED_CONTROLLER,STATUS_LED_OFF);
    now=106; huart3.gState=HAL_UART_STATE_READY; StatusLeds_BoardService();
    now=108; StatusLeds_BoardService(); assert(sends==3); expect(STATUS_LED_OFF);
    now=208; StatusLeds_BoardService(); assert(aborts==1 && sends==3);
    now=210; StatusLeds_BoardService(); assert(sends==4); expect(STATUS_LED_OFF);
    abort_fails=true; now=310; StatusLeds_BoardService(); assert(aborts==2);
    now=400; StatusLeds_BoardService(); assert(sends==4);
    abort_fails=false; now=UINT32_MAX-1; StatusLeds_BoardInit();
    ++now; StatusLeds_BoardService(); assert(sends==4);
    ++now; StatusLeds_BoardService(); assert(sends==5); expect(STATUS_LED_RED);
    puts("PASS LED DMA: inverted 4Mbaud setup, immutable active frame, latest pending colors, reset gap, stalled-TX abort/quarantine and tick wrap");
    return 0;
}
