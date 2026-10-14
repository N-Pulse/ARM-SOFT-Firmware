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
#if (defined(__arm__) || defined(__thumb__)) && (!N_PULSE_OLD_HW || !N_PULSE_TARGET_RET6)
#error "This branch requires the maintained OLD_HW RET6 build; generated legacy flags are unsafe"
#endif
#include "main.h"
#include "old_hw_timing.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fatal_stop.h"
#if N_PULSE_OLD_HW
#include "old_hw.h"
#endif
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "motor_safety.h"
#include "motor_guard.h"
#include "motor_safety_config.h"
#include "comm.h"
#include "board_link.h"
#include "encoder_bsp.h"
#include "hw_telemetry.h"
#include "adc_dma_bsp.h"
#include "motor_estimation.h"

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
/* MOTOR_CALIB_MODE remains disabled until the DRV8848 wiring is confirmed. */
#ifdef MOTOR_CALIB_MODE
#error "Legacy motor calibration is unavailable during disarmed startup bring-up"
#endif
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
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;
ADC_HandleTypeDef hadc4;
ADC_HandleTypeDef hadc5;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;
DMA_HandleTypeDef hdma_adc3;
DMA_HandleTypeDef hdma_adc4;
DMA_HandleTypeDef hdma_adc5;

#if !N_PULSE_OLD_HW
HRTIM_HandleTypeDef hhrtim1;

LPTIM_HandleTypeDef hlptim1;

OPAMP_HandleTypeDef hopamp1;
OPAMP_HandleTypeDef hopamp2;
OPAMP_HandleTypeDef hopamp3;
OPAMP_HandleTypeDef hopamp4;
OPAMP_HandleTypeDef hopamp5;
OPAMP_HandleTypeDef hopamp6;
#endif

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim16;
TIM_HandleTypeDef htim17;
TIM_HandleTypeDef htim20;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
#if !N_PULSE_OLD_HW
static TaskHandle_t sDefaultTaskHandle;
#endif
static TaskHandle_t sControlTaskHandle;
static TaskHandle_t sSafetyTaskHandle;
static TaskHandle_t sTelemetryTaskHandle;
static TimerHandle_t sLedTimer;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
#if !N_PULSE_OLD_HW
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_LPTIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM15_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM17_Init(void);
static void MX_TIM20_Init(void);
static void MX_UART4_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC4_Init(void);
static void MX_HRTIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_OPAMP1_Init(void);
static void MX_OPAMP2_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_OPAMP4_Init(void);
static void MX_OPAMP6_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_ADC5_Init(void);
static void MX_OPAMP5_Init(void);
static void MX_USART3_UART_Init(void);
#endif /* preserved new-board peripheral prototypes */
/* USER CODE BEGIN PFP */
static void ControlTask(void *argument);
static void SafetyTask(void *argument);
static void TelemetryTask(void *argument);
static void CreateTasks(void);
static void StartScheduler(void);
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
  if (HAL_Init() != HAL_OK) Error_Handler();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
#if N_PULSE_OLD_HW
  BoardLink_Init(); /* PC0 before motor peripherals or output pin setup. */
  OldHW_HoldOutputsLow();
  OldHW_ClockConfig();
#else
  SystemClock_Config();
#endif

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
#if N_PULSE_OLD_HW
  OldHW_Init();
#if N_PULSE_TIMING_DIAGNOSTICS
  OldHW_TimingInit();
#endif
#else
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_LPTIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_TIM8_Init();
  MX_TIM15_Init();
  MX_TIM16_Init();
  MX_TIM17_Init();
  MX_TIM20_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_ADC4_Init();
  MX_HRTIM1_Init();
  MX_ADC1_Init();
  MX_OPAMP1_Init();
  MX_OPAMP2_Init();
  MX_OPAMP3_Init();
  MX_OPAMP4_Init();
  MX_OPAMP6_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC5_Init();
  MX_OPAMP5_Init();
  MX_USART3_UART_Init();
