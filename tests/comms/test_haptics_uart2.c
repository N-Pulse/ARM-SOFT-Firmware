#include "haptics.h"
#include "haptics_uart2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
UART_HandleTypeDef huart2 = {.Instance = USART2};
static uint32_t now, primask;
static unsigned resets, starts, aborts;
static uint8_t *rx;
static const uint8_t *tx;
static size_t tx_length;
static bool irq_enabled, abort_fails;
uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1; }
void __set_PRIMASK(uint32_t value) { primask = value; }
uint32_t HAL_GetTick(void) { return now; }
void Haptics_Init(void) { ++resets; }
void HAL_NVIC_EnableIRQ(int irq) { assert(irq == USART2_IRQn); irq_enabled = true; }
void HAL_NVIC_DisableIRQ(int irq) { assert(irq == USART2_IRQn); irq_enabled = false; }
void HAL_NVIC_ClearPendingIRQ(int irq) { assert(irq == USART2_IRQn); }
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t sub) {
    assert(irq == USART2_IRQn && priority == 6 && sub == 0);
}
void HAL_UART_IRQHandler(UART_HandleTypeDef *uart) { assert(uart == &huart2); }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t n) {
    assert(uart == &huart2 && n == 1); rx = data; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t n) {
    assert(uart == &huart2 && uart->gState == HAL_UART_STATE_READY);
    tx = data; tx_length = n; ++starts; uart->gState = HAL_UART_STATE_BUSY_TX; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart) {
    assert(uart == &huart2); ++aborts; return abort_fails ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart) {
    assert(uart == &huart2); ++aborts;
    if (abort_fails) return HAL_ERROR;
    uart->gState = HAL_UART_STATE_READY; return HAL_OK;
}
static void receive(uint8_t byte) { *rx = byte; assert(HapticsUART2_RxComplete(&huart2)); }
int main(void) {
    UART_HandleTypeDef other = {0}; uint8_t byte; uint32_t timestamp;
    assert(!HapticsUART2_RxComplete(&other) && !HapticsUART2_Error(&other));
    assert(Haptics_TransportStart() && irq_enabled);
    now = 12; receive(0xA5); now = 30;
    assert(Haptics_TransportReadByte(&byte, &timestamp) && byte == 0xA5 && timestamp == 12);
    assert(!Haptics_TransportReadByte(&byte, &timestamp));
    uint8_t frame[] = {1, 2, 3};
    assert(Haptics_TransportTrySend(frame, sizeof(frame)));
    memset(frame, 0, sizeof(frame));
    assert(tx_length == 3 && tx[0] == 1 && tx[2] == 3);
    assert(!Haptics_TransportTrySend(frame, sizeof(frame)) && starts == 1);
    huart2.gState = HAL_UART_STATE_READY;
    (void)Haptics_TransportReadByte(&byte, &timestamp);
    assert(Haptics_TransportTrySend(frame, sizeof(frame)) && starts == 2);
    now += 100;
    assert(!Haptics_TransportReadByte(&byte, &timestamp) && resets == 1 && aborts == 2);
    receive(7); assert(HapticsUART2_Error(&huart2));
    assert(!Haptics_TransportReadByte(&byte, &timestamp) && resets == 2);
    assert(!Haptics_TransportReadByte(&byte, &timestamp));
    for (unsigned i = 0; i < 1024; ++i) receive((uint8_t)i);
    assert(!Haptics_TransportReadByte(&byte, &timestamp) && resets == 3);
    receive(8); assert(Haptics_TransportReadByte(&byte, &timestamp) && byte == 8);
    abort_fails = true; HapticsUART2_Error(&huart2);
    assert(!Haptics_TransportReadByte(&byte, &timestamp) && !irq_enabled);
    assert(!Haptics_TransportTrySend(frame, sizeof(frame)));
    puts("USART2 haptics transport: timestamp, copy/busy, timeout, error, overflow and abort failure passed");
    return 0;
}
