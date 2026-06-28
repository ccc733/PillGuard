/**
  ******************************************************************************
  * @file           : pin_test.c
  * @brief          : PA15/PB7 pin verification test
  *                   Configures PA15 (SCL) and PB7 (SDA) as plain push-pull
  *                   outputs. Toggles HIGH/LOW in a loop for multimeter
  *                   verification. No I2C, no USART, no peripherals.
  *                   PA15=GPIOA, PB7=GPIOB — different ports.
  *
  *   LED (PC13) fast blink = pins HIGH (expect 3.3V)
  *   LED (PC13) solid ON  = pins LOW  (expect 0V)
  *
  *   Cycle: 5s HIGH → 5s LOW → repeat
  ******************************************************************************
  */

#include "stm32g4xx_hal.h"

static void SystemClock_Config(void);
static void Error_Handler(void);

/* Standalone SysTick handler — no FreeRTOS dependency */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* Enable GPIOA, GPIOB and GPIOC clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* Configure PC13 (LED) as push-pull output */
  GPIO_InitTypeDef led = {0};
  led.Pin = GPIO_PIN_13;
  led.Mode = GPIO_MODE_OUTPUT_PP;
  led.Pull = GPIO_NOPULL;
  led.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &led);

  /* Configure PA15 (SCL) as push-pull output — GPIOA */
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = GPIO_PIN_15;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* Configure PB7 (SDA) as push-pull output — GPIOB */
  gpio.Pin = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOB, &gpio);

  while (1)
  {
    /* ===== PHASE 1: PINS HIGH (expect 3.3V) ===== */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* LED ON */

    /* Fast blink LED to indicate "HIGH phase" — measure now */
    for (int i = 0; i < 50; i++) {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
      HAL_Delay(100);  /* 100ms toggle → 5s total */
    }

    /* ===== PHASE 2: PINS LOW (expect 0V) ===== */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* LED ON solid */

    /* Solid LED for 5s — measure now */
    HAL_Delay(5000);
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