#endif
  /* USER CODE BEGIN 2 */
  BoardUART_Compat_Init();
  BoardLink_Init();
  App_Init();
  CreateTasks();
  StartScheduler();
  /* USER CODE END 2 */

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
#if !N_PULSE_OLD_HW
/* New-board generated initialization and legacy diagnostics preserved, inactive. */
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
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 20;
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
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = ENABLE;
  hadc2.Init.NbrOfConversion = 2;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = ENABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP3_ADC2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.GainCompensation = 0;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = ENABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.DMAContinuousRequests = ENABLE;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc3, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief ADC4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC4_Init(void)
{

  /* USER CODE BEGIN ADC4_Init 0 */

  /* USER CODE END ADC4_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC4_Init 1 */

  /* USER CODE END ADC4_Init 1 */

  /** Common config
  */
  hadc4.Instance = ADC4;
  hadc4.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc4.Init.Resolution = ADC_RESOLUTION_12B;
  hadc4.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc4.Init.GainCompensation = 0;
  hadc4.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc4.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc4.Init.LowPowerAutoWait = DISABLE;
  hadc4.Init.ContinuousConvMode = ENABLE;
  hadc4.Init.NbrOfConversion = 3;
  hadc4.Init.DiscontinuousConvMode = DISABLE;
  hadc4.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc4.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc4.Init.DMAContinuousRequests = ENABLE;
  hadc4.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc4.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP6;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC4_Init 2 */

  /* USER CODE END ADC4_Init 2 */

}

/**
  * @brief ADC5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC5_Init(void)
{

  /* USER CODE BEGIN ADC5_Init 0 */

  /* USER CODE END ADC5_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC5_Init 1 */

  /* USER CODE END ADC5_Init 1 */

  /** Common config
  */
  hadc5.Instance = ADC5;
  hadc5.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc5.Init.Resolution = ADC_RESOLUTION_12B;
  hadc5.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc5.Init.GainCompensation = 0;
  hadc5.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc5.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc5.Init.LowPowerAutoWait = DISABLE;
  hadc5.Init.ContinuousConvMode = ENABLE;
  hadc5.Init.NbrOfConversion = 2;
  hadc5.Init.DiscontinuousConvMode = DISABLE;
  hadc5.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc5.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc5.Init.DMAContinuousRequests = ENABLE;
  hadc5.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc5.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP5;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC5_Init 2 */

  /* USER CODE END ADC5_Init 2 */

}

/**
  * @brief HRTIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_HRTIM1_Init(void)
{

  /* USER CODE BEGIN HRTIM1_Init 0 */

  /* USER CODE END HRTIM1_Init 0 */

  HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg = {0};
  HRTIM_TimerCtlTypeDef pTimerCtl = {0};
  HRTIM_TimerCfgTypeDef pTimerCfg = {0};
  HRTIM_CompareCfgTypeDef pCompareCfg = {0};
  HRTIM_OutputCfgTypeDef pOutputCfg = {0};

  /* USER CODE BEGIN HRTIM1_Init 1 */

  /* USER CODE END HRTIM1_Init 1 */
  hhrtim1.Instance = HRTIM1;
  hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  if (HAL_HRTIM_Init(&hhrtim1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_DLLCalibrationStart(&hhrtim1, HRTIM_CALIBRATIONRATE_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_PollForDLLCalibration(&hhrtim1, 10) != HAL_OK)
  {
    Error_Handler();
  }
  pTimeBaseCfg.Period = 0x1F3F;
  pTimeBaseCfg.RepetitionCounter = 0x00;
  pTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_DIV1;
  pTimeBaseCfg.Mode = HRTIM_MODE_CONTINUOUS;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCtl.UpDownMode = HRTIM_TIMERUPDOWNMODE_UP;
  pTimerCtl.TrigHalf = HRTIM_TIMERTRIGHALF_DISABLED;
  pTimerCtl.GreaterCMP1 = HRTIM_TIMERGTCMP1_EQUAL;
  pTimerCtl.DualChannelDacEnable = HRTIM_TIMER_DCDE_DISABLED;
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.InterruptRequests = HRTIM_TIM_IT_NONE;
  pTimerCfg.DMARequests = HRTIM_TIM_DMA_NONE;
  pTimerCfg.DMASrcAddress = 0x0000;
  pTimerCfg.DMADstAddress = 0x0000;
  pTimerCfg.DMASize = 0x1;
  pTimerCfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
  pTimerCfg.InterleavedMode = HRTIM_INTERLEAVED_MODE_DISABLED;
  pTimerCfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
  pTimerCfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
  pTimerCfg.DACSynchro = HRTIM_DACSYNC_NONE;
  pTimerCfg.PreloadEnable = HRTIM_PRELOAD_DISABLED;
  pTimerCfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
  pTimerCfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
  pTimerCfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_DISABLED;
  pTimerCfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  pTimerCfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
  pTimerCfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_DISABLED;
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  pTimerCfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  pTimerCfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_UNCONDITIONAL;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_D_E_DELAYEDPROTECTION_DISABLED;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_F_DELAYEDPROTECTION_DISABLED;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.CompareValue = 3;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.AutoDelayedMode = HRTIM_AUTODELAYEDMODE_REGULAR;
  pCompareCfg.AutoDelayedTimeout = 0x0000;

  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_2, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  pOutputCfg.SetSource = HRTIM_OUTPUTSET_NONE;
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  pOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  pOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  pOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
  pOutputCfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  pOutputCfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_OUTPUT_TC1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_OUTPUT_TD1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_OUTPUT_TE1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_OUTPUT_TF1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_OUTPUT_TC2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_OUTPUT_TD2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_OUTPUT_TE2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_OUTPUT_TF2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_2, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_COMPAREUNIT_2, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_COMPAREUNIT_2, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.CompareValue = 3;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_COMPAREUNIT_2, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.CompareValue = 3;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_F, HRTIM_COMPAREUNIT_2, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN HRTIM1_Init 2 */

  /* USER CODE END HRTIM1_Init 2 */
  HAL_HRTIM_MspPostInit(&hhrtim1);

}

/**
  * @brief LPTIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPTIM1_Init(void)
{

  /* USER CODE BEGIN LPTIM1_Init 0 */

  /* USER CODE END LPTIM1_Init 0 */

  /* USER CODE BEGIN LPTIM1_Init 1 */

  /* USER CODE END LPTIM1_Init 1 */
  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim1.Init.UltraLowPowerClock.Polarity = LPTIM_CLOCKPOLARITY_RISING_FALLING;
  hlptim1.Init.UltraLowPowerClock.SampleTime = LPTIM_CLOCKSAMPLETIME_DIRECTTRANSITION;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_EXTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */

}

