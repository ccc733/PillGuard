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
#include "cmsis_os.h"  // FreeRTOS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "openmv_uart.h"   /* OpenMV H7 Plus 摔倒检测 */
#include "esp01_wifi.h"    /* ATK-ESP-01 WiFi + Server酱推送 */
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

/* USER CODE BEGIN Private defines */

/* ================================================================
   RGB LED 引脚定义 (4脚共阴三色LED)
   LED1 (降压药指示): PA0 → Green脚, 220Ω限流 → GND公共端
   LED2 (血糖药指示): PA1 → Green脚, 220Ω限流 → GND公共端
   ================================================================ */
#define LED_BP_PORT             GPIOA
#define LED_BP_PIN              GPIO_PIN_0    /* PA0: 降压药LED绿色 */
#define LED_SUGAR_PORT          GPIOA
#define LED_SUGAR_PIN           GPIO_PIN_1    /* PA1: 血糖药LED绿色 */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
