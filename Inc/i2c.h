/**
  ******************************************************************************
  * @file           : i2c.h
  * @brief          : I2C interface definitions (I2C1: PB10=SCL, PB11=SDA)
  ******************************************************************************
  */
#ifndef INC_I2C_H_
#define INC_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define I2C1_SCL_GPIO_PORT GPIOB
#define I2C1_SCL_PIN       GPIO_PIN_10
#define I2C1_SDA_GPIO_PORT GPIOB
#define I2C1_SDA_PIN       GPIO_PIN_11

extern I2C_HandleTypeDef hi2c1;

void I2C1_BusRecover(void);
void MX_I2C1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_I2C_H_ */
