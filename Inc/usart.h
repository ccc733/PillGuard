/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

/* 新增: OpenMV + ESP-01 模块 */
extern UART_HandleTypeDef huart_openmv;   /* USART3: PC10=TX, PC11=RX, 115200 */
extern UART_HandleTypeDef huart_esp01;     /* USART6: PC6=TX, PC7=RX, 115200 */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_OPENMV_UART_Init(void);            /* USART3 → OpenMV H7 Plus */
void MX_ESP01_UART_Init(void);             /* USART6 → ATK-ESP-01 */

/* USER CODE BEGIN Prototypes */

/**
 * @brief 线程安全USART1发送包装器
 *        使用互斥锁防止多任务并发发送导致串口屏指令损坏
 * @note  仅在调度器启动后使用，不可在ISR中调用
 */
HAL_StatusTypeDef UART1_Tx_Safe(const uint8_t *data, uint16_t size);

/* LU6288 语音合成模块 — 通过USART2 (PA2=TX, PA3=RX, 9600bps) 发送指令 */
void LU6288_SendString(const char *str);
void LU6288_SendVoice(const uint8_t *data, uint16_t len);

/* [DEBUG_CAPTURE] 零阻塞协议抓包缓冲区 — ISR写入，任务读取 */
#define RX_CAP_SIZE   64
extern volatile uint8_t  rx_cap_buf[RX_CAP_SIZE];
extern volatile uint8_t  rx_cap_idx;
extern volatile uint32_t rx_cap_tick;
extern volatile uint8_t  rx_cap_new;

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

