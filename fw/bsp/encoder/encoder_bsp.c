#include "encoder_bsp.h"
#include "board_link.h"
#include "stm32g4xx_hal.h"

/* ─── Constantes datasheet Moon DCU10025P12 + PG10C ──────────────────────────*/

#define ENC_PPR_PER_CHANNEL    12
#define ENC_QUAD_MULT          4
#define ENC_CPR_MOTOR_SHAFT    (ENC_PPR_PER_CHANNEL * ENC_QUAD_MULT)   /* 48   */
#define ENC_GEAR_RATIO         61
#define ENC_CPR_OUTPUT_SHAFT   (ENC_CPR_MOTOR_SHAFT * ENC_GEAR_RATIO)  /* 2928 */
#define TWO_PI                 6.28318530f
#define RAD_PER_COUNT          (TWO_PI / (float)ENC_CPR_OUTPUT_SHAFT)
#define VELOCITY_DT_S          0.01f  /* ControlTask 100 Hz */

/* ─── Table de configuration encodeur — un slot par motor_id ─────────────────
 *
 * Voir encoder_bsp.h pour le mapping détaillé. À runtime, seule la ligne du
 * moteur local (cf. BoardLink_IsLocalMotor) est effectivement initialisée. */

typedef struct {
    TIM_TypeDef    *instance;
    bool            is_32bit;
    GPIO_TypeDef   *ch1_port;  uint16_t ch1_pin;  uint8_t ch1_af;
    GPIO_TypeDef   *ch2_port;  uint16_t ch2_pin;  uint8_t ch2_af;
} enc_cfg_t;

static const enc_cfg_t s_enc_cfg[MOTOR_COUNT] = {
    [MOTOR_THUMB]   = { TIM2,  true,  GPIOA, GPIO_PIN_0,  GPIO_AF1_TIM2,
                                       GPIOA, GPIO_PIN_1,  GPIO_AF1_TIM2 },
    [MOTOR_INDEX]   = { TIM2,  true,  GPIOA, GPIO_PIN_0,  GPIO_AF1_TIM2,
                                       GPIOA, GPIO_PIN_1,  GPIO_AF1_TIM2 },
    [MOTOR_MIDDLE]  = { TIM3,  false, GPIOA, GPIO_PIN_6,  GPIO_AF2_TIM3,
                                       GPIOA, GPIO_PIN_7,  GPIO_AF2_TIM3 },
    [MOTOR_RING]    = { TIM8,  false, GPIOC, GPIO_PIN_6,  GPIO_AF4_TIM8,
                                       GPIOC, GPIO_PIN_7,  GPIO_AF4_TIM8 },
    [MOTOR_LITTLE]  = { TIM3,  false, GPIOA, GPIO_PIN_6,  GPIO_AF2_TIM3,
                                       GPIOA, GPIO_PIN_7,  GPIO_AF2_TIM3 },
    [MOTOR_WRIST_X] = { TIM8,  false, GPIOC, GPIO_PIN_6,  GPIO_AF4_TIM8,
                                       GPIOC, GPIO_PIN_7,  GPIO_AF4_TIM8 },
    [MOTOR_WRIST_Y] = { TIM15, false, GPIOB, GPIO_PIN_14, GPIO_AF1_TIM15,
                                       GPIOB, GPIO_PIN_15, GPIO_AF1_TIM15 },
    [MOTOR_PALM]    = { TIM20, false, GPIOB, GPIO_PIN_2,  GPIO_AF3_TIM20,
                                       GPIOC, GPIO_PIN_2,  GPIO_AF6_TIM20 },
};

/* Un handle HAL par timer physique (5 timers utilisés sur les 2 boards). */
static TIM_HandleTypeDef s_htim2;
static TIM_HandleTypeDef s_htim3;
static TIM_HandleTypeDef s_htim8;
static TIM_HandleTypeDef s_htim15;
static TIM_HandleTypeDef s_htim20;

static TIM_HandleTypeDef *htim_for(TIM_TypeDef *t)
{
    if (t == TIM2)  return &s_htim2;
    if (t == TIM3)  return &s_htim3;
    if (t == TIM8)  return &s_htim8;
    if (t == TIM15) return &s_htim15;
    if (t == TIM20) return &s_htim20;
    return NULL;
}

/* État interne — cumulé pour les locaux (compteur HW) et cache pour les distants. */
static int32_t  s_prev_count_local[MOTOR_COUNT];
static float    s_position_rad[MOTOR_COUNT];
static float    s_velocity_rad_s[MOTOR_COUNT];

/* ─── Helpers GPIO / clock ───────────────────────────────────────────────────*/

