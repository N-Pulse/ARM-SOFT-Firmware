#include "finger_test.h"
#include "motor_control.h"
#include "stm32g4xx_hal.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "main.h"
#include <math.h>

extern UART_HandleTypeDef hcom_uart[COMn];

/* Tiny debug beacon over LPUART (host-side captures it via send_action --listen).
 * Same wire format as the rest of the firmware: [0xAA][0x01][id][0x00][crc]. */
static void finger_beacon(uint8_t id, uint8_t crc)
{
    uint8_t b[] = { 0xAAU, 0x01U, id, 0x00U, crc };
    HAL_UART_Transmit(&hcom_uart[COM1], b, sizeof(b), 50U);
}

/* Which physical finger is wired to the L298N. Change this to retarget the
 * test bench to another finger; the duration auto-adjusts because each finger
 * has its own pose angles in motor_map. */
#define FINGER_TEST_MOTOR_ID         MOTOR_THUMB

/* Empirical motor speed at 100 % PWM, with the gearbox we ship. Used to map
 * the requested angular delta (radians from motor_map) to a run duration.
 * MEASURE THIS:
 *   1. Set speed_percent to 100, send a CLOSE_HAND
 *   2. Time how long it takes to traverse a known angle (e.g. from open
 *      stop to closed stop, ~85°)
 *   3. deg_per_s = 85 / time_seconds
 * Then update this constant. Linear scaling is assumed for lower duties. */
#define FINGER_DEG_PER_SEC_AT_FULL   120.0f

#define FINGER_RAD_TO_DEG            (180.0f / 3.14159265358979f)
#define FINGER_DURATION_CAP_MS       5000U   /* hard ceiling, safety */
#define FINGER_MIN_MOVE_DEG          1.0f    /* below this, treat as no-op */

/* Clock chain: HSI 16 MHz → PLL (M=4, N=85, R=2) → SYSCLK 170 MHz.
 * APB2 prescaler = 1, so TIM1 kernel clock = 170 MHz.
 * 170 MHz / (PSC+1)/(ARR+1) = 170M / 8500 = 20 kHz. */
#define FINGER_PWM_ARR        8499U

#define FINGER_PWM_GPIO_PORT  GPIOA
#define FINGER_PWM_GPIO_PIN   GPIO_PIN_8
#define FINGER_PWM_GPIO_AF    GPIO_AF6_TIM1   /* RM0440 Tab 13: PA8 / TIM1_CH1 */

#define FINGER_IN1_GPIO_PORT  GPIOB
#define FINGER_IN1_GPIO_PIN   GPIO_PIN_5

#define FINGER_IN2_GPIO_PORT  GPIOB
#define FINGER_IN2_GPIO_PIN   GPIO_PIN_4

static TIM_HandleTypeDef s_htim_finger;
static TimerHandle_t     s_stop_timer = NULL;
/* Cached commanded position. We assume the finger boots at the OPEN angle
 * (which is what the user is expected to physically place it at before
 * power-on). Updated after every successful MoveToPose call. */
static float             s_current_rad = 0.0f;

static void set_duty(uint32_t pulse)
{
    if (pulse > FINGER_PWM_ARR) pulse = FINGER_PWM_ARR;
    __HAL_TIM_SET_COMPARE(&s_htim_finger, TIM_CHANNEL_1, pulse);
}

/* FreeRTOS timer-service callback: fires FINGER_TRAVEL_MS after the last
 * Forward/Backward, cuts the motor. Runs in the timer-service task — safe
 * to call HAL_GPIO_Write + __HAL_TIM_SET_COMPARE here. */
static void finger_stop_cb(TimerHandle_t t)
{
    (void)t;
    finger_beacon(0xF1U, 0x55U);   /* F1 = auto-stop fired */
    Finger_Test_Stop();
}

static void arm_stop_timer(uint32_t duration_ms)
{
    if (s_stop_timer == NULL) return;
    /* xTimerChangePeriod sets the new period AND restarts the timer (whether
     * it was previously dormant or active). 10-tick block on the timer-cmd
     * queue is plenty under our load. */
    (void)xTimerChangePeriod(s_stop_timer,
                             pdMS_TO_TICKS(duration_ms),
                             pdMS_TO_TICKS(10));
}

