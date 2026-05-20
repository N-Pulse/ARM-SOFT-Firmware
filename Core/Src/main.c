/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "motor_safety.h"
#include "comm.h"
#include "board_link.h"
#include "motor_map.h"   /* poses canoniques (OPEN/CLOSED/PINCH/NEUTRAL) */

/* ============================================================
 *  PRESENTATION BUILD — MAIN seule (poignet retiré).
 *
 *  MOTOR_CALIB_MODE (defaut) :
 *      Closed-loop par moteur via commandes ASCII sur le VCP.
 *      Tunnel ASCII master <-> slave sur USART3 -> un seul terminal
 *      (motor_calib.py sur le COM du master) pilote les 6 doigts.
 *      Inclut aussi les INTENTS pre-definis (touches O/C/P/N) qui
 *      reproduisent en closed-loop ce que la pipeline proto fait :
 *          O = OPEN_HAND   (toute la main vers 0)
 *          C = CLOSE_HAND  (poses CLOSED du motor_map)
 *          P = PINCH       (poses PINCH du motor_map)
 *          N = NEUTRAL     (poses NEUTRAL du motor_map)
 *      → utile pour la demo sans avoir a brancher l'EMG.
 *
 *  Pipeline proto (commenter MOTOR_CALIB_MODE pour activer) :
 *      Master ecoute sur LPUART1 [0xAA][len][DeviceMessage protobuf].
 *          python tools\send_action.py --port COM6 --action close
 *      Octets close : AA 06 12 04 0A 02 08 01.
 *
 *  Pour la version complete avec poignet, voir la branche
 *  `simulation_pipeline`.
 * ============================================================ */
#define MOTOR_CALIB_MODE
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
/* USER CODE BEGIN PV */
static TaskHandle_t sDefaultTaskHandle;
static TaskHandle_t sControlTaskHandle;
static TaskHandle_t sSafetyTaskHandle;
static TaskHandle_t sTelemetryTaskHandle;
static TimerHandle_t sLedTimer;

extern UART_HandleTypeDef hcom_uart[COMn];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
static void ControlTask(void *argument);
static void SafetyTask(void *argument);
static void TelemetryTask(void *argument);
static void CreateTasks(void);
static void LedTimerCallback(TimerHandle_t timer);
void App_Init(void);
void App_Task(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_THREADS */
  /* Create defaultTask (responsible for App_Init + CreateTasks once scheduler runs) */
  {
    BaseType_t s = xTaskCreate(StartDefaultTask, "defTask", 256, NULL,
                               tskIDLE_PRIORITY + 1, &sDefaultTaskHandle);
    configASSERT(s == pdPASS);
  }
  /* USER CODE END RTOS_THREADS */

  /* Initialize led */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Start scheduler (FreeRTOS native API) */
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ---------------------------------------------------------------------------
 * Infra encodeurs — TOUJOURS compilée (calibration ET sécurité anti-butée).
 * Autonome (pas fw/bsp).
 *
 * Build "presentation" : MAIN seule. Le MÊME binaire tourne sur les 2 cartes ;
 * chaque carte initialise/stream UNIQUEMENT ses encodeurs locaux
 * (BoardLink_IsLocalMotor), nommés correctement, sur SON propre VCP USB :
 *   - Master  : LITTLE, RING                  (2 encodeurs)
 *   - Slave   : THUMB, INDEX, MIDDLE, PALM    (4 encodeurs)
 *
 * Timer-sharing : un timer sert 1 moteur master ET 1 moteur slave, jamais en
 * même temps (rôle décidé au boot par le strap PC0). Voir encoder_bsp.h.
 *
 * Moon DCU10025P12 : 12 PPR ×4 = 48 cnt/tour moteur, gearbox 61:1
 * → 2928 cnt/tour arbre sortie.
 * --------------------------------------------------------------------------- */

/* prototypes directs (Core/Src/subdir.mk n'inclut pas fw/motors/motor_l298n) */
void Motor_L298N_SetRaw(motor_id_t id, int dir, uint8_t speed_pct);
void Motor_L298N_Stop(motor_id_t id);
int  Motor_L298N_DrivingDir(motor_id_t id);

#define WT_CNT_PER_OUTPUT_REV  2928L

typedef struct {
  const char   *name;
  TIM_TypeDef  *tim;
  bool          is32;
  GPIO_TypeDef *p1; uint16_t pin1; uint8_t af1;
  GPIO_TypeDef *p2; uint16_t pin2; uint8_t af2;
} wt_enc_t;

/* Indexé par motor_id (THUMB..PALM). Identique à fw/bsp/encoder/encoder_bsp.c. */
static const wt_enc_t s_wt[MOTOR_COUNT] = {
  [MOTOR_THUMB]   = { "THUMB",   TIM15, false, GPIOB, GPIO_PIN_14, 1, GPIOB, GPIO_PIN_15, 1 },
  [MOTOR_INDEX]   = { "INDEX",   TIM2,  true,  GPIOA, GPIO_PIN_0,  1, GPIOA, GPIO_PIN_1,  1 },
  [MOTOR_MIDDLE]  = { "MIDDLE",  TIM3,  false, GPIOA, GPIO_PIN_6,  2, GPIOA, GPIO_PIN_7,  2 },
  [MOTOR_RING]    = { "RING",    TIM8,  false, GPIOC, GPIO_PIN_6,  4, GPIOC, GPIO_PIN_7,  4 },
  [MOTOR_LITTLE]  = { "LITTLE",  TIM3,  false, GPIOA, GPIO_PIN_6,  2, GPIOA, GPIO_PIN_7,  2 },
  [MOTOR_PALM]    = { "PALM",    TIM20, false, GPIOB, GPIO_PIN_2,  3, GPIOC, GPIO_PIN_2,  6 },
};

static TIM_HandleTypeDef s_h2, s_h3, s_h8, s_h20;
/* TIM15 supprime de la liste : ce timer n'a PAS de mode encodeur sur G4
 * (slave mode controller present mais sans Encoder Mode 1/2/3). Pour
 * THUMB on utilise un decodeur quadrature software sur EXTI PB14/PB15
 * (voir Thumb_SwEnc_* plus bas). */

static TIM_HandleTypeDef *wt_h(TIM_TypeDef *t)
{
  if (t == TIM2)  return &s_h2;
  if (t == TIM3)  return &s_h3;
  if (t == TIM8)  return &s_h8;
  if (t == TIM20) return &s_h20;
  return NULL;
}

/* ---------------------------------------------------------------------------
 * Encoder logiciel THUMB — TIM15 ne supporte PAS Encoder Mode sur G4.
 * On configure PB14/PB15 en EXTI rising+falling, et a chaque transition on
 * decode la quadrature (table 16 etats : prev_AB | curr_AB -> +1/-1/0).
 * Le compteur s_thumb_cnt remplace __HAL_TIM_GET_COUNTER pour le motor THUMB.
 * --------------------------------------------------------------------------- */
static volatile int32_t s_thumb_cnt;
static volatile uint8_t s_thumb_last_ab;

static void thumb_swenc_init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Mode  = GPIO_MODE_IT_RISING_FALLING;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Pin   = GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOB, &g);

  uint8_t a = (GPIOB->IDR & GPIO_PIN_14) ? 1U : 0U;
  uint8_t b = (GPIOB->IDR & GPIO_PIN_15) ? 1U : 0U;
  s_thumb_last_ab = (uint8_t)((a << 1) | b);
  s_thumb_cnt     = 0;

  /* EXTI lines 14 & 15 partagent EXTI15_10_IRQn avec le bouton user PC13. */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* Appele depuis EXTI15_10_IRQHandler dans stm32g4xx_it.c quand PB14 ou
 * PB15 fire. Decode quadrature 4x identique a TIM_ENCODERMODE_TI12. */
