/**
  ******************************************************************************
  * @file           : hall_sensor.h
  * @brief          : 49E线性霍尔传感器 — ADC3_IN9 (PF3) 驱动接口
  ******************************************************************************
  */

#ifndef __HALL_SENSOR_H__
#define __HALL_SENSOR_H__

#include "main.h"

/* 霍尔触发状态 */
typedef enum {
    HALL_NOT_TRIGGERED = 0,
    HALL_TRIGGERED     = 1
} HallState_t;

void        Hall_Init(void);
uint16_t    Hall_GetAdc(void);
HallState_t Hall_GetState(void);

#endif /* __HALL_SENSOR_H__ */
