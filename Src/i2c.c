/**
  ******************************************************************************
  * @file           : i2c.c
  * @brief          : I2C1 init (PB10=SCL, PB11=SDA) + bus recovery
  ******************************************************************************
  */

#include "i2c.h"

I2C_HandleTypeDef hi2c1;

static void I2C1_ShortDelay(void)
{
  for (volatile int d = 0; d < 1000; d++) {
    __NOP();
  }
}

static GPIO_PinState I2C1_ReadSDA(void)
{
  return HAL_GPIO_ReadPin(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN);
}

/**
  * @brief  GPIO-level I2C bus recovery (RobTillaart/I2C_SOFTRESET method)
  *         Clock out 9 pulses + STOP to release stuck I2C slaves.
  *         Does NOT touch I2C peripheral registers — pure GPIO bit-bang.
  *
  *         SCL = push-pull output (strong HIGH drive, ~20mA)
  *         SDA = open-drain output (will not fight a slave pulling SDA low)
  *
  *         Always sends 9 clock pulses unconditionally.  Stops early
  *         if SDA goes high (slave released the bus).
  */
void I2C1_BusRecover(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Deinit both pins back to default state (both on GPIOB). */
  HAL_GPIO_DeInit(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN);
  HAL_GPIO_DeInit(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN);

  GPIO_InitTypeDef gpio = {0};
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  /* SDA: open-drain, initial HIGH (GPIOB) */
  gpio.Pin = I2C1_SDA_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  HAL_GPIO_Init(I2C1_SDA_GPIO_PORT, &gpio);
  HAL_GPIO_WritePin(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN, GPIO_PIN_SET);

  /* SCL: push-pull, initial HIGH (GPIOB) */
  gpio.Pin = I2C1_SCL_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(I2C1_SCL_GPIO_PORT, &gpio);
  HAL_GPIO_WritePin(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN, GPIO_PIN_SET);

  I2C1_ShortDelay();

  /* 9 clock pulses — always send unconditionally */
  for (int i = 0; i < 9; i++) {
    HAL_GPIO_WritePin(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN, GPIO_PIN_RESET);
    I2C1_ShortDelay();
    HAL_GPIO_WritePin(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN, GPIO_PIN_SET);
    I2C1_ShortDelay();

    if (I2C1_ReadSDA() == GPIO_PIN_SET) {
      break;
    }
  }

  /* STOP condition: SDA low → SCL high → SDA high */
  HAL_GPIO_WritePin(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN, GPIO_PIN_RESET);
  I2C1_ShortDelay();
  HAL_GPIO_WritePin(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN, GPIO_PIN_SET);
  I2C1_ShortDelay();
  HAL_GPIO_WritePin(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN, GPIO_PIN_SET);
  I2C1_ShortDelay();

  /* Release pins back to default state */
  HAL_GPIO_DeInit(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN);
  HAL_GPIO_DeInit(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN);
}

void MX_I2C1_Init(void)
{
  /* 1. I2C1 clock source is APB1 (PCLK1 = 16MHz) — no separate source selection on F4 */
  (void)0;  /* placeholder */

  /* 2. Enable I2C1 clock then SWRST for clean initial state */
  __HAL_RCC_I2C1_CLK_ENABLE();

  I2C1->CR1 |= I2C_CR1_SWRST;
  I2C1->CR1 &= ~I2C_CR1_SWRST;

  /* 3. HAL I2C1 init (calls MspInit → configures PB10/PB11 as AF4 OD, enables PE) */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (hi2c->Instance == I2C1) {
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();  /* Both PB10(SCL) and PB11(SDA) on GPIOB */

    /* PB10 = I2C1_SCL (AF4, open-drain) */
    GPIO_InitStruct.Pin = I2C1_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(I2C1_SCL_GPIO_PORT, &GPIO_InitStruct);

    /* PB11 = I2C1_SDA (AF4, open-drain) */
    GPIO_InitStruct.Pin = I2C1_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(I2C1_SDA_GPIO_PORT, &GPIO_InitStruct);
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
  if (hi2c->Instance == I2C1) {
    __HAL_RCC_I2C1_CLK_DISABLE();
    HAL_GPIO_DeInit(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN);
    HAL_GPIO_DeInit(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN);
  }
}