void Thumb_SwEnc_IRQ(void)
{
  /* Edges peuvent venir de PB14 (A) ou PB15 (B). On clear les 2 pending
   * bits et on relit l'etat actuel des 2 lignes. */
  uint32_t pr = EXTI->PR1 & ((1UL << 14) | (1UL << 15));
  if (!pr) return;
  EXTI->PR1 = pr;

  uint8_t a   = (GPIOB->IDR & GPIO_PIN_14) ? 1U : 0U;
  uint8_t b   = (GPIOB->IDR & GPIO_PIN_15) ? 1U : 0U;
  uint8_t ab  = (uint8_t)((a << 1) | b);
  uint8_t key = (uint8_t)((s_thumb_last_ab << 2) | ab);
  s_thumb_last_ab = ab;

  /* Table 16 etats (Gray code) : prev<<2|curr -> direction
   *   00->01,01->11,11->10,10->00 = +1
   *   reciproque = -1
   *   00->11 / 11->00 / 01->10 / 10->01 = bond impossible (perte d'edge) -> 0 */
  static const int8_t quad_table[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
  };
  s_thumb_cnt += quad_table[key];
}

static void wt_clk(GPIO_TypeDef *p, TIM_TypeDef *t)
{
  if      (p == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
  else if (p == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
  else if (p == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
  if      (t == TIM2)  __HAL_RCC_TIM2_CLK_ENABLE();
  else if (t == TIM3)  __HAL_RCC_TIM3_CLK_ENABLE();
  else if (t == TIM8)  __HAL_RCC_TIM8_CLK_ENABLE();
  else if (t == TIM20) __HAL_RCC_TIM20_CLK_ENABLE();
}

static void wt_init_one(const wt_enc_t *e)
{
  /* THUMB est sur TIM15 qui ne supporte PAS Encoder Mode -> on bascule sur
   * le decodeur logiciel EXTI. Les pins PB14/PB15 sont alors initialisees
   * en GPIO+EXTI au lieu de AF/encoder. */
  if (e->tim == TIM15) {
    thumb_swenc_init();
    return;
  }

  TIM_HandleTypeDef *h = wt_h(e->tim);
  if (h == NULL || h->Instance != NULL) return;  /* déjà init (timer partagé) */

  wt_clk(e->p1, e->tim);
  wt_clk(e->p2, e->tim);

  GPIO_InitTypeDef g = {0};
  g.Mode  = GPIO_MODE_AF_PP;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Pin = e->pin1; g.Alternate = e->af1; HAL_GPIO_Init(e->p1, &g);
  g.Pin = e->pin2; g.Alternate = e->af2; HAL_GPIO_Init(e->p2, &g);

  TIM_Encoder_InitTypeDef enc = {0};
  h->Instance           = e->tim;
  h->Init.Prescaler     = 0;
  h->Init.CounterMode   = TIM_COUNTERMODE_UP;
  h->Init.Period        = e->is32 ? 0xFFFFFFFFUL : 0xFFFFUL;
  h->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  h->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  enc.EncoderMode  = TIM_ENCODERMODE_TI12;
  enc.IC1Polarity  = TIM_ICPOLARITY_RISING;
  enc.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  enc.IC1Prescaler = TIM_ICPSC_DIV1;
  enc.IC1Filter    = 4;
  enc.IC2Polarity  = TIM_ICPOLARITY_RISING;
  enc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  enc.IC2Prescaler = TIM_ICPSC_DIV1;
  enc.IC2Filter    = 4;
  HAL_TIM_Encoder_Init(h, &enc);
  HAL_TIM_Encoder_Start(h, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COUNTER(h, 0);
}

#ifndef MOTOR_CALIB_MODE
static int32_t wt_read_cnt(int i)
{
  const wt_enc_t *e = &s_wt[i];
  TIM_HandleTypeDef *h = wt_h(e->tim);
  if (h == NULL) return 0;
  uint32_t r = __HAL_TIM_GET_COUNTER(h);
  return e->is32 ? (int32_t)r : (int32_t)(int16_t)r;
}

/* ===========================================================================
 *  SÉCURITÉ ANTI-BUTÉE — TOUJOURS ACTIVE (pipeline normale + tests).
 *
 *  Surveille chaque moteur local : s'il est alimenté (L298N drive != 0) mais
 *  que son encodeur n'avance plus pendant GUARD_STALL_MS → il force contre une
 *  butée → on COUPE immédiatement (Motor_L298N_Stop). Indépendant de la
 *  commande : protège quel que soit le script (send_action.py…) ou la
 *  pipeline proto. Empêche la sur-course et la casse des câbles.
 *
 *  (En mode calibration ce garde est désactivé : le homing/seek POUSSE
 *   volontairement dans la butée pour la détecter.)
 * ======================================================================== */
#define GUARD_EPS_CNT     6     /* ~0.7° : en dessous = "n'avance plus" */
#define GUARD_TICK_MS     20
#define GUARD_STALL_MS    180   /* calé plus longtemps que ça → coupe */
#define GUARD_STALL_TICKS (GUARD_STALL_MS / GUARD_TICK_MS)

extern UART_HandleTypeDef hcom_uart[COMn];

static void MotorGuard_Task(void *argument)
{
  (void)argument;
  int32_t  ref[MOTOR_COUNT]   = {0};
  uint16_t still[MOTOR_COUNT] = {0};

  for (int i = 0; i < MOTOR_COUNT; i++)
    if (BoardLink_IsLocalMotor((motor_id_t)i)) wt_init_one(&s_wt[i]);

  for (;;)
  {
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
      if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;

      if (Motor_L298N_DrivingDir((motor_id_t)i) == 0) {
        ref[i] = wt_read_cnt(i); still[i] = 0;     /* à l'arrêt : rien à surveiller */
        continue;
      }
      int32_t c  = wt_read_cnt(i);
      int32_t dd = c - ref[i]; if (dd < 0) dd = -dd;
      if (dd >= GUARD_EPS_CNT) { ref[i] = c; still[i] = 0; }   /* ça avance, OK */
      else if (++still[i] >= GUARD_STALL_TICKS) {
        Motor_L298N_Stop((motor_id_t)i);            /* CALÉ EN BUTÉE → COUPE */
        still[i] = 0;
        uint8_t bk[] = { 0xAAU, 0x01U, 0xB0U | (uint8_t)i, 0x00U, 0x7EU };
        HAL_UART_Transmit(&hcom_uart[COM1], bk, sizeof(bk), 20U);  /* B0|id = guard cut */
      }
    }
    vTaskDelay(pdMS_TO_TICKS(GUARD_TICK_MS));
  }
}

static void MotorGuard_TaskCreate(void)
{
  (void)xTaskCreate(MotorGuard_Task, "guard", 384, NULL,
                    tskIDLE_PRIORITY + 3, NULL);   /* prio haute : sécurité */
}
#endif /* !MOTOR_CALIB_MODE */

#ifdef MOTOR_CALIB_MODE
/* ---------------------------------------------------------------------------
 * Calibration closed-loop par moteur (bang-bang + deadband).
 * Lit des commandes ASCII sur le VCP (g_rxring rempli par LPUART1_IRQHandler ;
 * en mode calib on NE démarre PAS Comms_Task, donc on possède le ring).
 * Pilote le L298N via Motor_L298N_SetRaw + feedback encodeur.
 * Chaque carte ne calibre QUE ses moteurs locaux (BoardLink_IsLocalMotor).
 * --------------------------------------------------------------------------- */
extern volatile uint8_t  g_rxring[];
extern volatile uint16_t g_rxring_head;
extern volatile uint16_t g_rxring_tail;
#define CB_RXRING_SIZE   256U

#define CB_SPEED         45U     /* % PWM par défaut (réglable par moteur via 'v') */
#define CB_DEADBAND_CNT  6       /* ~0.7° (2928 cnt/tour) — serré pour atteindre les fins de course */
#define CB_JOG_DEG       3
#define CB_STUCK_TICKS   80      /* ×10ms : pas d'amélioration → stop (mauvais sens?) */
#define CB_TICK_MS       10
#define CB_STATUS_EVERY  40      /* ×10ms = 400ms */

static int32_t  cb_target[MOTOR_COUNT];
static bool     cb_active[MOTOR_COUNT];
static int8_t   cb_dir[MOTOR_COUNT];       /* sens de câblage : +1 / -1 */
static int32_t  cb_bestaerr[MOTOR_COUNT];
static uint16_t cb_stuck[MOTOR_COUNT];
static int16_t  cb_close_deg[MOTOR_COUNT] = {
  [MOTOR_THUMB]=42,[MOTOR_INDEX]=42,[MOTOR_MIDDLE]=42,
  [MOTOR_RING]=40,[MOTOR_LITTLE]=37,[MOTOR_PALM]=5,
};
/* PWM par moteur (%). PALM tire des câbles → plus de couple pour passer le
 * point dur à mi-course. Réglable à chaud : commande  v<pct>  (ex: v75). */
static uint8_t  cb_speed[MOTOR_COUNT] = {
  [MOTOR_THUMB]=45,[MOTOR_INDEX]=45,[MOTOR_MIDDLE]=45,
  [MOTOR_RING]=45,[MOTOR_LITTLE]=45,[MOTOR_PALM]=75,
};
static int cb_sel = -1;

/* --- auto-homing par calage mécanique --- */
#define CB_HOME_EPS      8       /* ~1° : progrès mini pour dire "ça bouge" */
#define CB_HOME_STALL    60      /* ×10ms = 600ms sans progrès = vraie butée
                                    (tolère les points durs type cables PALM) */
#define CB_HOME_TIMEOUT  1000    /* ×10ms : sécurité (encodeur mort) */
static uint8_t  cb_hphase[MOTOR_COUNT];   /* 0 idle, 1 butée A, 2 butée B, 3 seek (o/c) */
static int32_t  cb_hstart[MOTOR_COUNT];
static int32_t  cb_href[MOTOR_COUNT];
static uint16_t cb_hstill[MOTOR_COUNT];
static uint16_t cb_ht[MOTOR_COUNT];
static int32_t  cb_endA[MOTOR_COUNT];
static int32_t  cb_open_cnt[MOTOR_COUNT];
static int32_t  cb_close_cnt[MOTOR_COUNT];
static int8_t   cb_open_raw[MOTOR_COUNT];  /* sens raw L298N pour aller à la butée OPEN  */
static int8_t   cb_close_raw[MOTOR_COUNT]; /* sens raw L298N pour aller à la butée CLOSE */
static int8_t   cb_seek_raw[MOTOR_COUNT];  /* sens courant pendant un seek o/c */
static bool     cb_homed[MOTOR_COUNT];

static int32_t cb_labs(int32_t v) { return v < 0 ? -v : v; }

static int32_t cb_deg2cnt(int deg)
{ return (int32_t)(((int64_t)deg * WT_CNT_PER_OUTPUT_REV) / 360); }

static int32_t cb_cnt(int i)
{
  const wt_enc_t *e = &s_wt[i];
  /* THUMB : compteur software (TIM15 ne fait pas d'encoder mode). */
  if (e->tim == TIM15) return s_thumb_cnt;
  TIM_HandleTypeDef *h = wt_h(e->tim);
  if (h == NULL) return 0;
  uint32_t r = __HAL_TIM_GET_COUNTER(h);
  return e->is32 ? (int32_t)r : (int32_t)(int16_t)r;
}

static void cb_arm(int i, int32_t target)
{
  if (i < 0 || i >= MOTOR_COUNT || !BoardLink_IsLocalMotor((motor_id_t)i)) return;
  cb_target[i]    = target;
  cb_active[i]    = true;
  cb_bestaerr[i]  = 0x7FFFFFFF;
  cb_stuck[i]     = 0;
}

static void cb_goto_deg(int i, int deg)        { cb_arm(i, cb_deg2cnt(deg)); }
static void cb_jog(int i, int ddeg)            { cb_arm(i, cb_cnt(i) + cb_deg2cnt(ddeg)); }

/* Diag encodeurs : snapshot des comptes + niveaux BRUTS des voies A/B
 * lues directement sur l'IDR du port GPIO. Permet de verifier le cablage
 * sans avoir besoin que le moteur tourne :
 *   - tu fais tourner l'arbre a la main, tu retapes W :
 *       cnt doit changer        -> encodeur OK
 *       cnt ne change pas mais A/B togglent -> probleme TIM (config AF/mode)
 *       A et/ou B figes a 0 ou 1 -> voie pas cablee / pas alimentee
 *       les 6 montrent A=1 B=1 fixes -> pull-up interne + voies en l'air
 *                                       (encodeur pas alimente ou GND manquant)
 */
static void cb_watch_enc(void)
{
  for (int i = 0; i < MOTOR_COUNT; i++) {
    if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
    const wt_enc_t *e = &s_wt[i];
    int a = ((e->p1->IDR & e->pin1) != 0U) ? 1 : 0;
    int b = ((e->p2->IDR & e->pin2) != 0U) ? 1 : 0;
    long c = (long)cb_cnt(i);
    printf("ENC %-8s cnt=%6ld  A=%d B=%d\r\n", e->name, c, a, b);
  }
}

/* Applique une pose canonique (OPEN/CLOSED/PINCH/NEUTRAL) sur tous les
 * moteurs LOCAUX de cette carte, en closed-loop. Equivalent calib de
 * apply_full_pose() de l'IntentRouter — meme table motor_map. */
static void cb_apply_pose(motor_pose_id_t pose, const char *label)
{
  for (int i = 0; i < MOTOR_COUNT; i++) {
    if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
    float    target_rad = MotorMap_GetPose(pose, (motor_id_t)i);
    /* rad -> counts : counts = rad * (cnt_par_tour / 2*pi) */
    int32_t  target_cnt = (int32_t)(target_rad * (WT_CNT_PER_OUTPUT_REV / 6.28318530f));
    cb_arm(i, target_cnt);
  }
  printf("CAL INTENT %s -> closed-loop sur les locaux\r\n", label);
}

static void cb_stop(int i)
{
  if (i < 0 || i >= MOTOR_COUNT) return;
  cb_active[i]  = false;
  cb_hphase[i]  = 0;
  Motor_L298N_SetRaw((motor_id_t)i, 0, 0);
}

static void cb_home_start(int i)
{
  if (i < 0 || i >= MOTOR_COUNT || !BoardLink_IsLocalMotor((motor_id_t)i)) return;
  cb_active[i] = false;
  cb_hphase[i] = 1;                 /* phase 1 : pousse en raw +1 vers une butée */
  cb_hstart[i] = cb_cnt(i);
  cb_href[i]   = cb_hstart[i];
  cb_hstill[i] = 0;
  cb_ht[i]     = 0;
  printf("CAL %s HOMING... (calage des 2 butees)\r\n", s_wt[i].name);
}

/* seek = pousse dans un sens jusqu'à caler sur la butée (idem H, 1 direction).
 * Reproduit EXACTEMENT la course de H, sans deadband ni recul. */
static void cb_seek_start(int i, int raw)
{
  if (i < 0 || i >= MOTOR_COUNT || !BoardLink_IsLocalMotor((motor_id_t)i)) return;
  cb_active[i]   = false;
  cb_hphase[i]   = 3;
  cb_seek_raw[i] = (int8_t)raw;
  cb_href[i]     = cb_cnt(i);
  cb_hstill[i]   = 0;
  cb_ht[i]       = 0;
}

static bool cb_pop(uint8_t *b)
{
  if (g_rxring_head == g_rxring_tail) return false;
  *b = g_rxring[g_rxring_tail];
  g_rxring_tail = (uint16_t)((g_rxring_tail + 1U) % CB_RXRING_SIZE);
  return true;
}

/* atoi minimal sur la fin de ligne (gère le '-'). */
static int cb_atoi(const char *s)
{
  int sign = 1, v = 0;
  while (*s == ' ') s++;
  if (*s == '-') { sign = -1; s++; }
  while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
  return sign * v;
}

static void cb_list(void)
{
  for (int i = 0; i < MOTOR_COUNT; i++)
  {
    if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
    int32_t c  = cb_cnt(i);
    long cdeg  = (long)((int64_t)c * 3600 / WT_CNT_PER_OUTPUT_REV); /* 1/10 ° */
    printf("CAL [%d] %-8s cnt=%ld deg=%ld.%ld close=%d dir=%d %s%s\r\n",
           i, s_wt[i].name, (long)c, cdeg/10, (cdeg<0?-cdeg:cdeg)%10,
           cb_close_deg[i], cb_dir[i],
           cb_active[i] ? "RUN" : "idle",
           (i == cb_sel) ? " <-sel" : "");
  }
}

static void cb_handle_line(char *ln)
{
  while (*ln == ' ') ln++;
  char cmd = *ln;
  int  arg = cb_atoi(ln + 1);

  if (cmd >= '0' && cmd <= '5') {
    int i = cmd - '0';
    if (BoardLink_IsLocalMotor((motor_id_t)i)) {
      cb_sel = i;
      printf("CAL sel=%s\r\n", s_wt[i].name);
    } else {
      /* L'utilisateur sélectionne un moteur de l'AUTRE carte → on
       * désélectionne ici pour que H/o/c/+/-/g/v/s ne tapent pas
       * accidentellement sur un moteur local. La carte qui possède
       * le moteur ciblé le sélectionne de son côté (commande forwardée
       * via le tunnel USART3). */
      cb_sel = -1;
    }
    return;
  }
  switch (cmd) {
    case '?': cb_list(); break;
    case 'H': cb_home_start(cb_sel); break;
    case 'A': for (int i=0;i<MOTOR_COUNT;i++) if(BoardLink_IsLocalMotor((motor_id_t)i)) cb_home_start(i); break;
    case 'x':
      if (cb_sel>=0 && cb_homed[cb_sel]) {
        int32_t tc=cb_open_cnt[cb_sel]; cb_open_cnt[cb_sel]=cb_close_cnt[cb_sel]; cb_close_cnt[cb_sel]=tc;
        int8_t  tr=cb_open_raw[cb_sel]; cb_open_raw[cb_sel]=cb_close_raw[cb_sel]; cb_close_raw[cb_sel]=tr;
        printf("CAL %s open/close SWAP\r\n", s_wt[cb_sel].name);
      } break;
    case 'o':
      if (cb_sel>=0 && cb_homed[cb_sel]) { cb_seek_start(cb_sel, cb_open_raw[cb_sel]);  printf("CAL %s -> OPEN (seek butee)\r\n", s_wt[cb_sel].name); }
      else { cb_goto_deg(cb_sel, 0); printf("CAL %s -> 0deg (pas home, fais H)\r\n", cb_sel>=0?s_wt[cb_sel].name:"?"); }
      break;
    case 'c':
      if (cb_sel>=0 && cb_homed[cb_sel]) { cb_seek_start(cb_sel, cb_close_raw[cb_sel]); printf("CAL %s -> CLOSE (seek butee)\r\n", s_wt[cb_sel].name); }
      else { cb_goto_deg(cb_sel, cb_close_deg[cb_sel]); printf("CAL %s -> %ddeg (pas home, fais H)\r\n", cb_sel>=0?s_wt[cb_sel].name:"?", cb_sel>=0?cb_close_deg[cb_sel]:0); }
      break;
    case '+': cb_jog(cb_sel, +CB_JOG_DEG);            break;
    case '-': cb_jog(cb_sel, -CB_JOG_DEG);            break;
    case 'g': cb_goto_deg(cb_sel, arg);               printf("CAL %s -> %d deg\r\n", cb_sel>=0?s_wt[cb_sel].name:"?", arg); break;
    case 'k': if (cb_sel>=0){ cb_close_deg[cb_sel]=(int16_t)arg; printf("CAL %s close=%d\r\n", s_wt[cb_sel].name, arg);} break;
    case 'v':
      if (cb_sel>=0) {
        if (arg < 20) arg = 20; if (arg > 100) arg = 100;
        cb_speed[cb_sel] = (uint8_t)arg;
        printf("CAL %s speed=%d%%\r\n", s_wt[cb_sel].name, arg);
      } break;
    case 's': cb_stop(cb_sel);                        break;
    case 'S': for (int i=0;i<MOTOR_COUNT;i++) cb_stop(i); printf("CAL ALL STOP\r\n"); break;
    case 'z':
      if (cb_sel>=0){
        if (s_wt[cb_sel].tim == TIM15) { s_thumb_cnt = 0; }
        else { TIM_HandleTypeDef*h=wt_h(s_wt[cb_sel].tim); if(h){__HAL_TIM_SET_COUNTER(h,0);} }
        cb_active[cb_sel]=false; cb_target[cb_sel]=0;
        printf("CAL %s ZERO\r\n", s_wt[cb_sel].name);
      } break;
    case 'i':
      { int x = (ln[1]>='0'&&ln[1]<='5') ? arg : cb_sel;
        if (x>=0 && x<MOTOR_COUNT){ cb_dir[x]=(int8_t)-cb_dir[x]; printf("CAL %s dir=%d\r\n", s_wt[x].name, cb_dir[x]); } }
      break;
    case 'd':
      for (int i=0;i<MOTOR_COUNT;i++){ if(!BoardLink_IsLocalMotor((motor_id_t)i))continue;
        if (cb_homed[i]) {
          long sp=(long)((int64_t)cb_labs(cb_close_cnt[i]-cb_open_cnt[i])*3600/WT_CNT_PER_OUTPUT_REV);
          printf("DUMP %-8s course=%ld.%ld deg (open=%ld close=%ld cnt)\r\n",
                 s_wt[i].name, sp/10,(sp<0?-sp:sp)%10,
                 (long)cb_open_cnt[i],(long)cb_close_cnt[i]);
        } else {
          printf("DUMP %-8s pas calibre (fais H)\r\n", s_wt[i].name);
        } }
      break;
    /* --- INTENTS pre-definis (poses du motor_map, closed-loop sur les 6) --- */
    case 'O': cb_apply_pose(MOTOR_POSE_OPEN,    "OPEN_HAND");   break;
    case 'C': cb_apply_pose(MOTOR_POSE_CLOSED,  "CLOSE_HAND");  break;
    case 'P': cb_apply_pose(MOTOR_POSE_PINCH,   "PINCH");       break;
    case 'N': cb_apply_pose(MOTOR_POSE_NEUTRAL, "NEUTRAL");     break;
    /* --- DIAG : snapshot encodeurs (count + niveaux bruts A/B) --- */
    case 'W': cb_watch_enc(); break;
    /* --- DIAG : drive RAW indefini (multimetre sur OUT1/OUT2 du L298N) ---
     *   T+   = drive +1 jusqu'a 's'  (IN1=HIGH IN2=LOW PWM=cb_speed)
     *   T-   = drive -1 jusqu'a 's'  (IN1=LOW  IN2=HIGH)
     *   Ts   = stop (equivalent a 's')
     * Pas de bang-bang, pas de stuck-timeout. Le moteur tourne tant qu'on
     * tape pas 's' (ou Ts ou S). Permet de mesurer tranquillement les
     * tensions sur le driver sans que l'anti-butee coupe au bout de 800ms. */
    case 'T': {
      if (cb_sel < 0) { printf("CAL T : selectionne un moteur d'abord\r\n"); break; }
      char sub = ln[1];
      int8_t dir = (sub == '+') ? +1 : (sub == '-') ? -1 : 0;
      if (dir == 0) {
        cb_stop(cb_sel);
        printf("CAL %s T stop\r\n", s_wt[cb_sel].name);
      } else {
        cb_active[cb_sel]  = false;
        cb_hphase[cb_sel]  = 0;
        Motor_L298N_SetRaw((motor_id_t)cb_sel, dir, cb_speed[cb_sel]);
        printf("CAL %s T drive raw %+d speed=%d%% (s pour stop)\r\n",
               s_wt[cb_sel].name, dir, cb_speed[cb_sel]);
      }
    } break;
    default: break;
  }
}

/* ---------------------------------------------------------------------------
 * Tunnel ASCII master ↔ slave (calib mode uniquement)
 *
 * Le mode calib n'utilise PAS le protocole framé sur USART3 (Comms_Task
 * pas démarré) → on s'en sert comme d'un simple tuyau série :
 *   - master : chaque caractère tapé est forwardé au slave ;
 *              les bytes reçus du slave (printf redirigés) sont relayés
 *              au VCP du master.
 *   - slave  : les bytes reçus du master sont injectés dans le parseur
 *              de Calib_Task (même chemin que le VCP local) ; tous ses
 *              printf passent par _write override → USART3 → master.
 *
 * Résultat côté utilisateur : un seul terminal (celui du master)
 * pilote les 6 moteurs des 2 cartes. */
#ifdef MOTOR_CALIB_MODE
int _write(int file, char *ptr, int len)
{
  (void)file;
  if (BoardLink_Role() == BOARD_ROLE_SLAVE) {
    for (int i = 0; i < len; i++) BoardLink_RawSendByte((uint8_t)ptr[i]);
  } else {
    HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)ptr, (uint16_t)len, 100U);
  }
  return len;
}
#endif

static void Calib_Task(void *argument)
{
  (void)argument;
  char ln[32]; uint8_t li = 0;
  const bool is_master = (BoardLink_Role() == BOARD_ROLE_MASTER);

  for (int i = 0; i < MOTOR_COUNT; i++) {
    cb_dir[i] = 1; cb_active[i] = false;
    if (BoardLink_IsLocalMotor((motor_id_t)i)) {
      wt_init_one(&s_wt[i]);
      if (cb_sel < 0) cb_sel = i;
    }
  }
  /* Seul le master imprime la bannière sur le VCP de l'utilisateur.
   * (Le slave imprimerait via le tunnel USART3 → message en double.) */
  if (is_master) {
    printf("\r\n=== CALIB MODE (presentation: main seule, 1 terminal) ===\r\n");
    printf("  ?  liste les 6 moteurs (locaux + distants via USART3)\r\n");
    printf("  0-5 select | H auto-home sel | A auto-home tous | S stop tous\r\n");
    printf("  o open | c close (moteur selectionne) | x swap | d dump\r\n");
    printf("  v<pct> force/vitesse du moteur (ex: v75 si cale a mi-course)\r\n");
    printf("  INTENTS toute la main : O=open  C=close  P=pinch  N=neutral\r\n");
    printf("  W = snapshot encodeurs (cnt + niveaux bruts A/B) -- diag cablage\r\n");
    printf("  T+/T-/Ts = drive RAW indefini sel (multimetre L298N OUT1/OUT2)\r\n");
    printf("  workflow: select -> H (le moteur cale les 2 butees seul) -> o/c\r\n");
  }
  cb_list();   /* master imprime ses locaux, slave les siens via le tunnel */

  for (;;)
  {
    /* --- commandes : VCP local (utilisateur sur le master) --- */
    uint8_t b;
    while (cb_pop(&b)) {
      /* Master : forwarde chaque caractère au slave pour que les 2 cartes
       * voient la même ligne (la sélection cb_sel reste cohérente). */
      if (is_master) BoardLink_RawSendByte(b);
      if (b == '\n' || b == '\r') { ln[li] = 0; if (li) cb_handle_line(ln); li = 0; }
      else if (li < sizeof(ln) - 1) ln[li++] = (char)b;
    }

    /* --- commandes/affichage : USART3 (tunnel inter-cartes) --- */
    uint8_t bb;
    while (BoardLink_RawRecvByte(&bb)) {
      if (is_master) {
        /* Bytes reçus = printf du slave → on les relaye sur le VCP utilisateur. */
        HAL_UART_Transmit(&hcom_uart[COM1], &bb, 1U, 5U);
      } else {
        /* Bytes reçus = ligne tapée par l'utilisateur côté master → on
         * la passe au parseur exactement comme si elle venait du VCP. */
        if (bb == '\n' || bb == '\r') { ln[li] = 0; if (li) cb_handle_line(ln); li = 0; }
        else if (li < sizeof(ln) - 1) ln[li++] = (char)bb;
      }
    }

    /* --- boucle d'asservissement par moteur (bang-bang + deadband) --- */
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
      if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;

      /* --- homing (phase 1/2) ou seek o/c (phase 3) : pousse jusqu'à caler --- */
      if (cb_hphase[i]) {
        int raw = (cb_hphase[i] == 1) ? +1
                : (cb_hphase[i] == 2) ? -1
                : cb_seek_raw[i];
        Motor_L298N_SetRaw((motor_id_t)i, raw, cb_speed[i]);
        int32_t hc = cb_cnt(i);
        cb_ht[i]++;
        if (cb_labs(hc - cb_href[i]) >= CB_HOME_EPS) { cb_href[i] = hc; cb_hstill[i] = 0; }
        else cb_hstill[i]++;

        if (cb_hstill[i] > CB_HOME_STALL || cb_ht[i] > CB_HOME_TIMEOUT) {
          Motor_L298N_SetRaw((motor_id_t)i, 0, 0);
          if (cb_hphase[i] == 1) {
            cb_endA[i] = hc;
            cb_dir[i]  = (cb_endA[i] >= cb_hstart[i]) ? +1 : -1;  /* raw+1 → +cnt ? */
            cb_hphase[i] = 2;                     /* phase 2 : autre butée (raw -1) */
            cb_hstart[i] = hc; cb_href[i] = hc; cb_hstill[i] = 0; cb_ht[i] = 0;
            printf("CAL %s butee1 cnt=%ld -> phase2\r\n", s_wt[i].name, (long)hc);
          } else if (cb_hphase[i] == 2) {
            int32_t a = cb_endA[i], bnd = hc;
            int32_t lo = (a < bnd) ? a : bnd;
            int32_t hi = (a < bnd) ? bnd : a;
            /* PAS de recul : open/close = vraies butées (o/c referont le calage). */
            cb_open_cnt[i]  = lo;
            cb_close_cnt[i] = hi;
            cb_open_raw[i]  = (int8_t)-cb_dir[i]; /* vers lo = sens qui baisse le cnt */
            cb_close_raw[i] = (int8_t)+cb_dir[i]; /* vers hi = sens qui monte le cnt  */
            cb_homed[i]     = true;
            cb_hphase[i]    = 0;
            long sp = (long)((int64_t)(hi - lo) * 3600 / WT_CNT_PER_OUTPUT_REV);
            printf("CAL %s HOMED course=%ld.%ld deg  (o=open c=close ; si inverse: x)\r\n",
                   s_wt[i].name, sp/10, (sp<0?-sp:sp)%10);
          } else {
            /* phase 3 : seek o/c terminé sur butée — exactement comme H */
            cb_hphase[i] = 0;
            if (cb_seek_raw[i] == cb_open_raw[i]) {
              /* on est sur la butée OUVERTE → re-référence : open = 0.
               * Élimine la dérive : 'c' affichera la vraie course depuis 0. */
              if (s_wt[i].tim == TIM15) { s_thumb_cnt = 0; }
              else { TIM_HandleTypeDef *h = wt_h(s_wt[i].tim); if (h) __HAL_TIM_SET_COUNTER(h, 0); }
              printf("CAL %s OPEN (butee, re-zero -> 0.0 deg)\r\n", s_wt[i].name);
            } else {
              long cd = (long)((int64_t)hc * 3600 / WT_CNT_PER_OUTPUT_REV);
              printf("CAL %s CLOSE (butee) course=%ld.%ld deg\r\n",
                     s_wt[i].name, cd/10, (cd<0?-cd:cd)%10);
            }
          }
        }
        continue;                                /* en homing/seek : pas de bang-bang */
      }

      if (!cb_active[i]) continue;
      int32_t cnt  = cb_cnt(i);
      int32_t err  = cb_target[i] - cnt;
      int32_t aerr = err < 0 ? -err : err;

      if (aerr <= CB_DEADBAND_CNT) {
        cb_stop(i);
        long cd = (long)((int64_t)cnt * 3600 / WT_CNT_PER_OUTPUT_REV);
        printf("CAL %s REACHED cnt=%ld (%ld.%ld deg)\r\n",
               s_wt[i].name, (long)cnt, cd/10, (cd<0?-cd:cd)%10);
        continue;
      }
      int drive = ((err > 0) ? 1 : -1) * cb_dir[i];
      Motor_L298N_SetRaw((motor_id_t)i, drive, cb_speed[i]);

      if (aerr < cb_bestaerr[i]) { cb_bestaerr[i] = aerr; cb_stuck[i] = 0; }
      else if (++cb_stuck[i] > CB_STUCK_TICKS) {
        cb_stop(i);
        printf("CAL %s BLOQUE/ MAUVAIS SENS — tape i%d puis relance\r\n",
               s_wt[i].name, i);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(CB_TICK_MS));
  }
}

static void Calib_TaskCreate(void)
{
  (void)xTaskCreate(Calib_Task, "calib", 512, NULL,
                    tskIDLE_PRIORITY + 2, NULL);
}
#endif /* MOTOR_CALIB_MODE */

static void CreateTasks(void)
{
  BaseType_t status = pdPASS;

  status = xTaskCreate(ControlTask, "ctrl", 256, NULL, tskIDLE_PRIORITY + 3, &sControlTaskHandle);
  configASSERT(status == pdPASS);

  status = xTaskCreate(SafetyTask, "safe", 256, NULL, tskIDLE_PRIORITY + 2, &sSafetyTaskHandle);
  configASSERT(status == pdPASS);

  status = xTaskCreate(TelemetryTask, "tele", 256, NULL, tskIDLE_PRIORITY + 1, &sTelemetryTaskHandle);
  configASSERT(status == pdPASS);

#ifdef MOTOR_CALIB_MODE
  /* Mode calibration : la tâche calib possède le VCP (g_rxring),
   * on ne démarre PAS la pile proto. */
  Calib_TaskCreate();
#else
  Comms_TaskCreate();
  BoardLink_TaskCreate();    /* slave : drain USART3 framé proto (no-op sur master) */
  MotorGuard_TaskCreate();   /* SÉCURITÉ anti-butée — toujours active */
#endif

  sLedTimer = xTimerCreate("led",
                           pdMS_TO_TICKS(500),
                           pdTRUE,
                           NULL,
                           LedTimerCallback);
  configASSERT(sLedTimer != NULL);
  xTimerStart(sLedTimer, 0);
}

static void StartScheduler(void)
{
  vTaskStartScheduler();
}

static void ControlTask(void *argument)
{
  (void)argument;
  const TickType_t period = pdMS_TO_TICKS(10); /* 100 Hz loop */
  TickType_t       last_wake = xTaskGetTickCount();

  while (1)
  {
    App_Task();
    Motor_Update();
    vTaskDelayUntil(&last_wake, period);
  }
}

static void SafetyTask(void *argument)
{
  (void)argument;
  const TickType_t period = pdMS_TO_TICKS(20);
  TickType_t       last_wake = xTaskGetTickCount();

  while (1)
  {
    if (MotorSafety_IsFaultActive())
    {
      Motor_StopAll();
    }
    vTaskDelayUntil(&last_wake, period);
  }
}

static void TelemetryTask(void *argument)
{
  (void)argument;
  const TickType_t period = pdMS_TO_TICKS(1000);

  while (1)
  {
    vTaskDelay(period);
  }
}

static void LedTimerCallback(TimerHandle_t timer)
{
  (void)timer;
  BSP_LED_Toggle(LED_GREEN);
}

void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  taskDISABLE_INTERRUPTS();
  Error_Handler();
}

