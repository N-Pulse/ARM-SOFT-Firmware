#include "motor_l298n.h"
#include "board_link.h"
#include "stm32g4xx_hal.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "main.h"
#include <math.h>
#include <stdint.h>

extern UART_HandleTypeDef hcom_uart[COMn];

/* Tiny debug beacon over LPUART1 (capture host-side with --listen). */
static void l298n_beacon(uint8_t id, uint8_t crc)
{
    uint8_t b[] = { 0xAAU, 0x01U, id, 0x00U, crc };
    HAL_UART_Transmit(&hcom_uart[COM1], b, sizeof(b), 50U);
}

/* Clock chain: HSI 16 MHz → PLL → SYSCLK 170 MHz, APB1/APB2 prescaler = 1
 * → TIM1 and TIM4 kernel clock = 170 MHz.
 * 170 MHz / 8500 = 20 kHz PWM (PSC=0, ARR=8499). */
#define PWM_ARR                   8499U

/* Empirical full-speed travel rate. See README for calibration procedure. */
#define L298N_DEG_PER_SEC_AT_FULL 120.0f
#define L298N_RAD_TO_DEG          (180.0f / 3.14159265358979f)
#define L298N_DURATION_CAP_MS     5000U
#define L298N_MIN_MOVE_DEG        1.0f

/* Sens de câblage global du H-bridge vs sens d'angle attendu.
 *   1  = normal
 *  -1  = tous les moteurs inversés (close ouvre / open ferme) → on inverse.
 * Tous les moteurs sont câblés pareil, donc une seule inversion globale. */
#define L298N_DIR_SIGN            (-1)

typedef struct {
    TIM_TypeDef *  tim;
    uint32_t       tim_channel;
    GPIO_TypeDef * pwm_port;
    uint16_t       pwm_pin;
    uint8_t        pwm_af;
    GPIO_TypeDef * in1_port;
    uint16_t       in1_pin;
    GPIO_TypeDef * in2_port;
    uint16_t       in2_pin;
} l298n_motor_t;

/* Pin map for all 8 motors. The board owning a given motor (per
 * BoardLink_IsLocalMotor) is the one that initializes its pins and timer
 * channel; the others ignore it. */
static const l298n_motor_t s_motors[MOTOR_COUNT] = {
    [MOTOR_THUMB]   = { TIM1, TIM_CHANNEL_1, GPIOA, GPIO_PIN_8,  GPIO_AF6_TIM1, GPIOB, GPIO_PIN_5,  GPIOB, GPIO_PIN_4  },
    [MOTOR_INDEX]   = { TIM1, TIM_CHANNEL_2, GPIOA, GPIO_PIN_9,  GPIO_AF6_TIM1, GPIOC, GPIO_PIN_10, GPIOC, GPIO_PIN_11 },
    [MOTOR_MIDDLE]  = { TIM1, TIM_CHANNEL_3, GPIOA, GPIO_PIN_10, GPIO_AF6_TIM1, GPIOC, GPIO_PIN_12, GPIOA, GPIO_PIN_15 },
    [MOTOR_RING]    = { TIM1, TIM_CHANNEL_4, GPIOA, GPIO_PIN_11, GPIO_AF6_TIM1, GPIOB, GPIO_PIN_12, GPIOB, GPIO_PIN_13 },
    [MOTOR_LITTLE]  = { TIM4, TIM_CHANNEL_1, GPIOB, GPIO_PIN_6,  GPIO_AF2_TIM4, GPIOC, GPIO_PIN_8,  GPIOC, GPIO_PIN_9  },
    [MOTOR_WRIST_X] = { TIM4, TIM_CHANNEL_2, GPIOB, GPIO_PIN_7,  GPIO_AF2_TIM4, GPIOC, GPIO_PIN_4,  GPIOC, GPIO_PIN_5  },
    /* WRIST_Y IN1/IN2 moved from PC6/PC7 → PB0/PB1 (PC6/PC7 reserved for TIM8 encoder RING). */
    [MOTOR_WRIST_Y] = { TIM4, TIM_CHANNEL_3, GPIOB, GPIO_PIN_8,  GPIO_AF2_TIM4, GPIOB, GPIO_PIN_0,  GPIOB, GPIO_PIN_1  },
    /* PALM IN1 moved from PC2 → PD2 (PC2 reserved for TIM20 encoder PALM). */
    [MOTOR_PALM]    = { TIM4, TIM_CHANNEL_4, GPIOB, GPIO_PIN_9,  GPIO_AF2_TIM4, GPIOD, GPIO_PIN_2,  GPIOC, GPIO_PIN_3  },
};

