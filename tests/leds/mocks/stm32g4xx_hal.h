#ifndef LED_MOCK_HAL_H
#define LED_MOCK_HAL_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { HAL_OK,HAL_ERROR } HAL_StatusTypeDef;
typedef struct { uint32_t BRR; } USART_TypeDef;
extern USART_TypeDef mock_usart3;
#define USART3 (&mock_usart3)
#define DMA1_Channel1 ((void *)1)
#define DMA1_Channel1_IRQn 11
#define USART3_IRQn 39
#define UART_WORDLENGTH_8B 8
#define UART_STOPBITS_1 1
#define UART_PARITY_NONE 0
#define UART_MODE_TX 1
#define UART_HWCONTROL_NONE 0
#define UART_OVERSAMPLING_16 16
#define UART_PRESCALER_DIV1 1
#define UART_ADVFEATURE_TXINVERT_INIT 1
#define UART_ADVFEATURE_TXINV_ENABLE 1
#define DMA_REQUEST_USART3_TX 26
#define DMA_MEMORY_TO_PERIPH 1
#define DMA_PINC_DISABLE 0
#define DMA_MINC_ENABLE 1
#define DMA_PDATAALIGN_BYTE 1
#define DMA_MDATAALIGN_BYTE 1
#define DMA_NORMAL 0
#define DMA_PRIORITY_VERY_HIGH 3
#define HAL_UART_STATE_READY 0
#define HAL_UART_STATE_BUSY_TX 1
#define HAL_UART_ERROR_NONE 0
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DMAMUX1_CLK_ENABLE() ((void)0)
#define __HAL_LINKDMA(h,f,d) do { (h)->f=&(d); (d).Parent=(h); } while(0)
typedef struct {
    void *Instance,*Parent;
    struct { uint32_t Request,Direction,PeriphInc,MemInc,PeriphDataAlignment,
        MemDataAlignment,Mode,Priority; } Init;
} DMA_HandleTypeDef;
typedef struct {
    USART_TypeDef *Instance;
    struct { uint32_t BaudRate,WordLength,StopBits,Parity,Mode,HwFlowCtl,OverSampling,ClockPrescaler; } Init;
    struct { uint32_t AdvFeatureInit,TxPinLevelInvert; } AdvancedInit;
    uint32_t gState,ErrorCode;
    DMA_HandleTypeDef *hdmatx;
} UART_HandleTypeDef;
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *,const uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_NVIC_SetPriority(int,uint32_t,uint32_t);
void HAL_NVIC_ClearPendingIRQ(int);
void HAL_NVIC_EnableIRQ(int);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *);
void HAL_UART_IRQHandler(UART_HandleTypeDef *);
#endif
