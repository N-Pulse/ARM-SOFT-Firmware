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

/* ============================================================
 *  MODES DE BRING-UP (décommenter UN seul, ou aucun = build normal)
 *
 *  WRIST_ENCODER_TEST : stream les encodeurs locaux (lecture seule),
 *      la pipeline proto normale tourne en //.
 *
 *  MOTOR_CALIB_MODE   : asservissement closed-loop par moteur via
 *      commandes ASCII sur le VCP (PAS la pipeline proto). Sert à
 *      trouver/définir les bons angles de chaque moteur.
 *      → utiliser le script tools motor_calib.py
 * ============================================================ */
/* #define WRIST_ENCODER_TEST */
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
 * Infra encodeurs — TOUJOURS compilée (test, calibration, ET sécurité
 * anti-butée de la pipeline normale). Autonome (pas fw/bsp).
 *
 * Le MÊME binaire tourne sur les 2 cartes. Chaque carte initialise et stream
 * UNIQUEMENT ses encodeurs locaux (BoardLink_IsLocalMotor), nommés correctement,
 * sur SON propre VCP USB :
 *   - Master  : WRIST_X, WRIST_Y, LITTLE   (3 encodeurs)
 *   - Slave   : INDEX, MIDDLE, RING, THUMB, PALM   (5 encodeurs)
 * → pour voir les 8, ouvre le script sur le COM du master ET sur celui du slave.
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
void Motor_L298N_Brake(motor_id_t id);

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
  [MOTOR_WRIST_X] = { "WRIST_X", TIM2,  true,  GPIOA, GPIO_PIN_0,  1, GPIOA, GPIO_PIN_1,  1 },
  [MOTOR_WRIST_Y] = { "WRIST_Y", TIM8,  false, GPIOC, GPIO_PIN_6,  4, GPIOC, GPIO_PIN_7,  4 },
  [MOTOR_PALM]    = { "PALM",    TIM20, false, GPIOB, GPIO_PIN_2,  3, GPIOC, GPIO_PIN_2,  6 },
};

static TIM_HandleTypeDef s_h2, s_h3, s_h8, s_h15, s_h20;

static TIM_HandleTypeDef *wt_h(TIM_TypeDef *t)
{
  if (t == TIM2)  return &s_h2;
  if (t == TIM3)  return &s_h3;
  if (t == TIM8)  return &s_h8;
  if (t == TIM15) return &s_h15;
  if (t == TIM20) return &s_h20;
  return NULL;
}