/**
  * @brief OPAMP1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP1_Init(void)
{

  /* USER CODE BEGIN OPAMP1_Init 0 */

  /* USER CODE END OPAMP1_Init 0 */

  /* USER CODE BEGIN OPAMP1_Init 1 */

  /* USER CODE END OPAMP1_Init 1 */
  hopamp1.Instance = OPAMP1;
  hopamp1.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp1.Init.Mode = OPAMP_PGA_MODE;
  hopamp1.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO1;
  hopamp1.Init.InternalOutput = ENABLE;
  hopamp1.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp1.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp1.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP1_Init 2 */

  /* USER CODE END OPAMP1_Init 2 */

}

/**
  * @brief OPAMP2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP2_Init(void)
{

  /* USER CODE BEGIN OPAMP2_Init 0 */

  /* USER CODE END OPAMP2_Init 0 */

  /* USER CODE BEGIN OPAMP2_Init 1 */

  /* USER CODE END OPAMP2_Init 1 */
  hopamp2.Instance = OPAMP2;
  hopamp2.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp2.Init.Mode = OPAMP_PGA_MODE;
  hopamp2.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp2.Init.InternalOutput = ENABLE;
  hopamp2.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp2.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp2.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP2_Init 2 */

  /* USER CODE END OPAMP2_Init 2 */

}

/**
  * @brief OPAMP3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP3_Init(void)
{

  /* USER CODE BEGIN OPAMP3_Init 0 */

  /* USER CODE END OPAMP3_Init 0 */

  /* USER CODE BEGIN OPAMP3_Init 1 */

  /* USER CODE END OPAMP3_Init 1 */
  hopamp3.Instance = OPAMP3;
  hopamp3.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp3.Init.Mode = OPAMP_PGA_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp3.Init.InternalOutput = ENABLE;
  hopamp3.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp3.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp3.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp3.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP3_Init 2 */

  /* USER CODE END OPAMP3_Init 2 */

}

/**
  * @brief OPAMP4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP4_Init(void)
{

  /* USER CODE BEGIN OPAMP4_Init 0 */

  /* USER CODE END OPAMP4_Init 0 */

  /* USER CODE BEGIN OPAMP4_Init 1 */

  /* USER CODE END OPAMP4_Init 1 */
  hopamp4.Instance = OPAMP4;
  hopamp4.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp4.Init.Mode = OPAMP_PGA_MODE;
  hopamp4.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO1;
  hopamp4.Init.InternalOutput = ENABLE;
  hopamp4.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp4.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp4.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp4.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP4_Init 2 */

  /* USER CODE END OPAMP4_Init 2 */

}

