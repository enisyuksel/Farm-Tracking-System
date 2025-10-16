/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021 STMicroelectronics.
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
#include "usart.h"
#include "app_subghz_phy.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "common.h"
#include "EMPA_MqttAws.h"
#include "myESP32AT.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "subghz_phy_app.h"
#include "sys_app.h"
#include "ota_manager.h"  // OTA Manager
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  MX_SubGHz_Phy_Init();
  MX_LPUART1_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  // Initialize OTA Manager
  OTA_Init();
  
  // MQTT Task Registration
  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_MQTT_Process), UTIL_SEQ_RFU, MY_MqttAwsProcess);

  // Initial MQTT task trigger - sadece bir kez
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  static uint8_t lora_started = 0;  // Flag to start LoRa only once
  static uint32_t mqtt_periodic_trigger = 0;  // Periodic MQTT task trigger

	while (1) {
		// Check if MQTT is connected and LoRa is not started yet
		if (flag_mqtt_connected == SET && lora_started == 0) {
			printf("*** MQTT Connected - Starting LoRa Gateway ***\n\r");

			// Send MQTT connection test message
			extern char mqttPacketBuffer[];
			extern MQTT_Config mqttConfig;
			char test_message[] = "{\"type\":\"gateway_status\",\"status\":\"connected\",\"gw\":\"GW-001\",\"timestamp\":0,\"message\":\"Gateway MQTT connection successful\"}";
			printf("*** Sending MQTT test message ***\n\r");
			if (Wifi_MqttPubRaw2(mqttPacketBuffer, mqttConfig.pubtopic, strlen(test_message), test_message, QOS_0, RTN_0, POLLING_MODE) == FUNC_OK) {
				printf("*** MQTT test message sent successfully ***\n\r");
			} else {
				printf("*** MQTT test message failed ***\n\r");
			}

			StartLoRaGateway();
			lora_started = 1;
			printf("*** LoRa Gateway Started Successfully ***\n\r");
		}

		// Only process LoRa if it's started
		if (lora_started) {
			MX_SubGHz_Phy_Process();
		}

		// Periodic MQTT task trigger for receiving config messages (every ~5000 main loop cycles)
		mqtt_periodic_trigger++;
		if (mqtt_periodic_trigger > 5000) {
			mqtt_periodic_trigger = 0;
			UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_MQTT_Process), CFG_SEQ_Prio_0);
		}

		// UTIL_SEQ işlemci - task'lar işlenir
		UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void Custom_Delay_ms(uint32_t delay_ms) {

	uint32_t cycles_per_ms = 4362;

	for (uint32_t ms = 0; ms < delay_ms; ms++) {
		for (volatile uint32_t i = 0; i < cycles_per_ms; i++) {
			__NOP();
		}
	}
}

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
	while (1) {
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
  while (1)
  {
  }
  /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */
