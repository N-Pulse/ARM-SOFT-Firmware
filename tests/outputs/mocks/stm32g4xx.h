#ifndef OUTPUTS_DEVICE_MOCK_H
#define OUTPUTS_DEVICE_MOCK_H
#include "../../safety/mocks/stm32g4xx_hal.h"
typedef struct { uint32_t AHB2ENR, APB2ENR; } RCC_TypeDef;
extern RCC_TypeDef mock_rcc;
extern HRTIM_TypeDef *fatal_hrtim;
extern TIM_TypeDef *fatal_tim15, *fatal_tim16, *fatal_tim17;
#define RCC (&mock_rcc)
#define HRTIM1 fatal_hrtim
#define TIM15 fatal_tim15
#define TIM16 fatal_tim16
#define TIM17 fatal_tim17
#define RCC_AHB2ENR_GPIOCEN (1U << 2)
#define RCC_AHB2ENR_GPIOGEN (1U << 6)
#define RCC_APB2ENR_HRTIM1EN (1U << 26)
#define RCC_APB2ENR_TIM15EN (1U << 16)
#define RCC_APB2ENR_TIM16EN (1U << 17)
#define RCC_APB2ENR_TIM17EN (1U << 18)
#define HRTIM_MCR_TBCEN (1U << 18)
#define HRTIM_MCR_TCCEN (1U << 19)
#define HRTIM_MCR_TDCEN (1U << 20)
#define HRTIM_MCR_TECEN (1U << 21)
#define HRTIM_MCR_TFCEN (1U << 22)
void __NOP(void);
#endif
