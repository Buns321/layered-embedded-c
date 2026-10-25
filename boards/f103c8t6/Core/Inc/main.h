/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RGB_G_Pin GPIO_PIN_7
#define RGB_G_GPIO_Port GPIOA
#define RGB_R_Pin GPIO_PIN_0
#define RGB_R_GPIO_Port GPIOB
#define KEY_1_Pin GPIO_PIN_12
#define KEY_1_GPIO_Port GPIOB
#define KEY_1_EXTI_IRQn EXTI15_10_IRQn
#define KEY_2_Pin GPIO_PIN_13
#define KEY_2_GPIO_Port GPIOB
#define KEY_2_EXTI_IRQn EXTI15_10_IRQn
#define TCRT_Pin GPIO_PIN_14
#define TCRT_GPIO_Port GPIOB
#define TCRT_EXTI_IRQn EXTI15_10_IRQn
#define EC11_BTN_Pin GPIO_PIN_15
#define EC11_BTN_GPIO_Port GPIOB
#define EC11_BTN_EXTI_IRQn EXTI15_10_IRQn
#define SONAR_ECHO_Pin GPIO_PIN_10
#define SONAR_ECHO_GPIO_Port GPIOA
#define SONAR_ECHO_EXTI_IRQn EXTI15_10_IRQn
#define SONAR_TRIG_Pin GPIO_PIN_11
#define SONAR_TRIG_GPIO_Port GPIOA
#define WS2812_Pin GPIO_PIN_4
#define WS2812_GPIO_Port GPIOB
#define RELAY_Pin GPIO_PIN_5
#define RELAY_GPIO_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_6
#define I2C1_SCL_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_7
#define I2C1_SDA_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_9
#define BUZZER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
