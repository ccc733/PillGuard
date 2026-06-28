/**
  ******************************************************************************
  * @file    debug_rgb.c
  * @brief   4引脚 RGB LED 调试模块实现 (共阴极, GPIO 推挽输出)
  *          PA5=Red, PA6=Green, PA7=Blue
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "debug_rgb.h"

/* ================================================================
   RGB_Init — 初始化 PA5/PA6/PA7 为推挽输出
   ================================================================ */
void RGB_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 确保 GPIOA 时钟已使能 */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5 (Red), PA6 (Green), PA7 (Blue) — 推挽输出, 无上下拉 */
    GPIO_InitStruct.Pin   = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 初始全灭 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
                      GPIO_PIN_RESET);
}

/* ================================================================
   RGB_SetRaw — 直接设置 R/G/B 电平 (ISR 安全)
   ================================================================ */
void RGB_SetRaw(uint8_t r, uint8_t g, uint8_t b)
{
    /* 共阴极: HIGH=亮, LOW=灭 */
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN,
                      r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_G_PORT, RGB_G_PIN,
                      g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_B_PORT, RGB_B_PIN,
                      b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ================================================================
   RGB_Set — 设置预定义颜色 (ISR 安全)
   ================================================================ */
void RGB_Set(RGB_Color_t color)
{
    switch (color) {
    case RGB_OFF:     RGB_SetRaw(0, 0, 0); break;  /* ⚫ */
    case RGB_RED:     RGB_SetRaw(1, 0, 0); break;  /* 🔴 */
    case RGB_GREEN:   RGB_SetRaw(0, 1, 0); break;  /* 🟢 */
    case RGB_BLUE:    RGB_SetRaw(0, 0, 1); break;  /* 🔵 */
    case RGB_YELLOW:  RGB_SetRaw(1, 1, 0); break;  /* 🟡 */
    case RGB_CYAN:    RGB_SetRaw(0, 1, 1); break;  /* 🩵 */
    case RGB_MAGENTA: RGB_SetRaw(1, 0, 1); break;  /* 🟣 */
    case RGB_WHITE:   RGB_SetRaw(1, 1, 1); break;  /* ⚪ */
    default:          RGB_SetRaw(0, 0, 0); break;
    }
}

/* ================================================================
   RGB_Flash — 闪烁指定颜色 (任务上下文, 会阻塞)
   ================================================================ */
void RGB_Flash(RGB_Color_t color, uint32_t duration_ms)
{
    RGB_Set(color);
    HAL_Delay(duration_ms);
    RGB_Set(RGB_OFF);
}

/* ================================================================
   RGB_Blink — 交替闪烁两种颜色 (任务上下文, 会阻塞)
   ================================================================ */
void RGB_Blink(RGB_Color_t c1, RGB_Color_t c2,
               uint8_t count, uint32_t duration_ms)
{
    for (uint8_t i = 0; i < count; i++) {
        RGB_Set(c1);
        HAL_Delay(duration_ms);
        RGB_Set(c2);
        HAL_Delay(duration_ms);
    }
    RGB_Set(RGB_OFF);
}
