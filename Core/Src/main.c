/* Standalone STM32G474RET6 single-motor test bench.
 * Maintained outside CubeMX generation; see docs/BENCH_OVERVIEW.md. */
#include "main.h"
#include "bench.h"
#include "bench_hw.h"
#include "bench_protocol.h"
int main(void)
{
    BenchHW_Init();
    uint32_t last_control = HAL_GetTick(), last_led = last_control;
    Bench_Init(BenchHW_Counter(), last_control, BenchHW_EncoderRunning());
    BenchProtocol_Init(last_control);
    for (;;) {
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - last_control) >= 10U) {
            last_control = now;
            Bench_SetEncoderAvailable(BenchHW_EncoderRunning());
            Bench_Update(BenchHW_Counter(), now);
        }
        /* Bounded RX work cannot starve the100Hz controller. */
        uint8_t byte;
        for (unsigned i = 0; i < 64U && BenchHW_ReadByte(&byte); ++i)
            BenchProtocol_Byte(byte, HAL_GetTick());
        BenchProtocol_Service(HAL_GetTick());
        if ((uint32_t)(now - last_led) >= 500U) {
            last_led = now;
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