/* One TIM_HandleTypeDef per shared timer. TIM1 drives motors 0-3 (4 PWM
 * channels), TIM4 drives motors 4-7. */
static TIM_HandleTypeDef s_htim1;
static TIM_HandleTypeDef s_htim4;

/* Per-motor auto-stop timer (one-shot, period rewritten per move) +
 * cached commanded angle for open-loop direction/duration calculation. */
static TimerHandle_t s_stop_timer[MOTOR_COUNT];
static float        s_current_rad[MOTOR_COUNT];

/* Sens de drive courant par moteur : +1 / -1 / 0 (arrêté).
 * Lu par la sécurité anti-butée (MotorGuard, main.c) pour détecter un calage. */
static volatile int8_t s_drv[MOTOR_COUNT];

int Motor_L298N_DrivingDir(motor_id_t id)
{
    if (id >= MOTOR_COUNT) return 0;
    return (int)s_drv[id];
}

/* ------------------------------------------------------------------------ */

static TIM_HandleTypeDef *htim_for(TIM_TypeDef *tim)
{
    if (tim == TIM1) return &s_htim1;
    if (tim == TIM4) return &s_htim4;
    return NULL;
}

static void enable_gpio_clock(GPIO_TypeDef *port)
{
    if      (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
}

static void init_timer_base(TIM_HandleTypeDef *htim, TIM_TypeDef *instance)
{
    htim->Instance               = instance;
    htim->Init.Prescaler         = 0;
    htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim->Init.Period            = PWM_ARR;
    htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(htim);
}

static void init_pwm_channel(TIM_HandleTypeDef *htim, uint32_t channel)
{
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 0;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    /* TIM1 is advanced; HAL still wants OCN fields. */
    oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(htim, &oc, channel);
    HAL_TIM_PWM_Start(htim, channel);
}

static void set_duty(motor_id_t id, uint32_t pulse)
{
    if (pulse > PWM_ARR) pulse = PWM_ARR;
    __HAL_TIM_SET_COMPARE(htim_for(s_motors[id].tim),
                          s_motors[id].tim_channel, pulse);
}

static void stop_callback(TimerHandle_t t)
{
    motor_id_t id = (motor_id_t)(uintptr_t)pvTimerGetTimerID(t);
    l298n_beacon(0xF1U, (uint8_t)id);   /* F1 = motor `id` auto-stopped */
    Motor_L298N_Stop(id);
}

static void arm_stop_timer(motor_id_t id, uint32_t duration_ms)
{
    if (s_stop_timer[id] == NULL) return;
    (void)xTimerChangePeriod(s_stop_timer[id],
                             pdMS_TO_TICKS(duration_ms),
                             pdMS_TO_TICKS(10));
}

/* ------------------------------------------------------------------------ */

void Motor_L298N_Init(void)
{
    bool need_tim1 = false;
    bool need_tim4 = false;

    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        s_current_rad[i]   = 0.0f;
        s_stop_timer[i]    = NULL;
        if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
        if (s_motors[i].tim == TIM1) need_tim1 = true;
        if (s_motors[i].tim == TIM4) need_tim4 = true;
    }

    if (need_tim1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
        init_timer_base(&s_htim1, TIM1);
    }
    if (need_tim4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
        init_timer_base(&s_htim4, TIM4);
    }

    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
        const l298n_motor_t *m = &s_motors[i];

        enable_gpio_clock(m->pwm_port);
        enable_gpio_clock(m->in1_port);
        enable_gpio_clock(m->in2_port);

        /* PWM pin in alternate-function push-pull. */
        GPIO_InitTypeDef g = {0};
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = m->pwm_af;
        g.Pin       = m->pwm_pin;
        HAL_GPIO_Init(m->pwm_port, &g);

        /* IN1 / IN2 — push-pull GPIO, default LOW. */
        g.Mode      = GPIO_MODE_OUTPUT_PP;
        g.Speed     = GPIO_SPEED_FREQ_LOW;
        g.Alternate = 0;
        g.Pin       = m->in1_pin;
        HAL_GPIO_Init(m->in1_port, &g);
        g.Pin       = m->in2_pin;
        HAL_GPIO_Init(m->in2_port, &g);
        HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);

        init_pwm_channel(htim_for(m->tim), m->tim_channel);

        s_stop_timer[i] = xTimerCreate("L298NStop",
                                       1,                /* placeholder */
                                       pdFALSE,
                                       (void *)(uintptr_t)i,
                                       stop_callback);
        configASSERT(s_stop_timer[i] != NULL);
    }

    /* TIM1 advanced timer needs MOE asserted to actually drive the pins.
     * HAL_TIM_PWM_Start does this internally but be paranoid. */
    if (need_tim1) {
        __HAL_TIM_MOE_ENABLE(&s_htim1);
    }
}