static void enable_gpio_clock(GPIO_TypeDef *port)
{
    if      (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
}

static void enable_tim_clock(TIM_TypeDef *t)
{
    if      (t == TIM2)  __HAL_RCC_TIM2_CLK_ENABLE();
    else if (t == TIM3)  __HAL_RCC_TIM3_CLK_ENABLE();
    else if (t == TIM8)  __HAL_RCC_TIM8_CLK_ENABLE();
    else if (t == TIM15) __HAL_RCC_TIM15_CLK_ENABLE();
    else if (t == TIM20) __HAL_RCC_TIM20_CLK_ENABLE();
}

/* ─── Init d'un encodeur via HAL ─────────────────────────────────────────────*/

static void init_encoder(const enc_cfg_t *c, TIM_HandleTypeDef *htim)
{
    enable_tim_clock(c->instance);
    enable_gpio_clock(c->ch1_port);
    enable_gpio_clock(c->ch2_port);

    /* Configure les deux pins en alternate function input. */
    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_AF_PP;
    g.Pull  = GPIO_PULLUP;       /* la plupart des encodeurs open-drain ou push-pull */
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin       = c->ch1_pin;
    g.Alternate = c->ch1_af;
    HAL_GPIO_Init(c->ch1_port, &g);

    g.Pin       = c->ch2_pin;
    g.Alternate = c->ch2_af;
    HAL_GPIO_Init(c->ch2_port, &g);

    /* Configure le timer en mode encodeur TI12 (décodage ×4). */
    htim->Instance               = c->instance;
    htim->Init.Prescaler         = 0;
    htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim->Init.Period            = c->is_32bit ? 0xFFFFFFFFUL : 0xFFFFUL;
    htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    htim->Init.RepetitionCounter = 0;

    TIM_Encoder_InitTypeDef enc = {0};
    enc.EncoderMode  = TIM_ENCODERMODE_TI12;
    enc.IC1Polarity  = TIM_ICPOLARITY_RISING;
    enc.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC1Prescaler = TIM_ICPSC_DIV1;
    enc.IC1Filter    = 4;        /* filtre numérique léger contre les rebonds */
    enc.IC2Polarity  = TIM_ICPOLARITY_RISING;
    enc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC2Prescaler = TIM_ICPSC_DIV1;
    enc.IC2Filter    = 4;
    HAL_TIM_Encoder_Init(htim, &enc);

    HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(htim, 0);
}

/* ─── Lecture compteur signé ─────────────────────────────────────────────────*/

static int32_t read_local_counter(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return 0;
    const enc_cfg_t *c = &s_enc_cfg[id];
    if (c->instance == NULL) return 0;

    uint32_t raw = c->instance->CNT;
    /* 16-bit : extension de signe (transition 0x7FFF↔0x8000) ;
     * 32-bit : cast direct. */
    return c->is_32bit ? (int32_t)raw : (int32_t)(int16_t)raw;
}

static void zero_local_counter(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
    const enc_cfg_t *c = &s_enc_cfg[id];
    if (c->instance != NULL)
    {
        c->instance->CNT = 0;
    }
}

/* ─── API publique ───────────────────────────────────────────────────────────*/

bool Encoder_BSP_IsLocal(motor_id_t id)
{
    return BoardLink_IsLocalMotor(id);
}

void Encoder_BSP_Init(void)
{
    /* On n'initialise QUE les moteurs locaux de cette carte (strap PC0). */
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;

        const enc_cfg_t   *c    = &s_enc_cfg[i];
        TIM_HandleTypeDef *htim = htim_for(c->instance);
        if (c->instance == NULL || htim == NULL) continue;

        init_encoder(c, htim);
    }

    /* Position au boot = 0 rad. L'opérateur doit positionner la main
     * "ouverte + poignet droit" AVANT mise sous tension. */
    Encoder_BSP_ResetAll();
}

float Encoder_BSP_GetPosition(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return 0.0f;

    if (BoardLink_IsLocalMotor(id))
    {
        int32_t count = read_local_counter(id);
        int32_t delta = count - s_prev_count_local[id];

        s_velocity_rad_s[id]   = (float)delta * RAD_PER_COUNT / VELOCITY_DT_S;
        s_prev_count_local[id] = count;
        s_position_rad[id]     = (float)count * RAD_PER_COUNT;
    }
    /* Pour les moteurs distants, s_position_rad/velocity sont alimentés par
     * Encoder_BSP_UpdateRemote() depuis board_link.c. */

    return s_position_rad[id];
}

float Encoder_BSP_GetVelocity(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return 0.0f;
    return s_velocity_rad_s[id];
}

void Encoder_BSP_Reset(motor_id_t id)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;

    if (BoardLink_IsLocalMotor(id))
    {
        zero_local_counter(id);
    }
    /* Pour les distants : pas d'action ici. Pour forcer un reset côté slave,
     * envoyer une trame ENC_RESET sur board_link UART (TODO). */

    s_prev_count_local[id] = 0;
    s_position_rad[id]     = 0.0f;
    s_velocity_rad_s[id]   = 0.0f;
}

void Encoder_BSP_ResetAll(void)
{
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        Encoder_BSP_Reset((motor_id_t)i);
    }
}

void Encoder_BSP_UpdateRemote(motor_id_t id, float position_rad, float velocity_rad_s)
{
    if ((uint32_t)id >= MOTOR_COUNT) return;
    if (BoardLink_IsLocalMotor(id)) return;  /* on n'écrase pas un compteur local */

    s_position_rad[id]   = position_rad;
    s_velocity_rad_s[id] = velocity_rad_s;
}

/* ─── TODO : feedback slave → master via board_link ──────────────────────────
 *
 * Le bus UART board_link existe (USART3, master→slave pour les commandes).
 * Pour fermer la boucle, ajouter une trame slave→master :
 *
 *   [SYNC=0xBB][LEN=24][5 × (motor_id + pos_q15_h + pos_q15_l + vel_q15_h + vel_q15_l)][CRC]
 *
 * Côté slave : lire les 5 encodeurs locaux toutes les 10 ms, encoder en Q15,
 *              envoyer la trame sur USART3.
 * Côté master : parser la trame et appeler Encoder_BSP_UpdateRemote() pour chacun.
 *
 * À implémenter dans fw/comm/board_link.c (nouvelles fonctions + tâche).
 * ──────────────────────────────────────────────────────────────────────────── */
