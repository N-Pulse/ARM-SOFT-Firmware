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
 *  BANC DE TEST POIGNET + ENCODEUR
 *  Commenter la ligne suivante = build normal.
 *  Décommentée = stream la position des 2 encodeurs du poignet
 *  (WRIST_X=TIM2, WRIST_Y=TIM8) sur le VCP USB, en // de la
 *  pipeline normale (proto → intent → L298N).
 * ============================================================ */
#define WRIST_ENCODER_TEST
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

#ifdef WRIST_ENCODER_TEST
/* ---------------------------------------------------------------------------
 * Banc de test poignet — autonome (pas de dépendance à fw/bsp/encoder).
 * Lit directement TIM2 (WRIST_X, PA0/PA1, AF1) et TIM8 (WRIST_Y, PC6/PC7, AF4)
 * en mode encodeur quadrature TI12. Stream sur le VCP USB via printf (COM1).
 *
 * Datasheet Moon DCU10025P12 : 12 PPR ×4 quadrature = 48 cnt/tour moteur,
 * réducteur 61:1 → 2928 cnt/tour arbre sortie.
 * --------------------------------------------------------------------------- */
#define WT_CNT_PER_OUTPUT_REV  2928L

static TIM_HandleTypeDef s_wt_htim2;   /* WRIST_X (32-bit) */
static TIM_HandleTypeDef s_wt_htim8;   /* WRIST_Y (16-bit) */

static void wt_encoder_init(TIM_HandleTypeDef *htim, TIM_TypeDef *inst,
                            uint32_t period)
{
  TIM_Encoder_InitTypeDef enc = {0};
  htim->Instance           = inst;
  htim->Init.Prescaler     = 0;
  htim->Init.CounterMode   = TIM_COUNTERMODE_UP;
  htim->Init.Period        = period;
  htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  enc.EncoderMode  = TIM_ENCODERMODE_TI12;
  enc.IC1Polarity  = TIM_ICPOLARITY_RISING;
  enc.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  enc.IC1Prescaler = TIM_ICPSC_DIV1;
  enc.IC1Filter    = 4;
  enc.IC2Polarity  = TIM_ICPOLARITY_RISING;
  enc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  enc.IC2Prescaler = TIM_ICPSC_DIV1;
  enc.IC2Filter    = 4;
  HAL_TIM_Encoder_Init(htim, &enc);
  HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COUNTER(htim, 0);
}

static void WristTest_Task(void *argument)
{
  (void)argument;

  /* GPIO : PA0/PA1 → TIM2 AF1 (WRIST_X) ; PC6/PC7 → TIM8 AF4 (WRIST_Y) */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM8_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Mode  = GPIO_MODE_AF_PP;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;

  g.Pin = GPIO_PIN_0 | GPIO_PIN_1;  g.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &g);
  g.Pin = GPIO_PIN_6 | GPIO_PIN_7;  g.Alternate = GPIO_AF4_TIM8;
  HAL_GPIO_Init(GPIOC, &g);

  wt_encoder_init(&s_wt_htim2, TIM2, 0xFFFFFFFFUL);  /* 32-bit */
  wt_encoder_init(&s_wt_htim8, TIM8, 0xFFFFUL);      /* 16-bit */

  for (;;)
  {
    int32_t cx = (int32_t)__HAL_TIM_GET_COUNTER(&s_wt_htim2);
    int32_t cy = (int32_t)(int16_t)__HAL_TIM_GET_COUNTER(&s_wt_htim8);

    long xmdeg = (long)((int64_t)cx * 360000 / WT_CNT_PER_OUTPUT_REV);
    long ymdeg = (long)((int64_t)cy * 360000 / WT_CNT_PER_OUTPUT_REV);

    /* Niveau LOGIQUE BRUT des 4 voies (lisible même en mode timer : l'IDR
     * reflète toujours l'état réel du pin). Tourne l'arbre encodeur LENTEMENT
     * à la main et regarde si Xa/Xb/Ya/Yb togglent 0↔1.
     *   - togglent → câblage OK, problème = couplage mécanique au moteur
     *   - figés    → encodeur non alimenté / pas câblé / mauvais pin */
    int xa = (int)((GPIOA->IDR >> 0) & 1U);  /* PA0 = WRIST_X A */
    int xb = (int)((GPIOA->IDR >> 1) & 1U);  /* PA1 = WRIST_X B */
    int ya = (int)((GPIOC->IDR >> 6) & 1U);  /* PC6 = WRIST_Y A */
    int yb = (int)((GPIOC->IDR >> 7) & 1U);  /* PC7 = WRIST_Y B */

    printf("ENC WRIST local=%d X_cnt=%ld X_mdeg=%ld Y_cnt=%ld Y_mdeg=%ld "
           "| Xa=%d Xb=%d Ya=%d Yb=%d\r\n",
           BoardLink_IsLocalMotor(MOTOR_WRIST_X) ? 1 : 0,
           (long)cx, xmdeg, (long)cy, ymdeg,
           xa, xb, ya, yb);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void WristTest_TaskCreate(void)
{
  (void)xTaskCreate(WristTest_Task, "wtest", 256, NULL,
                    tskIDLE_PRIORITY + 1, NULL);
}
#endif /* WRIST_ENCODER_TEST */

static void CreateTasks(void)
{
  BaseType_t status = pdPASS;

  status = xTaskCreate(ControlTask, "ctrl", 256, NULL, tskIDLE_PRIORITY + 3, &sControlTaskHandle);
  configASSERT(status == pdPASS);

  status = xTaskCreate(SafetyTask, "safe", 256, NULL, tskIDLE_PRIORITY + 2, &sSafetyTaskHandle);
  configASSERT(status == pdPASS);

  status = xTaskCreate(TelemetryTask, "tele", 256, NULL, tskIDLE_PRIORITY + 1, &sTelemetryTaskHandle);
  configASSERT(status == pdPASS);

  Comms_TaskCreate();

#ifdef WRIST_ENCODER_TEST
  WristTest_TaskCreate();
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