static void wt_clk(GPIO_TypeDef *p, TIM_TypeDef *t)
{
  if      (p == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
  else if (p == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
  else if (p == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
  if      (t == TIM2)  __HAL_RCC_TIM2_CLK_ENABLE();
  else if (t == TIM3)  __HAL_RCC_TIM3_CLK_ENABLE();
  else if (t == TIM8)  __HAL_RCC_TIM8_CLK_ENABLE();
  else if (t == TIM15) __HAL_RCC_TIM15_CLK_ENABLE();
  else if (t == TIM20) __HAL_RCC_TIM20_CLK_ENABLE();
}

static void wt_init_one(const wt_enc_t *e)
{
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
 *  commande : protège quel que soit le script (send_action.py, wrist_test.py…)
 *  ou la pipeline proto. Empêche la sur-course et la casse des câbles.
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

#ifdef WRIST_ENCODER_TEST
static void WristTest_Task(void *argument)
{
  (void)argument;

  /* Init des encodeurs LOCAUX de cette carte uniquement. */
  for (int i = 0; i < MOTOR_COUNT; i++)
  {
    if (BoardLink_IsLocalMotor((motor_id_t)i))
      wt_init_one(&s_wt[i]);
  }

  for (;;)
  {
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
      if (!BoardLink_IsLocalMotor((motor_id_t)i)) continue;
      const wt_enc_t *e = &s_wt[i];
      TIM_HandleTypeDef *h = wt_h(e->tim);
      if (h == NULL) continue;

      uint32_t raw = __HAL_TIM_GET_COUNTER(h);
      int32_t cnt  = e->is32 ? (int32_t)raw : (int32_t)(int16_t)raw;
      long mdeg    = (long)((int64_t)cnt * 360000 / WT_CNT_PER_OUTPUT_REV);

      printf("ENC %s cnt=%ld mdeg=%ld\r\n", e->name, (long)cnt, mdeg);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

static void WristTest_TaskCreate(void)
{
  (void)xTaskCreate(WristTest_Task, "enctest", 384, NULL,
                    tskIDLE_PRIORITY + 1, NULL);
}
#endif /* WRIST_ENCODER_TEST */

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
  [MOTOR_THUMB]=42,[MOTOR_INDEX]=42,[MOTOR_MIDDLE]=42,[MOTOR_RING]=40,
  [MOTOR_LITTLE]=37,[MOTOR_WRIST_X]=7,[MOTOR_WRIST_Y]=7,[MOTOR_PALM]=5,
};
/* PWM par moteur (%). PALM tire des câbles → plus de couple pour passer le
 * point dur à mi-course. Réglable à chaud : commande  v<pct>  (ex: v75). */
static uint8_t  cb_speed[MOTOR_COUNT] = {
  [MOTOR_THUMB]=45,[MOTOR_INDEX]=45,[MOTOR_MIDDLE]=45,[MOTOR_RING]=45,
  [MOTOR_LITTLE]=45,[MOTOR_WRIST_X]=45,[MOTOR_WRIST_Y]=45,[MOTOR_PALM]=75,
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

/* Contrôle POIGNET couplé.
 *  mode 0 = idle
 *       1 = impulse ROTATION  (open-loop, durée fixe, PAS de butée — infini)
 *       2 = FLEXION seek       (closed-loop : pousse jusqu'au calage encodeur,
 *                               coupe les 2 → protège les câbles) */
static uint8_t  cb_wmode;
static int8_t   cb_wrx, cb_wry;       /* sens raw L298N X / Y du mouvement */
static uint16_t cb_wticks;            /* impulse : ticks restants */
static int32_t  cb_wrefX, cb_wrefY;   /* flex : derniers comptes (calage) */
static uint16_t cb_wstill;            /* flex : ticks sans progrès */
static int      cb_wimp_ms = 400;     /* durée d'une impulsion rotation (ms) */

/* État poignet : positions X/Y, rotation/flexion déduites, ET niveaux
 * BRUTS des voies encodeur (diag "lit 0" : si Xa/Xb/Ya/Yb ne togglent pas
 * quand ça tourne → voie A ou B pas câblée, comme sur les doigts). */
static void cb_wrist_print(void)
{
  long xd = (long)((int64_t)cb_cnt(MOTOR_WRIST_X) * 3600 / WT_CNT_PER_OUTPUT_REV);
  long yd = (long)((int64_t)cb_cnt(MOTOR_WRIST_Y) * 3600 / WT_CNT_PER_OUTPUT_REV);
  long rot = (xd + yd) / 2;
  long flx = (xd - yd) / 2;
  int xa=(int)((GPIOA->IDR>>0)&1U), xb=(int)((GPIOA->IDR>>1)&1U);  /* WRIST_X TIM2 PA0/PA1 */
  int ya=(int)((GPIOC->IDR>>6)&1U), yb=(int)((GPIOC->IDR>>7)&1U);  /* WRIST_Y TIM8 PC6/PC7 */
  printf("WRIST X=%ld.%ld Y=%ld.%ld | rot=%ld.%ld flex=%ld.%ld | Xa=%d Xb=%d Ya=%d Yb=%d\r\n",
         xd/10,(xd<0?-xd:xd)%10, yd/10,(yd<0?-yd:yd)%10,
         rot/10,(rot<0?-rot:rot)%10, flx/10,(flx<0?-flx:flx)%10,
         xa,xb,ya,yb);
}

/* Poignet au repos = FREIN dynamique (pas roue libre) : tient la position
 * contre le poids de la main (sinon ça retombe en flexion). */
static void cb_wrist_stop(void)
{
  cb_wmode = 0;
  Motor_L298N_Brake(MOTOR_WRIST_X);
  Motor_L298N_Brake(MOTOR_WRIST_Y);
  cb_active[MOTOR_WRIST_X] = cb_active[MOTOR_WRIST_Y] = false;
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
  /* Le POIGNET se pilote en UNITÉ — pas de select, commandes directes. */
  if (BoardLink_IsLocalMotor(MOTOR_WRIST_X) &&
      BoardLink_IsLocalMotor(MOTOR_WRIST_Y)) {
    printf("CAL [W] POIGNET (X+Y, PAS de select) : "
           "r/l=ROT impulse(%dms)  f/F=FLEX seek  w<ms>=duree  W=etat  s=stop\r\n",
           cb_wimp_ms);
  }
}

static void cb_handle_line(char *ln)
{
  while (*ln == ' ') ln++;
  char cmd = *ln;
  int  arg = cb_atoi(ln + 1);

  if (cmd >= '0' && cmd <= '7') {
    int i = cmd - '0';
    if (BoardLink_IsLocalMotor((motor_id_t)i)) { cb_sel = i; printf("CAL sel=%s\r\n", s_wt[i].name); }
    else printf("CAL %d non local\r\n", i);
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
    case 's': cb_wrist_stop(); cb_stop(cb_sel);       break;
    case 'S': cb_wrist_stop(); for (int i=0;i<MOTOR_COUNT;i++) cb_stop(i); printf("CAL ALL STOP\r\n"); break;
    case 'z':
      if (cb_sel>=0){ TIM_HandleTypeDef*h=wt_h(s_wt[cb_sel].tim);
        if(h){__HAL_TIM_SET_COUNTER(h,0);} cb_active[cb_sel]=false; cb_target[cb_sel]=0;
        printf("CAL %s ZERO\r\n", s_wt[cb_sel].name);} break;
    case 'i':
      { int x = (ln[1]>='0'&&ln[1]<='7') ? arg : cb_sel;
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
    /* poignet couplé (WRIST_X=5, WRIST_Y=6) — rotation = même sens, flexion = opposé */
    /* --- POIGNET COUPLÉ (les 2 moteurs ensemble, closed-loop) ---
     * rotation = même sens (X+,Y+) | flexion = sens opposé (X+,Y−).
     * Anti-butée couplé : si un des 2 cale, les 2 sont coupés. */
    /* ROTATION = impulsion open-loop (infinie, pas de butée). */
    case 'r': cb_active[MOTOR_WRIST_X]=cb_active[MOTOR_WRIST_Y]=false;
              cb_wmode=1; cb_wrx=+1; cb_wry=+1;
              cb_wticks=(uint16_t)(cb_wimp_ms/CB_TICK_MS);
              printf("WRIST ROT R impulse %dms\r\n", cb_wimp_ms); break;
    case 'l': cb_active[MOTOR_WRIST_X]=cb_active[MOTOR_WRIST_Y]=false;
              cb_wmode=1; cb_wrx=-1; cb_wry=-1;
              cb_wticks=(uint16_t)(cb_wimp_ms/CB_TICK_MS);
              printf("WRIST ROT L impulse %dms\r\n", cb_wimp_ms); break;
    /* FLEXION = pousse jusqu'au calage encodeur puis coupe les 2 (câbles). */
    case 'f': cb_active[MOTOR_WRIST_X]=cb_active[MOTOR_WRIST_Y]=false;
              cb_wmode=2; cb_wrx=+1; cb_wry=-1;
              cb_wrefX=cb_cnt(MOTOR_WRIST_X); cb_wrefY=cb_cnt(MOTOR_WRIST_Y); cb_wstill=0;
              printf("WRIST FLEX+ (seek butee)\r\n"); break;
    case 'F': cb_active[MOTOR_WRIST_X]=cb_active[MOTOR_WRIST_Y]=false;
              cb_wmode=2; cb_wrx=-1; cb_wry=+1;
              cb_wrefX=cb_cnt(MOTOR_WRIST_X); cb_wrefY=cb_cnt(MOTOR_WRIST_Y); cb_wstill=0;
              printf("WRIST FLEX- (seek butee)\r\n"); break;
    case 'w':  /* durée d'une impulsion rotation en ms (ex: w800) */
      if (arg >= 50 && arg <= 3000) { cb_wimp_ms = arg; printf("WRIST impulse=%dms\r\n", arg); }
      break;
    case 'W': cb_wrist_print(); break;   /* état poignet + niveaux bruts A/B */
    default: break;
  }
}

static void Calib_Task(void *argument)
{
  (void)argument;
  char ln[32]; uint8_t li = 0;

  for (int i = 0; i < MOTOR_COUNT; i++) {
    cb_dir[i] = 1; cb_active[i] = false;
    if (BoardLink_IsLocalMotor((motor_id_t)i)) {
      wt_init_one(&s_wt[i]);
      if (cb_sel < 0) cb_sel = i;
    }
  }
  /* Poignet : frein dès le boot pour qu'il ne retombe pas sous son poids
   * avant la 1ère commande (master uniquement). */
  if (BoardLink_IsLocalMotor(MOTOR_WRIST_X) &&
      BoardLink_IsLocalMotor(MOTOR_WRIST_Y)) {
    Motor_L298N_Brake(MOTOR_WRIST_X);
    Motor_L298N_Brake(MOTOR_WRIST_Y);
  }
  printf("\r\n=== CALIB MODE ===\r\n");
  printf("  ?  liste | 0-7 select | H auto-home sel | A auto-home tous\r\n");
  printf("  o open | c close | x si open/close inverse | d dump | S stop\r\n");
  printf("  v<pct> force/vitesse du moteur (ex: v75 si cale a mi-course)\r\n");
  printf("  POIGNET (X+Y, PAS de select): r/l ROTATION impulse, f/F FLEXION\r\n");
  printf("    w<ms> duree impulse rotation (ex: w800) | W etat+niveaux A/B | s stop\r\n");
  printf("  workflow: select -> H (le moteur cale les 2 butees seul) -> o/c\r\n");
  cb_list();

  for (;;)
  {
    /* --- commandes --- */
    uint8_t b;
    while (cb_pop(&b)) {
      if (b == '\n' || b == '\r') { ln[li] = 0; if (li) cb_handle_line(ln); li = 0; }
      else if (li < sizeof(ln) - 1) ln[li++] = (char)b;
    }

    /* --- POIGNET couplé (rotation impulse / flexion seek-calage) --- */
    if (cb_wmode == 1) {                       /* ROTATION : impulse open-loop */
      Motor_L298N_SetRaw(MOTOR_WRIST_X, cb_wrx, cb_speed[MOTOR_WRIST_X]);
      Motor_L298N_SetRaw(MOTOR_WRIST_Y, cb_wry, cb_speed[MOTOR_WRIST_Y]);
      if (cb_wticks == 0 || --cb_wticks == 0) {
        cb_wrist_stop();
        printf("WRIST rot done\r\n"); cb_wrist_print();
      }
    }
    else if (cb_wmode == 2) {                  /* FLEXION : pousse jusqu'au calage */
      Motor_L298N_SetRaw(MOTOR_WRIST_X, cb_wrx, cb_speed[MOTOR_WRIST_X]);
      Motor_L298N_SetRaw(MOTOR_WRIST_Y, cb_wry, cb_speed[MOTOR_WRIST_Y]);
      int32_t cx = cb_cnt(MOTOR_WRIST_X), cy = cb_cnt(MOTOR_WRIST_Y);
      if (cb_labs(cx - cb_wrefX) >= CB_HOME_EPS ||
          cb_labs(cy - cb_wrefY) >= CB_HOME_EPS) {
        cb_wrefX = cx; cb_wrefY = cy; cb_wstill = 0;
      } else if (++cb_wstill > CB_HOME_STALL) {
        cb_wrist_stop();
        printf("WRIST FLEX butee (calage) -> stop pair\r\n"); cb_wrist_print();
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
              TIM_HandleTypeDef *h = wt_h(s_wt[i].tim);
              if (h) __HAL_TIM_SET_COUNTER(h, 0);
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
        /* Poignet couplé : si un des 2 cale, on coupe AUSSI l'autre
         * (sinon le différentiel force le mécanisme → câbles). */
        if (i == MOTOR_WRIST_X) { cb_stop(MOTOR_WRIST_Y); printf("WRIST pair stop (butee)\r\n"); }
        else if (i == MOTOR_WRIST_Y) { cb_stop(MOTOR_WRIST_X); printf("WRIST pair stop (butee)\r\n"); }
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
  MotorGuard_TaskCreate();   /* SÉCURITÉ anti-butée — toujours active */
#ifdef WRIST_ENCODER_TEST
  WristTest_TaskCreate();
#endif
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