/**
  * @brief OPAMP5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP5_Init(void)
{

  /* USER CODE BEGIN OPAMP5_Init 0 */

  /* USER CODE END OPAMP5_Init 0 */

  /* USER CODE BEGIN OPAMP5_Init 1 */

  /* USER CODE END OPAMP5_Init 1 */
  hopamp5.Instance = OPAMP5;
  hopamp5.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp5.Init.Mode = OPAMP_PGA_MODE;
  hopamp5.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO2;
  hopamp5.Init.InternalOutput = ENABLE;
  hopamp5.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp5.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp5.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp5.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP5_Init 2 */

  /* USER CODE END OPAMP5_Init 2 */

}

/**
  * @brief OPAMP6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP6_Init(void)
{

  /* USER CODE BEGIN OPAMP6_Init 0 */

  /* USER CODE END OPAMP6_Init 0 */

  /* USER CODE BEGIN OPAMP6_Init 1 */

  /* USER CODE END OPAMP6_Init 1 */
  hopamp6.Instance = OPAMP6;
  hopamp6.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp6.Init.Mode = OPAMP_PGA_MODE;
  hopamp6.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO1;
  hopamp6.Init.InternalOutput = ENABLE;
  hopamp6.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp6.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp6.Init.PgaGain = OPAMP_PGA_GAIN_2_OR_MINUS_1;
  hopamp6.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP6_Init 2 */

  /* USER CODE END OPAMP6_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 65535;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
/* TIM6 is initialized exclusively by HAL_InitTick(), including clock changes. */

/**
  * @brief TIM8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM8_Init(void)
{

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 0;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 65535;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim8, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */

}

/**
  * @brief TIM15 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM15_Init(void)
{

  /* USER CODE BEGIN TIM15_Init 0 */

  /* USER CODE END TIM15_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM15_Init 1 */

  /* USER CODE END TIM15_Init 1 */
  htim15.Instance = TIM15;
  htim15.Init.Prescaler = 0;
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 7999;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM15_Init 2 */

  /* USER CODE END TIM15_Init 2 */
  HAL_TIM_MspPostInit(&htim15);

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 0;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 7999;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */
  HAL_TIM_MspPostInit(&htim16);

}

/**
  * @brief TIM17 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM17_Init(void)
{

  /* USER CODE BEGIN TIM17_Init 0 */

  /* USER CODE END TIM17_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM17_Init 1 */

  /* USER CODE END TIM17_Init 1 */
  htim17.Instance = TIM17;
  htim17.Init.Prescaler = 0;
  htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim17.Init.Period = 7999;
  htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim17.Init.RepetitionCounter = 0;
  htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim17, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim17, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM17_Init 2 */

  /* USER CODE END TIM17_Init 2 */
  HAL_TIM_MspPostInit(&htim17);

}

