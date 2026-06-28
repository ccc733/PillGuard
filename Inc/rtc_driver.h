/**
  ******************************************************************************
  * @file           : rtc_driver.h
  * @brief          : RTC驱动接口定义
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026
  * All rights reserved.
  *
  ******************************************************************************
  */

#ifndef INC_RTC_DRIVER_H_
#define INC_RTC_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Includes ------------------------------------------------------------------*/
/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} RTC_TimeStruct;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */
HAL_StatusTypeDef RTC_Init(void);
HAL_StatusTypeDef RTC_ReadTime(RTC_TimeStruct *time);
HAL_StatusTypeDef RTC_SetTime(RTC_TimeStruct *time);

#ifdef __cplusplus
}
#endif

#endif /* INC_RTC_DRIVER_H_ */