/* ------------------------------------------------------------------------ */

static void drive_forward_for(motor_id_t id, uint8_t speed_pct, uint32_t ms)
{
    const l298n_motor_t *m = &s_motors[id];
    HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);
    set_duty(id, ((uint32_t)speed_pct * (PWM_ARR + 1U)) / 100U);
    s_drv[id] = +1;
    arm_stop_timer(id, ms);
}

static void drive_backward_for(motor_id_t id, uint8_t speed_pct, uint32_t ms)
{
    const l298n_motor_t *m = &s_motors[id];
    HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_SET);
    set_duty(id, ((uint32_t)speed_pct * (PWM_ARR + 1U)) / 100U);
    s_drv[id] = -1;
    arm_stop_timer(id, ms);
}

void Motor_L298N_MoveToAngle(motor_id_t id, float target_rad, uint8_t speed_pct)
{
    if (id >= MOTOR_COUNT) return;
    if (s_stop_timer[id] == NULL) return;   /* not local on this board */
    if (speed_pct == 0U) { Motor_L298N_Stop(id); return; }
    if (speed_pct > 100U) speed_pct = 100U;

    float delta_rad = target_rad - s_current_rad[id];
    float delta_deg = fabsf(delta_rad) * L298N_RAD_TO_DEG;

    if (delta_deg < L298N_MIN_MOVE_DEG) {
        Motor_L298N_Stop(id);
        return;
    }

    float deg_per_s   = L298N_DEG_PER_SEC_AT_FULL * ((float)speed_pct / 100.0f);
    uint32_t duration = (uint32_t)((delta_deg / deg_per_s) * 1000.0f);
    if (duration == 0U) duration = 1U;
    if (duration > L298N_DURATION_CAP_MS) duration = L298N_DURATION_CAP_MS;

    float dir = delta_rad * (float)L298N_DIR_SIGN;   /* inversion globale câblage */
    if (dir > 0.0f) {
        drive_forward_for(id, speed_pct, duration);
        l298n_beacon(0xF4U, (uint8_t)id);
    } else {
        drive_backward_for(id, speed_pct, duration);
        l298n_beacon(0xF5U, (uint8_t)id);
    }

    /* Optimistic update — no encoder. Drift recoverable via reboot. */
    s_current_rad[id] = target_rad;
}

void Motor_L298N_Stop(motor_id_t id)
{
    if (id >= MOTOR_COUNT) return;
    if (s_stop_timer[id] == NULL) return;

    (void)xTimerStop(s_stop_timer[id], 0);
    set_duty(id, 0);
    const l298n_motor_t *m = &s_motors[id];
    HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);
    s_drv[id] = 0;
}

void Motor_L298N_StopAll(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++) {
        Motor_L298N_Stop((motor_id_t)i);
    }
}

void Motor_L298N_SetRaw(motor_id_t id, int dir, uint8_t speed_pct)
{
    if (id >= MOTOR_COUNT) return;
    if (s_stop_timer[id] == NULL) return;          /* pas local sur cette carte */

    (void)xTimerStop(s_stop_timer[id], 0);         /* pas d'auto-stop en closed-loop */
    const l298n_motor_t *m = &s_motors[id];

    if (dir > 0) {
        HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);
        s_drv[id] = +1;
    } else if (dir < 0) {
        HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_SET);
        s_drv[id] = -1;
    } else {
        HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);
        set_duty(id, 0);
        s_drv[id] = 0;
        return;
    }
    if (speed_pct > 100U) speed_pct = 100U;
    set_duty(id, ((uint32_t)speed_pct * (PWM_ARR + 1U)) / 100U);
}