/**
  * @brief TIM20 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM20_Init(void)
{

  /* USER CODE BEGIN TIM20_Init 0 */

  /* USER CODE END TIM20_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM20_Init 1 */

  /* USER CODE END TIM20_Init 1 */
  htim20.Instance = TIM20;
  htim20.Init.Prescaler = 0;
  htim20.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim20.Init.Period = 65535;
  htim20.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim20.Init.RepetitionCounter = 0;
  htim20.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim20, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim20, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM20_Init 2 */

  /* USER CODE END TIM20_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  /* DMA1_Channel8_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel8_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel8_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BLE_RST_GPIO_Port, BLE_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : NRST_Pin */
  GPIO_InitStruct.Pin = NRST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(NRST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PD10 PD12 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PG0 PG2 PG4 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : PG1 PG3 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : PG8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : BLE_RST_Pin */
  GPIO_InitStruct.Pin = BLE_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BLE_RST_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ---------------------------------------------------------------------------
 * Infra encodeurs — TOUJOURS compilée (test, calibration, ET sécurité
 * anti-butée de la pipeline normale). Autonome (pas fw/bsp).
 *
 * La carte unique initialise les encodeurs exposés par le dernier .ioc.
 * Les timers et GPIO sont configurés par CubeMX ; ce code se contente de
 * démarrer les instances et de lire leurs compteurs.
 *
 * Le dernier .ioc expose sept paires d'entrées encodeur. PALM reste donc
 * explicitement sans entrée jusqu'à ce qu'un câblage et une ressource timer
 * valides soient ajoutés au matériel et au .ioc.
 *
 * Moon DCU10025P12 : 12 PPR ×4 = 48 cnt/tour moteur, gearbox 61:1
 * → 2928 cnt/tour arbre sortie.
 * --------------------------------------------------------------------------- */

/* prototypes directs (Core/Src/subdir.mk n'inclut pas fw/motors/motor_l298n) */
void Motor_DRV8848_SetSignedRaw(motor_id_t id, int dir, uint8_t speed_pct);
void Motor_DRV8848_Stop(motor_id_t id);
int  Motor_DRV8848_DrivingDir(motor_id_t id);
void Motor_DRV8848_Brake(motor_id_t id);

#ifdef MOTOR_CALIB_MODE
/* Legacy calibration is excluded from this lot. Its historical mapping and
 * conversion constants must be reviewed before this mode can be restored. */
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
  [MOTOR_THUMB]   = { "THUMB",   TIM1,  false, GPIOC, GPIO_PIN_0,  1, GPIOC, GPIO_PIN_1,  1 },
  [MOTOR_INDEX]   = { "INDEX",   TIM2,  true,  GPIOD, GPIO_PIN_3,  2, GPIOD, GPIO_PIN_4,  2 },
  [MOTOR_MIDDLE]  = { "MIDDLE",  TIM3,  false, GPIOA, GPIO_PIN_6,  2, GPIOA, GPIO_PIN_4,  2 },
  [MOTOR_RING]    = { "RING",    TIM4,  false, GPIOB, GPIO_PIN_6,  2, GPIOB, GPIO_PIN_7,  2 },
  [MOTOR_LITTLE]  = { "LITTLE",  TIM5,  true,  GPIOA, GPIO_PIN_0,  2, GPIOA, GPIO_PIN_1,  2 },
  [MOTOR_WRIST_X] = { "WRIST_X", TIM8,  false, GPIOA, GPIO_PIN_15, 3, GPIOB, GPIO_PIN_8,  3 },
  [MOTOR_WRIST_Y] = { "WRIST_Y", TIM20, false, GPIOE, GPIO_PIN_2,  2, GPIOE, GPIO_PIN_3,  2 },
  [MOTOR_PALM]    = { "PALM",    NULL,   false, NULL, 0, 0, NULL, 0, 0 },
};

extern TIM_HandleTypeDef htim1, htim2, htim3, htim4, htim5, htim8, htim20;

static TIM_HandleTypeDef *wt_h(TIM_TypeDef *t)
{
  if (t == TIM1)  return &htim1;
  if (t == TIM2)  return &htim2;
  if (t == TIM3)  return &htim3;
  if (t == TIM4)  return &htim4;
  if (t == TIM5)  return &htim5;
  if (t == TIM8)  return &htim8;
  if (t == TIM20) return &htim20;
  return NULL;
}

static void wt_init_one(const wt_enc_t *e)
{
  TIM_HandleTypeDef *h = wt_h(e->tim);
  if (h == NULL || e->tim == NULL) return;
  if (HAL_TIM_Encoder_Start(h, TIM_CHANNEL_ALL) != HAL_OK) {
    Error_Handler();
    return;
  }
  __HAL_TIM_SET_COUNTER(h, 0);
}

#endif /* MOTOR_CALIB_MODE: legacy helpers are not compiled */

#endif /* preserved new-board peripheral initialization */

#ifndef MOTOR_CALIB_MODE
/* ===========================================================================
 *  SÉCURITÉ ANTI-BUTÉE — TOUJOURS ACTIVE (pipeline normale + tests).
 *
 *  Surveille chaque moteur local : s'il est alimenté (DRV8848 drive != 0) mais
 *  que son encodeur n'avance plus pendant GUARD_STALL_MS → il force contre une
 *  butée → on COUPE immédiatement (Motor_DRV8848_Stop). Indépendant de la
 *  commande : protège quel que soit le script (send_action.py, wrist_test.py…)
 *  ou la pipeline proto. Empêche la sur-course et la casse des câbles.
 *
 *  (En mode calibration ce garde est désactivé : le homing/seek POUSSE
 *   volontairement dans la butée pour la détecter.)
 * ======================================================================== */

