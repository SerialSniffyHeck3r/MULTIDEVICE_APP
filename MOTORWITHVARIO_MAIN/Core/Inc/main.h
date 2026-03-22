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
#include "stm32f4xx_hal.h"

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
#define BACKLIGHT_Pin GPIO_PIN_2
#define BACKLIGHT_GPIO_Port GPIOE
#define BEEP_SOUND_Pin GPIO_PIN_5
#define BEEP_SOUND_GPIO_Port GPIOE
#define LED11_Pin GPIO_PIN_6
#define LED11_GPIO_Port GPIOE
#define BOARD_DEBUG_BUTTON_Pin GPIO_PIN_0
#define BOARD_DEBUG_BUTTON_GPIO_Port GPIOA
#define BOARD_DEBUG_BUTTON_EXTI_IRQn EXTI0_IRQn
#define BOARD_LED_Pin GPIO_PIN_1
#define BOARD_LED_GPIO_Port GPIOA
#define SD_DETECT_Pin GPIO_PIN_2
#define SD_DETECT_GPIO_Port GPIOA
#define LIGHT_SENSOR_Pin GPIO_PIN_3
#define LIGHT_SENSOR_GPIO_Port GPIOA
#define AUDIO_OUT_Pin GPIO_PIN_4
#define AUDIO_OUT_GPIO_Port GPIOA
#define LED2_Pin GPIO_PIN_6
#define LED2_GPIO_Port GPIOA
#define LED3_Pin GPIO_PIN_7
#define LED3_GPIO_Port GPIOA
#define LED4_Pin GPIO_PIN_0
#define LED4_GPIO_Port GPIOB
#define LCD_BACKLIGHT_Pin GPIO_PIN_1
#define LCD_BACKLIGHT_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_9
#define LED1_GPIO_Port GPIOE
#define BUTTON1_Pin GPIO_PIN_10
#define BUTTON1_GPIO_Port GPIOE
#define BUTTON1_EXTI_IRQn EXTI15_10_IRQn
#define BUTTON2_Pin GPIO_PIN_11
#define BUTTON2_GPIO_Port GPIOE
#define BUTTON2_EXTI_IRQn EXTI15_10_IRQn
#define BUTTON3_Pin GPIO_PIN_12
#define BUTTON3_GPIO_Port GPIOE
#define BUTTON3_EXTI_IRQn EXTI15_10_IRQn
#define BUTTON4_Pin GPIO_PIN_13
#define BUTTON4_GPIO_Port GPIOE
#define BUTTON4_EXTI_IRQn EXTI15_10_IRQn
#define BUTTON5_Pin GPIO_PIN_14
#define BUTTON5_GPIO_Port GPIOE
#define BUTTON5_EXTI_IRQn EXTI15_10_IRQn
#define BUTTON6_Pin GPIO_PIN_15
#define BUTTON6_GPIO_Port GPIOE
#define BUTTON6_EXTI_IRQn EXTI15_10_IRQn
#define LCD_CS_Pin GPIO_PIN_12
#define LCD_CS_GPIO_Port GPIOB
#define LCD_SCK_Pin GPIO_PIN_13
#define LCD_SCK_GPIO_Port GPIOB
#define LCD_MISO_UNUSED_Pin GPIO_PIN_14
#define LCD_MISO_UNUSED_GPIO_Port GPIOB
#define LCD_SDA_Pin GPIO_PIN_15
#define LCD_SDA_GPIO_Port GPIOB
#define BT_TX_PIN_Pin GPIO_PIN_8
#define BT_TX_PIN_GPIO_Port GPIOD
#define BT_RX_PIN_Pin GPIO_PIN_9
#define BT_RX_PIN_GPIO_Port GPIOD
#define LED6_Pin GPIO_PIN_12
#define LED6_GPIO_Port GPIOD
#define LED7_Pin GPIO_PIN_13
#define LED7_GPIO_Port GPIOD
#define LED8_Pin GPIO_PIN_14
#define LED8_GPIO_Port GPIOD
#define LED9_Pin GPIO_PIN_15
#define LED9_GPIO_Port GPIOD
#define LED10_Pin GPIO_PIN_6
#define LED10_GPIO_Port GPIOC
#define LED5_Pin GPIO_PIN_9
#define LED5_GPIO_Port GPIOC
#define DEBUG_TX_Pin GPIO_PIN_9
#define DEBUG_TX_GPIO_Port GPIOA
#define DEBUG_RX_Pin GPIO_PIN_10
#define DEBUG_RX_GPIO_Port GPIOA
#define WINBOND_CS_Pin GPIO_PIN_15
#define WINBOND_CS_GPIO_Port GPIOA
#define GPS_TX_PIN_Pin GPIO_PIN_5
#define GPS_TX_PIN_GPIO_Port GPIOD
#define GPS_RX_PIN_Pin GPIO_PIN_6
#define GPS_RX_PIN_GPIO_Port GPIOD
#define WINBOND_CLK_Pin GPIO_PIN_3
#define WINBOND_CLK_GPIO_Port GPIOB
#define WINBOND_MISO_Pin GPIO_PIN_4
#define WINBOND_MISO_GPIO_Port GPIOB
#define WINBOND_MOSI_Pin GPIO_PIN_5
#define WINBOND_MOSI_GPIO_Port GPIOB
#define I2C_SCL_GY_Pin GPIO_PIN_6
#define I2C_SCL_GY_GPIO_Port GPIOB
#define I2C_SDA_GY_Pin GPIO_PIN_7
#define I2C_SDA_GY_GPIO_Port GPIOB
#define DS18B20_OW_Pin GPIO_PIN_0
#define DS18B20_OW_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
