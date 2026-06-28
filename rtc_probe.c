#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "i2c.h"
#include <string.h>
#include <stdio.h>

#define TJC_END "\xff\xff\xff"

extern I2C_HandleTypeDef hi2c1;

static void SystemClock_Config(void);
static void Probe_Send(const char *cmd);
static void Probe_Show(const char *text, uint32_t hold_ms);
static void Probe_Page(void);

int main(void)
{
  uint8_t data = 0;
  GPIO_PinState sda0;
  GPIO_PinState scl0;
  GPIO_PinState sda1;
  GPIO_PinState scl1;

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();

  HAL_Delay(500);
  Probe_Show("BOOT", 800);
  Probe_Show("WAIT PWR", 1500);

  sda0 = HAL_GPIO_ReadPin(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN);
  scl0 = HAL_GPIO_ReadPin(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN);

  {
    char msg[16];
    snprintf(msg, sizeof(msg), "PIN %d%d",
             (sda0 == GPIO_PIN_SET) ? 1 : 0,
             (scl0 == GPIO_PIN_SET) ? 1 : 0);
    Probe_Show(msg, 1500);
  }

  if (scl0 == GPIO_PIN_RESET) {
    while (1) {
      Probe_Show("SCL LOW HW", 1500);
    }
  }

  Probe_Show("BUS REC", 1000);
  I2C1_BusRecover();
  MX_I2C1_Init();

  sda1 = HAL_GPIO_ReadPin(I2C1_SDA_GPIO_PORT, I2C1_SDA_PIN);
  scl1 = HAL_GPIO_ReadPin(I2C1_SCL_GPIO_PORT, I2C1_SCL_PIN);

  {
    char msg[16];
    snprintf(msg, sizeof(msg), "PIN %d%d",
             (sda1 == GPIO_PIN_SET) ? 1 : 0,
             (scl1 == GPIO_PIN_SET) ? 1 : 0);
    Probe_Show(msg, 1500);
  }

  if (scl1 == GPIO_PIN_RESET) {
    while (1) {
      Probe_Show("SCL LOW HW", 1500);
    }
  }

  if (sda1 == GPIO_PIN_RESET) {
    while (1) {
      Probe_Show("SDA LOW BUS", 1500);
    }
  }

  Probe_Show("SCAN 68", 1000);
  if (HAL_I2C_IsDeviceReady(&hi2c1, 0x68 << 1, 3, 80) != HAL_OK) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x57 << 1, 2, 40) == HAL_OK ||
        HAL_I2C_IsDeviceReady(&hi2c1, 0x50 << 1, 2, 40) == HAL_OK) {
      while (1) {
        Probe_Show("RTC NOACK", 1500);
      }
    }

    while (1) {
      Probe_Show("NO I2C DEV", 1500);
    }
  }

  Probe_Show("RTC ACK", 1200);

  Probe_Show("READ 0F", 1000);
  if (HAL_I2C_Mem_Read(&hi2c1, 0xD0, 0x0F, I2C_MEMADD_SIZE_8BIT, &data, 1, 100) != HAL_OK) {
    while (1) {
      Probe_Show("RTC RD ERR", 1500);
    }
  }

  Probe_Show("READ 00", 1000);
  if (HAL_I2C_Mem_Read(&hi2c1, 0xD0, 0x00, I2C_MEMADD_SIZE_8BIT, &data, 1, 100) != HAL_OK) {
    while (1) {
      Probe_Show("RTC REG ERR", 1500);
    }
  }

  while (1) {
    Probe_Show("RTC OK", 1500);
  }
}

static void Probe_Send(const char *cmd)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 200);
}

static void Probe_Page(void)
{
  Probe_Send("page 0" TJC_END);
  HAL_Delay(40);
  Probe_Send("page booting" TJC_END);
  HAL_Delay(40);
}

static void Probe_Show(const char *text, uint32_t hold_ms)
{
  char cmd[64];

  Probe_Page();

  snprintf(cmd, sizeof(cmd), "t0.txt=\"%s\"" TJC_END, text);
  Probe_Send(cmd);
  HAL_Delay(40);

  snprintf(cmd, sizeof(cmd), "t1.txt=\"%s\"" TJC_END, text);
  Probe_Send(cmd);
  HAL_Delay(40);

  HAL_Delay(hold_ms);
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      while (1) {
      }
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
      while (1) {
      }
    }
    return;
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    while (1) {
    }
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(120);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(120);
  }
}