static void MotorGuard_Task(void *argument)
{
  (void)argument;
  for (;;)
  {
    MotorGuard_Poll();
    Motor_ServiceStop();
    /* Raw UART beacons removed; causes/masks are debugger diagnostics. */
    vTaskDelay(pdMS_TO_TICKS(MOTOR_GUARD_PERIOD_MS));
  }
}

static void MotorGuard_TaskCreate(void)
{
  BaseType_t status = xTaskCreate(MotorGuard_Task, "guard", 384, NULL,
                                  tskIDLE_PRIORITY + 3, NULL);
  configASSERT(status == pdPASS);
}
#endif /* !MOTOR_CALIB_MODE */

#ifdef WRIST_ENCODER_TEST
static void WristTest_Task(void *argument)
{
  (void)argument;

  for (;;)
  {
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
      if (!Encoder_BSP_IsLocal((motor_id_t)i)) continue;
      int64_t cnt = Encoder_BSP_GetRawCount((motor_id_t)i);
      /* Raw counts only: no unapproved mechanical conversion. */
      printf("ENC id=%d cnt=%lld\r\n", i, (long long)cnt);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

static void WristTest_TaskCreate(void)
{
  BaseType_t status = xTaskCreate(WristTest_Task, "enctest", 384, NULL,
                                  tskIDLE_PRIORITY + 1, NULL);
  configASSERT(status == pdPASS);
}
#endif /* WRIST_ENCODER_TEST */

#if !N_PULSE_OLD_HW
#ifdef MOTOR_CALIB_MODE
/* ---------------------------------------------------------------------------
 * Calibration closed-loop par moteur (bang-bang + deadband).
 * Lit des commandes ASCII sur le VCP (g_rxring rempli par LPUART1_IRQHandler ;
 * en mode calib on NE démarre PAS Comms_Task, donc on possède le ring).
 * Pilote le DRV8848 via Motor_DRV8848_SetSignedRaw + feedback encodeur.
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
static int8_t   cb_open_raw[MOTOR_COUNT];  /* sens raw DRV8848 pour aller à la butée OPEN  */
static int8_t   cb_close_raw[MOTOR_COUNT]; /* sens raw DRV8848 pour aller à la butée CLOSE */
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
static int8_t   cb_wrx, cb_wry;       /* sens raw DRV8848 X / Y du mouvement */
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
  Motor_DRV8848_Brake(MOTOR_WRIST_X);
  Motor_DRV8848_Brake(MOTOR_WRIST_Y);
  cb_active[MOTOR_WRIST_X] = cb_active[MOTOR_WRIST_Y] = false;
}

static void cb_stop(int i)
{
  if (i < 0 || i >= MOTOR_COUNT) return;
  cb_active[i]  = false;
  cb_hphase[i]  = 0;
  Motor_DRV8848_SetSignedRaw((motor_id_t)i, 0, 0);
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
    Motor_DRV8848_Brake(MOTOR_WRIST_X);
    Motor_DRV8848_Brake(MOTOR_WRIST_Y);
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
      Motor_DRV8848_SetSignedRaw(MOTOR_WRIST_X, cb_wrx, cb_speed[MOTOR_WRIST_X]);
      Motor_DRV8848_SetSignedRaw(MOTOR_WRIST_Y, cb_wry, cb_speed[MOTOR_WRIST_Y]);
      if (cb_wticks == 0 || --cb_wticks == 0) {
        cb_wrist_stop();
        printf("WRIST rot done\r\n"); cb_wrist_print();
      }
    }
    else if (cb_wmode == 2) {                  /* FLEXION : pousse jusqu'au calage */
      Motor_DRV8848_SetSignedRaw(MOTOR_WRIST_X, cb_wrx, cb_speed[MOTOR_WRIST_X]);
      Motor_DRV8848_SetSignedRaw(MOTOR_WRIST_Y, cb_wry, cb_speed[MOTOR_WRIST_Y]);
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
        Motor_DRV8848_SetSignedRaw((motor_id_t)i, raw, cb_speed[i]);
        int32_t hc = cb_cnt(i);
        cb_ht[i]++;
        if (cb_labs(hc - cb_href[i]) >= CB_HOME_EPS) { cb_href[i] = hc; cb_hstill[i] = 0; }
        else cb_hstill[i]++;

        if (cb_hstill[i] > CB_HOME_STALL || cb_ht[i] > CB_HOME_TIMEOUT) {
          Motor_DRV8848_SetSignedRaw((motor_id_t)i, 0, 0);
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
      Motor_DRV8848_SetSignedRaw((motor_id_t)i, drive, cb_speed[i]);

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

#endif /* preserved new-board initialization/legacy test code */

static void CreateTasks(void)
{
  BaseType_t status = pdPASS;

  status = xTaskCreate(ControlTask, "ctrl", 256, NULL, tskIDLE_PRIORITY + 3, &sControlTaskHandle);
  configASSERT(status == pdPASS);

  /* Must preempt ctrl/guard/simtx (3) and comms (4) to check their delays. */
  status = xTaskCreate(SafetyTask, "safe", 256, NULL, tskIDLE_PRIORITY + 5, &sSafetyTaskHandle);
  configASSERT(status == pdPASS);

  status = xTaskCreate(TelemetryTask, "tele", 768, NULL, tskIDLE_PRIORITY + 1, &sTelemetryTaskHandle);
  configASSERT(status == pdPASS);

#ifdef MOTOR_CALIB_MODE
  /* Mode calibration : la tâche calib possède le VCP (g_rxring),
   * on ne démarre PAS la pile proto. */
  Calib_TaskCreate();
#else
#if N_PULSE_OLD_HW
  if (OldHW_HostEnabled()) Comms_TaskCreate();
#else
  Comms_TaskCreate();
#endif
  MotorGuard_TaskCreate();   /* SÉCURITÉ anti-butée — toujours active */
#if N_PULSE_DUAL_BOARD_LINK && !defined(MOTOR_BACKEND_SIM)
  BoardLink_TaskCreate();
#endif
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
  /* A return indicates that the scheduler could not start. */
  Error_Handler();
}

static void ControlTask(void *argument)
{
  (void)argument;
  const TickType_t period = pdMS_TO_TICKS(10); /* 100 Hz loop */
  TickType_t       last_wake = xTaskGetTickCount();

  while (1)
  {
    App_Task();
#if N_PULSE_TIMING_DIAGNOSTICS
    uint32_t timing=OldHW_TimingBegin(OLD_TIME_CONTROL);
#endif
    Motor_Update(); /* Real elapsed HAL time; excessive delays latch a stop. */
#if N_PULSE_TIMING_DIAGNOSTICS
    OldHW_TimingEnd(OLD_TIME_CONTROL,timing);
#endif
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
#if N_PULSE_TIMING_DIAGNOSTICS
    uint32_t timing=OldHW_TimingBegin(OLD_TIME_SAFETY);
#endif
    MotorSafety_Poll();
    Motor_ServiceStop();
#if N_PULSE_TIMING_DIAGNOSTICS
    OldHW_TimingEnd(OLD_TIME_SAFETY,timing);
#endif
    vTaskDelayUntil(&last_wake, period);
  }
}

static void TelemetryTask(void *argument)
{
  (void)argument;
  const TickType_t period = pdMS_TO_TICKS(10);
  uint32_t last_telemetry = HAL_GetTick();

  while (1)
  {
#ifndef MOTOR_BACKEND_SIM
    ADC_DMA_BSP_Service();
    if ((uint32_t)(HAL_GetTick()-last_telemetry) >= N_PULSE_TELEMETRY_PERIOD_MS) {
      last_telemetry = HAL_GetTick();
      HWTelemetry_Service();
    }
#else
    (void)last_telemetry;
#endif
    MotorEstimation_Service();
    vTaskDelay(period);
  }
}

static void LedTimerCallback(TimerHandle_t timer)
{
  (void)timer;
#if N_PULSE_OLD_HW
  OldHW_Heartbeat();
#else
  HAL_GPIO_TogglePin(USART3_TX_LED_GPIO_Port, USART3_TX_LED_Pin);
#endif
}

void vApplicationMallocFailedHook(void)
{
  N_PULSE_FatalHalt(N_PULSE_FATAL_MALLOC);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  N_PULSE_FatalHalt(N_PULSE_FATAL_STACK);
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

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  N_PULSE_FatalHalt(N_PULSE_FATAL_ERROR);
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
  (void)file; (void)line;
  N_PULSE_FatalHalt(N_PULSE_FATAL_HAL_ASSERT);
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