void Finger_Test_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* IN1 / IN2 — push-pull GPIO outputs, default LOW (motor coasts). */
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = FINGER_IN1_GPIO_PIN;
    HAL_GPIO_Init(FINGER_IN1_GPIO_PORT, &g);
    g.Pin   = FINGER_IN2_GPIO_PIN;
    HAL_GPIO_Init(FINGER_IN2_GPIO_PORT, &g);
    HAL_GPIO_WritePin(FINGER_IN1_GPIO_PORT, FINGER_IN1_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FINGER_IN2_GPIO_PORT, FINGER_IN2_GPIO_PIN, GPIO_PIN_RESET);

    /* PA8 → TIM1_CH1 alternate function. */
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = FINGER_PWM_GPIO_AF;
    g.Pin       = FINGER_PWM_GPIO_PIN;
    HAL_GPIO_Init(FINGER_PWM_GPIO_PORT, &g);

    /* TIM1 base — 20 kHz PWM. */
    s_htim_finger.Instance               = TIM1;
    s_htim_finger.Init.Prescaler         = 0;
    s_htim_finger.Init.CounterMode       = TIM_COUNTERMODE_UP;
    s_htim_finger.Init.Period            = FINGER_PWM_ARR;
    s_htim_finger.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    s_htim_finger.Init.RepetitionCounter = 0;
    s_htim_finger.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&s_htim_finger);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 0;                    /* 0% on boot — motor stopped */
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    /* TIM1 is an advanced-control timer: even though we only drive CH1 (not
     * CH1N), HAL still wants OCN fields filled in. */
    oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&s_htim_finger, &oc, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&s_htim_finger, TIM_CHANNEL_1);
    /* PWM_Start enables MOE on advanced timers internally; be explicit. */
    __HAL_TIM_MOE_ENABLE(&s_htim_finger);

    /* Auto-stop timer. One-shot, period rewritten on every move via
     * arm_stop_timer(). The 1-tick initial period is just a placeholder. */
    s_stop_timer = xTimerCreate("FingerStop",
                                1,
                                pdFALSE,
                                NULL,
                                finger_stop_cb);
    configASSERT(s_stop_timer != NULL);
}

/* Internal: forward at duty for the given duration, then auto-stop. */
static void drive_forward_for(uint8_t speed_percent, uint32_t duration_ms)
{
    HAL_GPIO_WritePin(FINGER_IN1_GPIO_PORT, FINGER_IN1_GPIO_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FINGER_IN2_GPIO_PORT, FINGER_IN2_GPIO_PIN, GPIO_PIN_RESET);
    set_duty(((uint32_t)speed_percent * (FINGER_PWM_ARR + 1U)) / 100U);
    arm_stop_timer(duration_ms);
}

static void drive_backward_for(uint8_t speed_percent, uint32_t duration_ms)
{
    HAL_GPIO_WritePin(FINGER_IN1_GPIO_PORT, FINGER_IN1_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FINGER_IN2_GPIO_PORT, FINGER_IN2_GPIO_PIN, GPIO_PIN_SET);
    set_duty(((uint32_t)speed_percent * (FINGER_PWM_ARR + 1U)) / 100U);
    arm_stop_timer(duration_ms);
}

/* Public free-running APIs — kept for manual jogging / debugging. They run
 * for FINGER_DURATION_CAP_MS so a forgotten command can't burn the motor. */
void Finger_Test_Forward(uint8_t speed_percent)
{
    if (speed_percent > 100U) speed_percent = 100U;
    drive_forward_for(speed_percent, FINGER_DURATION_CAP_MS);
    finger_beacon(0xF2U, 0xA1U);
}

void Finger_Test_Backward(uint8_t speed_percent)
{
    if (speed_percent > 100U) speed_percent = 100U;
    drive_backward_for(speed_percent, FINGER_DURATION_CAP_MS);
    finger_beacon(0xF3U, 0xB2U);
}

void Finger_Test_MoveToPose(motor_pose_id_t pose, uint8_t speed_percent)
{
    if (speed_percent == 0U) { Finger_Test_Stop(); return; }
    if (speed_percent > 100U) speed_percent = 100U;

    float target_rad = MotorMap_GetPose(pose, FINGER_TEST_MOTOR_ID);
    float delta_rad  = target_rad - s_current_rad;
    float delta_deg  = fabsf(delta_rad) * FINGER_RAD_TO_DEG;

    if (delta_deg < FINGER_MIN_MOVE_DEG) {
        /* Already at target — make sure motor isn't running. */
        Finger_Test_Stop();
        return;
    }

    /* Linear scaling of empirical full-speed deg/s by current duty. */
    float deg_per_s = FINGER_DEG_PER_SEC_AT_FULL * ((float)speed_percent / 100.0f);
    uint32_t duration_ms = (uint32_t)((delta_deg / deg_per_s) * 1000.0f);
    if (duration_ms == 0U) duration_ms = 1U;
    if (duration_ms > FINGER_DURATION_CAP_MS) duration_ms = FINGER_DURATION_CAP_MS;

    if (delta_rad > 0.0f) {
        drive_forward_for(speed_percent, duration_ms);
        finger_beacon(0xF4U, 0xC3U);   /* F4 = MoveToPose forward */
    } else {
        drive_backward_for(speed_percent, duration_ms);
        finger_beacon(0xF5U, 0xD4U);   /* F5 = MoveToPose backward */
    }

    /* Optimistic update: assume we'll reach the target. Without an encoder
     * we can't verify; the user can recalibrate FINGER_DEG_PER_SEC_AT_FULL
     * if the cached position drifts from reality. */
    s_current_rad = target_rad;
}

void Finger_Test_Stop(void)
{
    /* Cancel any pending auto-stop so an explicit Stop isn't followed by
     * a stale timer fire. Safe to call from any FreeRTOS task context. */
    if (s_stop_timer != NULL) {
        (void)xTimerStop(s_stop_timer, 0);
    }
    set_duty(0);
    HAL_GPIO_WritePin(FINGER_IN1_GPIO_PORT, FINGER_IN1_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FINGER_IN2_GPIO_PORT, FINGER_IN2_GPIO_PIN, GPIO_PIN_RESET);
}
