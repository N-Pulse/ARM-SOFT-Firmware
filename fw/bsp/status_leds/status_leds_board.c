#include "status_leds_codec.h"
#include "stm32g4xx_hal.h"
#include <stdbool.h>
#ifndef MOTOR_BACKEND_SIM
extern UART_HandleTypeDef huart3;
static DMA_HandleTypeDef led_dma;
static uint8_t wire[STATUS_LEDS_WIRE_BYTES];
static status_led_color_t desired[STATUS_LED_COUNT];
static bool ready,dirty,in_flight;
static uint32_t started_ms,idle_since_ms;

void StatusLeds_BoardInit(void)
{
    ready=false; dirty=true; in_flight=false;
    for(unsigned i=0;i<STATUS_LED_COUNT;++i) desired[i]=STATUS_LED_RED;
    /* PB10 AF7 was initialized by Cube. This dedicated transmitter replaces
     * the old 115200 UART configuration; USART3 has no other consumer. */
    if(huart3.Instance!=USART3) return;
    huart3.Init.BaudRate=4000000U;
    huart3.Init.WordLength=UART_WORDLENGTH_8B;
    huart3.Init.StopBits=UART_STOPBITS_1;
    huart3.Init.Parity=UART_PARITY_NONE;
    huart3.Init.Mode=UART_MODE_TX;
    huart3.Init.HwFlowCtl=UART_HWCONTROL_NONE;
    huart3.Init.OverSampling=UART_OVERSAMPLING_16;
    huart3.Init.ClockPrescaler=UART_PRESCALER_DIV1;
    huart3.AdvancedInit.AdvFeatureInit=UART_ADVFEATURE_TXINVERT_INIT;
    huart3.AdvancedInit.TxPinLevelInvert=UART_ADVFEATURE_TXINV_ENABLE;
    if(HAL_UART_Init(&huart3)!=HAL_OK) return;
    /* Require an exact 4 MHz symbol clock; do not silently produce a different
     * waveform if the clock tree or oversampling mode is changed. */
    if(HAL_RCC_GetPCLK1Freq()!=huart3.Init.BaudRate*huart3.Instance->BRR) return;
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    led_dma.Instance=DMA1_Channel1; /* Free in the supplied control-board .ioc. */
    led_dma.Init.Request=DMA_REQUEST_USART3_TX;
    led_dma.Init.Direction=DMA_MEMORY_TO_PERIPH;
    led_dma.Init.PeriphInc=DMA_PINC_DISABLE;
    led_dma.Init.MemInc=DMA_MINC_ENABLE;
    led_dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
    led_dma.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
    led_dma.Init.Mode=DMA_NORMAL;
    led_dma.Init.Priority=DMA_PRIORITY_VERY_HIGH;
    if(HAL_DMA_Init(&led_dma)!=HAL_OK) return;
    __HAL_LINKDMA(&huart3,hdmatx,led_dma);
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn,6U,0U);
    HAL_NVIC_SetPriority(USART3_IRQn,6U,0U);
    HAL_NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_ClearPendingIRQ(USART3_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    started_ms=idle_since_ms=HAL_GetTick(); ready=true;
}
void StatusLeds_BoardWrite(status_led_id_t led,status_led_color_t color)
{
    if((unsigned)led>=STATUS_LED_COUNT || (unsigned)color>STATUS_LED_OFF) return;
    if(desired[led]!=color) { desired[led]=color; dirty=true; }
}
void StatusLeds_BoardService(void)
{
    if(!ready) return;
    uint32_t now=HAL_GetTick();
    if(in_flight) {
        if(huart3.gState==HAL_UART_STATE_READY) {
            in_flight=false; idle_since_ms=now;
            if(huart3.ErrorCode!=HAL_UART_ERROR_NONE) dirty=true;
        }
        else if((uint32_t)(now-started_ms)>=100U) {
            if(HAL_UART_AbortTransmit(&huart3)!=HAL_OK) { ready=false; return; }
            in_flight=false; dirty=true; idle_since_ms=now;
        }
    }
    /* 2 tick boundaries imply >=1 ms wall time after observing completion,
     * so idle-low reset exceeds the required 250 us, including
     * the initial power-up reset. DMA prevents per-byte interrupt gaps. */
    if(in_flight || !dirty || (uint32_t)(now-idle_since_ms)<2U) return;
    StatusLeds_Encode(desired,wire);
    if(HAL_UART_Transmit_DMA(&huart3,wire,sizeof(wire))==HAL_OK) {
        dirty=false; in_flight=true; started_ms=now;
    }
}
void DMA1_Channel1_IRQHandler(void) { HAL_DMA_IRQHandler(&led_dma); }
void USART3_IRQHandler(void) { HAL_UART_IRQHandler(&huart3); }
#endif
