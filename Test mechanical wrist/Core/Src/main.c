#include "main.h"
#include "bench_hw.h"
#include "wrist.h"
#include "wrist_protocol.h"
#include "wrist_config.h"

int main(void)
{
    BenchHW_Init();
    uint32_t last = HAL_GetTick(), led = last;
    Wrist_Init(last);
    Protocol_Init(last);
    for (;;) {
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - last) >= WRIST_CONTROL_MS) {
            last = now;
            Wrist_Update(now);
        }
        uint8_t byte;
        for (unsigned i = 0; i < 64U && BenchHW_ReadByte(&byte); ++i)
            Protocol_Byte(byte, HAL_GetTick());
        Protocol_Service(HAL_GetTick());
        if ((uint32_t)(now - led) >= 500U) {
            led = now;
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }
        __WFI();
    }
}
void Error_Handler(void) { BenchHW_Fatal(); }
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{ (void)file; (void)line; BenchHW_Fatal(); }
#endif
