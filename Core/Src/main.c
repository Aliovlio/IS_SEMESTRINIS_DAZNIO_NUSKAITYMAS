/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "sh1106_driver.h"
#include "testing.h"
#include <stdio.h>
#include <string.h>
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
DMA_HandleTypeDef hdma_adc1;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint16_t adc_buf[ADC_BUF_SIZE];
float measured_freq = 0;
float vpp = 0;
float vmin = 0;
float vmax = 0;
float sample_rate = 1400000.0f;
volatile uint8_t dma_ready = 0;
volatile uint8_t display_ready = 0;
volatile uint8_t timer_done = 0;
uint32_t test_start_tick = 0;
uint8_t test_freq_idx = 0;
uint8_t test_mode = 0;
uint8_t start_test=0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//1 sekundes intevalui praejus iskeliama ekrano atnaujinimo flag
    if(htim->Instance == TIM3)
    {
        display_ready = 1;
        timer_done = 1;
    }
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	//Paspaudus mygtuką inicializuojamas testavimo rezimas
    if(GPIO_Pin == B1_Pin)
        test_mode = 1;
}
//ADC iskvietimas
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	//Iskelia flag kai DMA pasiruoses
    dma_ready = 1;
}
//Funkcija naudojama ASK keitiklio nuskaitymo dazniui pakeisti
void SetAdcSpeed(uint32_t sample_time, float rate)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    HAL_ADC_Stop_DMA(&hadc1);
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = sample_time;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    sample_rate = rate;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, ADC_BUF_SIZE);
}
void SignaloApdorojimas(void)
{
	//-------1 Dalis Itampos apdorojimas
    //Surandame maziausia ir didziausia matuojamas itampos reiksmes
    uint16_t abs_min = 4095;
    uint16_t abs_max = 0;
    for(int i = 0; i < ADC_BUF_SIZE; i++)
    {
		//Jei masyve egzistuoja mazesne/didesne verte ji prilyginama absoliuc vertei
        uint16_t s = adc_buf[i];
        if(s < abs_min) abs_min = s;
        if(s > abs_max) abs_max = s;
    }
	//Kadangi paduodamas staciakampis signalas, skirtingais dazniais
	//gali atsirasti triuksmu, todel randame vertes, kurios 20%
	//mazesnes/didesnes nei pati maziausia/didziausia verte
	//Taip islyginsime triuksmu itaka itampos matavime ir atsikratysime netinkamu
	//itampos suoliu ar kritimu  itakos
	uint32_t sum_zem_l  = 0;
    uint32_t sum_aukst_l = 0;
    uint32_t count_zem  = 0;
    uint32_t count_aukst = 0;
    uint16_t diapazonas     = abs_max - abs_min;
    uint16_t zem_auksc  = abs_min + diapazonas * 0.2;
    uint16_t aukst_zem = abs_max - diapazonas * 0.2;
    for(int i = 0; i < ADC_BUF_SIZE; i++)
    {
		//Jei verte mazesne/didesne sumuojama ir renkamas kiekis, kad veliau butu
		//galima gauti vidutine reiksme vmin ir vmax nustatymui
        uint16_t s = adc_buf[i];
        if(s <= zem_auksc){sum_zem_l  += s; count_zem++;}
        if(s >= aukst_zem){sum_aukst_l += s; count_aukst++;}
    }
	//Apskaiciuojama vidutine min max verte desimtainiu skaiciumi
    uint16_t min, max;
    if(count_zem  > 0){min = (uint16_t)(sum_zem_l  / count_zem);}
    else{min = abs_min;}
    if(count_aukst > 0){max = (uint16_t)(sum_aukst_l / count_aukst);}
    else{max = abs_max;}
    //Itampos skaiciavimas atvaizdavimui ekrane
    vmin = min * 3.3f / 4095.0f;
    vmax = max * 3.3f / 4095.0f;
	vpp = vmax - vmin;
	//Jei gaunamas per mazas signalas, nuresetinam, ADC perjungiam i letesni
	//rezima kad sumazintume triuksmo itaka, laukiam DMA persikrovimo pries
	//tesiant
    if(vpp < 0.03f)
    {
        measured_freq = 0;
        vpp = 0;
        dma_ready = 0;
        SetAdcSpeed(ADC_SAMPLETIME_480CYCLES, 21000000.0f / 492.0f);
        while(!dma_ready);
        return;
    }
	//-------2 Dalis Daznio nustatymas
    //Parenka zingsni, kuris nustato kaip daznai stebeti signala pagal jo dazni
	//Jei dazni zinom kaip zingsni naudojam 40% periodo
	//Jei daznio nezinom zingsnis = 4
	float fs = sample_rate;
	uint32_t zingsnis = (measured_freq > 1.0f)
                   ? (uint32_t)(fs / measured_freq * 0.4f)
                   : 4;
    if(zingsnis < 3)    zingsnis = 3;
    if(zingsnis > 5000) zingsnis = 5000;
	//Kai itampos reiksme yra didesne nei puse itampos vertes (rising front),
	//priskaiciuojamas kaip perejimas. 2 perejimai yra 1 periodas.
	//Perejimu vertes gauname is ADC indeksu pagal kuriuos galime rasti
	//laika tarp perejimu ir is jo rasti signalo dazni
	uint32_t perejimas[64];
    uint32_t count_perej = 0;
    uint8_t  state      = 0;
    uint32_t last_perej = 0;
	uint16_t mid = (min + max) / 2;
    for(int i = 1; i < ADC_BUF_SIZE; i++)
    {
		//Rising front
        if(state == 0 && adc_buf[i] > mid)
        {
            if((i - last_perej) > zingsnis)
            {
                state = 1;
                last_perej = i;
                if(count_perej < 64)
                    perejimas[count_perej++] = i;
            }
        }
		//Falling front
        if(state == 1 && adc_buf[i] < mid)
            state = 0;
    }
	//Jei perejimu skaicius yra didesnis nei 4 apskaiciuojame laiko tarpa
	//tarp paskutinio ir pirmo perejimo. Daznis apskaiciuojamas zinant
	//ADC atskaitu emimo dazni, dauginant ji is gauto periodu kiekio,
	//dalinant is imciu kiekio.
    if(count_perej >= 4)
    {
        uint32_t t1 = perejimas[0];
        uint32_t t2 = perejimas[count_perej - 1];
        float periods       = (float)(count_perej - 1);
        float total_samples = (float)(t2 - t1);
        if(total_samples > 0.0f)
        {
            float freq_now = fs * periods / total_samples;
            if(measured_freq < 1.0f){measured_freq = freq_now;}
			//EMA (exponential moving average) filtras dazniu sokinejimo mazinimui,
			//60% senos reiksmes, 40% naujos reiksmes
            else{measured_freq = 0.6f * measured_freq + 0.4f * freq_now;}
        }
    }
    dma_ready = 0;
	//Atliekame ADC perjungima i rezimus, kurie palengvins daznio nuskaityma
    float range_freq = (measured_freq < 1.0f) ? 0.0f : measured_freq;
    if(range_freq < 600.0f)
        SetAdcSpeed(ADC_SAMPLETIME_480CYCLES, 21000000.0f / 492.0f);
    else if(range_freq < 15000.0f)
        SetAdcSpeed(ADC_SAMPLETIME_28CYCLES,  21000000.0f / 40.0f);
    else
        SetAdcSpeed(ADC_SAMPLETIME_3CYCLES,   1400000.0f);
    while(!dma_ready);
}
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_ADC_Start_DMA(&hadc1,
                    (uint32_t*)adc_buf,
                    ADC_BUF_SIZE);
  HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_RESET);
  HAL_Delay(50);

  HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_SET);
  HAL_Delay(50);


  SH1106_Init();

  dma_ready = 0;
  SetAdcSpeed(ADC_SAMPLETIME_480CYCLES, 21000000.0f / 492.0f); /* ~42.7 kHz */
  while(!dma_ready);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  if(test_mode)
	  {
		  test_mode = 0;
		  RunFullTest();
		  test_freq_idx++;
		  if(test_freq_idx>=4)
		  {
			  test_freq_idx = 0;
		  }
	  }
	  SignaloApdorojimas();
	  if(display_ready)
	  {
		  display_ready = 0;
		  char line1[32];
		  char line2[32];
		  sprintf(line1, "Vpp: %.3f V", vpp);
		  sprintf(line2, "Freq: %.1f Hz", measured_freq);

		  SH1106_Fill(SH1106_COLOR_BLACK);
		  SH1106_GotoXY(12, 10);
		  SH1106_Puts(line1, &Font_7x10, SH1106_COLOR_WHITE);
		  SH1106_GotoXY(12, 30);
		  SH1106_Puts(line2, &Font_7x10, SH1106_COLOR_WHITE);
		  SH1106_UpdateScreen();

		  HAL_UART_Transmit(&huart2, (uint8_t*)line1, strlen(line1), 100);
		  HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100);
		  HAL_UART_Transmit(&huart2, (uint8_t*)line2, strlen(line2), 100);
		  HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n\r\n", 4, 100);

	  }
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 8399;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 9999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OLED_RES_Pin */
  GPIO_InitStruct.Pin = OLED_RES_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OLED_RES_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
