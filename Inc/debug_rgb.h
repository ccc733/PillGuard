/**
  ******************************************************************************
  * @file    debug_rgb.h
  * @brief   4引脚 RGB LED 调试模块 (共阴极)
  *          PA5 = Red, PA6 = Green, PA7 = Blue
  *
  *          颜色含义 (调试 PC11→PC6 链路):
  *            🔵 BLUE   = PC11 收到有效帧 (CRC通过)
  *            🔴 RED    = 解析到摔倒事件 (cmd=0x03)
  *            🟢 GREEN  = 摔倒事件已入队 → 转发给 vEsp01Task
  *            🟡 YELLOW = vEsp01Task 正在处理 AT 指令序列
  *            🟣 MAGENTA= 错误/超时
  *            ⚪ WHITE  = 系统初始化完成
  *            ⚫ OFF    = 空闲等待
  *
  *          升级 PWM: PA5=TIM2_CH1, PA6=TIM13_CH1, PA7=TIM14_CH1
  ******************************************************************************
  */
#ifndef __DEBUG_RGB_H__
#define __DEBUG_RGB_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>

/* ====================================================================
   引脚定义 — 按实际接线修改
   ==================================================================== */

/** RGB LED — Red   (PA5, 推挽输出, HIGH=亮) */
#define RGB_R_PORT      GPIOA
#define RGB_R_PIN       GPIO_PIN_5

/** RGB LED — Green (PA6, 推挽输出, HIGH=亮) */
#define RGB_G_PORT      GPIOA
#define RGB_G_PIN       GPIO_PIN_6

/** RGB LED — Blue  (PA7, 推挽输出, HIGH=亮) */
#define RGB_B_PORT      GPIOA
#define RGB_B_PIN       GPIO_PIN_7

/* ====================================================================
   预定义颜色
   ==================================================================== */

typedef enum {
    RGB_OFF     = 0,    /**< ⚫ 全灭 — 空闲等待 */
    RGB_RED     = 1,    /**< 🔴 摔倒事件检测到 */
    RGB_GREEN   = 2,    /**< 🟢 摔倒事件已转发 */
    RGB_BLUE    = 3,    /**< 🔵 PC11 收到有效帧 */
    RGB_YELLOW  = 4,    /**< 🟡 正在发送 AT 指令 */
    RGB_CYAN    = 5,    /**< 🩵 等待 AT 响应 */
    RGB_MAGENTA = 6,    /**< 🟣 错误/超时 */
    RGB_WHITE   = 7,    /**< ⚪ 系统就绪 */
} RGB_Color_t;

/* ====================================================================
   函数声明
   ==================================================================== */

/**
  * @brief  初始化 RGB LED 引脚 (PA5/PA6/PA7 推挽输出)
  * @note   在 main() 中 MX_GPIO_Init() 之后调用
  */
void RGB_Init(void);

/**
  * @brief  设置 RGB 颜色 (ISR 安全, 直接写 GPIO)
  * @param  color: 预定义颜色枚举值
  * @note   可在 ISR 中调用, 执行时间 < 1μs
  */
void RGB_Set(RGB_Color_t color);

/**
  * @brief  设置原始 R/G/B 电平 (ISR 安全)
  * @param  r: 1=Red亮, 0=灭
  * @param  g: 1=Green亮, 0=灭
  * @param  b: 1=Blue亮, 0=灭
  */
void RGB_SetRaw(uint8_t r, uint8_t g, uint8_t b);

/**
  * @brief  闪烁指定颜色 (仅任务上下文, 会阻塞)
  * @param  color: 颜色
  * @param  duration_ms: 亮持续时间 (ms)
  * @note   内部调用 HAL_Delay, 不可在 ISR 中使用
  */
void RGB_Flash(RGB_Color_t color, uint32_t duration_ms);

/**
  * @brief  交替闪烁两种颜色各 N 次 (仅任务上下文)
  * @param  c1: 颜色1
  * @param  c2: 颜色2
  * @param  count: 交替次数
  * @param  duration_ms: 每种颜色持续时间 (ms)
  */
void RGB_Blink(RGB_Color_t c1, RGB_Color_t c2,
               uint8_t count, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_RGB_H__ */