/* Required when configSUPPORT_STATIC_ALLOCATION = 1 */
static StaticTask_t s_idle_tcb;
static StackType_t  s_idle_stack[configMINIMAL_STACK_SIZE];
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer   = &s_idle_tcb;
  *ppxIdleTaskStackBuffer = s_idle_stack;
  *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

static StaticTask_t s_timer_tcb;
static StackType_t  s_timer_stack[configTIMER_TASK_STACK_DEPTH];
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t  **ppxTimerTaskStackBuffer,
                                    uint32_t      *pulTimerTaskStackSize)
{
  *ppxTimerTaskTCBBuffer   = &s_timer_tcb;
  *ppxTimerTaskStackBuffer = s_timer_stack;
  *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  (void)argument;

  /* Boot beacon — scheduler is now running, HAL timebase is TIM6
   * (independent of FreeRTOS SysTick), so HAL_UART_Transmit is safe. */
  {
    uint8_t boot[] = {0xAAU, 0x01U, 0xFDU, 0x00U, 0x58U};
    HAL_UART_Transmit(&hcom_uart[COM1], boot, sizeof(boot), 100U);
  }

  /* App init: creates motor backend (mutex, timer, SimTxTask),
   * comm queue/timer. Safe here since scheduler is running. */
  App_Init();

  /* Create user tasks (Control, Safety, Telemetry, Comms). */
  CreateTasks();

  /* RX is driven by LPUART1_IRQHandler (in stm32g4xx_it.c) which fills a
   * ring buffer drained by Comms_Task. Enable RX FIFO (8 bytes) + RXNE
   * interrupt + NVIC line, so byte arrival wakes the IRQ immediately and
   * we never overrun even on back-to-back bursts. */
  __HAL_UART_CLEAR_FLAG(&hcom_uart[COM1], UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);
  hcom_uart[COM1].Instance->CR1 &= ~USART_CR1_UE;             /* UE off to change FIFOEN */
  hcom_uart[COM1].Instance->CR1 |= USART_CR1_FIFOEN;
  hcom_uart[COM1].Instance->CR1 |= USART_CR1_RXNEIE_RXFNEIE;  /* RX interrupt on byte */
  hcom_uart[COM1].Instance->CR1 |= USART_CR1_UE;              /* UE back on */
  HAL_NVIC_SetPriority(LPUART1_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(LPUART1_IRQn);

  /* defaultTask done with init — idle forever. */
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
