/**
  ******************************************************************************
  * @file           : bus_hardreset_test.c
  * @brief          : DS3231 bus hard-reset diagnostic
  *
  *   Only DS3231 is connected (VCC, GND, SCL, SDA) — no battery, no 32K crystal.
  *
  *   Phase 1: Hard-reset sequence
  *     SDA = open-drain output
  *     SCL = push-pull output  (stronger drive than OD)
  *     9 clock pulses on SCL while SDA=HIGH
  *     STOP condition (SDA low → SCL high → SDA high)
  *
  *   Phase 2: Release & measure
  *     Both pins → open-drain output, set HIGH
  *     LED blink pattern indicates SCL/SDA pin state (read via IDR)
  *
  *   LED pattern (5 cycles, then repeat):
  *     LED blinks = SCL state (1 blink = HIGH, 2 blinks = LOW)
  *     Pause
  *     LED blinks = SDA state (1 blink = HIGH, 2 blinks = LOW)
  *     Pause
  *     → Also measurable with multimeter during this phase
  *
  *   Success: Both HIGH (3.3V) → DS3231 released, can init I2C
  *   Failure: SCL or SDA still LOW → DS3231 is pulling the line down
  ******************************************************************************
  */

#include "stm32g4xx_hal.h"

static void SystemClock_Config(void);
static void Error_Handler(void);
static void ShortDelay(void);

/* ---- Pin defines (PA15=SCL, PB7=SDA) ---- */
#define TEST_SCL_PORT  GPIOA
#define TEST_SCL_PIN   GPIO_PIN_15
#define TEST_SDA_PORT  GPIOB
#define TEST_SDA_PIN   GPIO_PIN_7
#define TEST_LED_PORT  GPIOC
#define TEST_LED_PIN   GPIO_PIN_13

void SysTick_Handler(void)
{
  HAL_IncTick();
}

static void ShortDelay(void)
{
  for (volatile int d = 0; d < 5000; d++) {
    __NOP();
  }
}

int main(void)
{
  GPIO_InitTypeDef gpio = {0};

  HAL_Init();
  SystemClock_Config();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* LED */
  gpio.Pin   = TEST_LED_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_LED_PORT, &gpio);
  HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_SET); /* LED OFF */

  /* ===============================================
     PHASE 1: Hard-reset bus sequence
     SDA = open-drain output
     SCL = push-pull output  (drives LOW hard, releases to external pull-up)
     =============================================== */

  /* SDA: open-drain output, initial HIGH */
  gpio.Pin   = TEST_SDA_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_OD;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_SDA_PORT, &gpio);
  HAL_GPIO_WritePin(TEST_SDA_PORT, TEST_SDA_PIN, GPIO_PIN_SET);

  /* SCL: push-pull output, initial HIGH */
  gpio.Pin   = TEST_SCL_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_SCL_PORT, &gpio);
  HAL_GPIO_WritePin(TEST_SCL_PORT, TEST_SCL_PIN, GPIO_PIN_SET);

  /* Wait for DS3231 power-up */
  HAL_Delay(500);

  /* 9 clock pulses on SCL while SDA remains HIGH */
  for (int i = 0; i < 9; i++) {
    HAL_GPIO_WritePin(TEST_SCL_PORT, TEST_SCL_PIN, GPIO_PIN_RESET);
    ShortDelay();
    HAL_GPIO_WritePin(TEST_SCL_PORT, TEST_SCL_PIN, GPIO_PIN_SET);
    ShortDelay();
  }

  /* STOP condition: SDA low → SCL high → SDA high */
  HAL_GPIO_WritePin(TEST_SDA_PORT, TEST_SDA_PIN, GPIO_PIN_RESET);
  ShortDelay();
  HAL_GPIO_WritePin(TEST_SCL_PORT, TEST_SCL_PIN, GPIO_PIN_SET);
  ShortDelay();
  HAL_GPIO_WritePin(TEST_SDA_PORT, TEST_SDA_PIN, GPIO_PIN_SET);
  ShortDelay();

  /* ===============================================
     PHASE 2: Release & measure
     Both pins → open-drain output, set HIGH
     Now measure with multimeter!
     =============================================== */

  /* SDA: open-drain, HIGH (released) */
  gpio.Pin   = TEST_SDA_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_OD;
  gpio.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(TEST_SDA_PORT, &gpio);
  HAL_GPIO_WritePin(TEST_SDA_PORT, TEST_SDA_PIN, GPIO_PIN_SET);

  /* SCL: open-drain, HIGH (released) */
  gpio.Pin   = TEST_SCL_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_OD;
  gpio.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(TEST_SCL_PORT, &gpio);
  HAL_GPIO_WritePin(TEST_SCL_PORT, TEST_SCL_PIN, GPIO_PIN_SET);

  while (1)
  {
    GPIO_PinState scl = HAL_GPIO_ReadPin(TEST_SCL_PORT, TEST_SCL_PIN);
    GPIO_PinState sda = HAL_GPIO_ReadPin(TEST_SDA_PORT, TEST_SDA_PIN);

    /*
       LED blink pattern:
       - SCL state: 1 blink=HIGH, 2 blinks=LOW
       - Pause
       - SDA state: 1 blink=HIGH, 2 blinks=LOW
       - Pause
    */

    /* Report SCL */
    int scl_blinks = (scl == GPIO_PIN_SET) ? 1 : 2;
    for (int i = 0; i < scl_blinks; i++) {
      HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_RESET);
      HAL_Delay(300);
      HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_SET);
      HAL_Delay(300);
    }
    HAL_Delay(1500);

    /* Report SDA */
    int sda_blinks = (sda == GPIO_PIN_SET) ? 1 : 2;
    for (int i = 0; i < sda_blinks; i++) {
      HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_RESET);
      HAL_Delay(300);
      HAL_GPIO_WritePin(TEST_LED_PORT, TEST_LED_PIN, GPIO_PIN_SET);
      HAL_Delay(300);
    }
    HAL_Delay(1500);
  }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM = RCC_PLLM_DIV2;
  osc.PLL.PLLN = 85;
  osc.PLL.PLLP = RCC_PLLP_DIV2;
  osc.PLL.PLLQ = RCC_PLLQ_DIV2;
  osc.PLL.PLLR = RCC_PLLR_DIV2;

  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)
      Error_Handler();
    return;
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK)
    Error_Handler();
}

static void Error_Handler(void)
{
  __disable_irq();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIOC->MODER &= ~(3U << (13 * 2));
  GPIOC->MODER |= (1U << (13 * 2));
  GPIOC->OTYPER &= ~(1U << 13);
  while (1) {
    GPIOC->BSRR = (1U << 13);
    for (volatile uint32_t i = 0; i < 200000; i++);
    GPIOC->BSRR = (1U << (13 + 16));
    for (volatile uint32_t i = 0; i < 200000; i++);
  }
}
