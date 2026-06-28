/**
  ******************************************************************************
  * @file           : hall_sensor.c
  * @brief          : 49E线性霍尔传感器 — PF3 / ADC3_IN9 模拟电压读取
  *                   磁铁靠近ADC≈2353~2822，远离ADC≈2057~2061
  *                   ADC < 2200 判定为开盖触发
  ******************************************************************************
  */

#include "hall_sensor.h"

/* ADC阈值：< HALL_THRESHOLD 即磁铁远离（开盖触发） */
#define HALL_THRESHOLD          2200U

static ADC_HandleTypeDef hadc3;

/**
  * @brief 初始化 PF3 为 ADC3_IN9 模拟输入
  */
void Hall_Init(void)
{
    __HAL_RCC_ADC3_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* PF3 → 模拟模式 (ADC3_IN9) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* ADC3 基础配置：12bit，单次转换，软件触发 */
    hadc3.Instance = ADC3;
    hadc3.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
    hadc3.Init.Resolution = ADC_RESOLUTION_12B;
    hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc3.Init.ScanConvMode = DISABLE;
    hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc3.Init.ContinuousConvMode = DISABLE;
    hadc3.Init.NbrOfConversion = 1;
    hadc3.Init.DiscontinuousConvMode = DISABLE;
    hadc3.Init.NbrOfDiscConversion = 0;
    hadc3.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
    hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc3.Init.DMAContinuousRequests = DISABLE;
    if (HAL_ADC_Init(&hadc3) != HAL_OK) {
        return;
    }

    /* 配置通道9，采样时间28周期 */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_9;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    sConfig.Offset = 0;
    HAL_ADC_ConfigChannel(&hadc3, &sConfig);
}

/**
  * @brief 读取PF3 ADC3_IN9原始值（12bit: 0-4095）
  *        读取失败返回 2048（低于阈值，偏向开盖，安全侧）
  */
uint16_t Hall_GetAdc(void)
{
    uint16_t val = 2048U;

    HAL_ADC_Start(&hadc3);
    if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK) {
        val = (uint16_t)HAL_ADC_GetValue(&hadc3);
    }
    HAL_ADC_Stop(&hadc3);
    return val;
}

/**
  * @brief 判断霍尔是否触发（磁铁远离=开盖）
  *        磁铁靠近ADC≈2353~2822，远离ADC≈2057~2061
  *        ADC < 2200 → 磁铁远离 → 开盖触发
  */
HallState_t Hall_GetState(void)
{
    uint16_t adc = Hall_GetAdc();

    if (adc < HALL_THRESHOLD) {
        return HALL_TRIGGERED;
    }
    return HALL_NOT_TRIGGERED;
}
