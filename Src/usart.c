/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */

#include "hmi_screen.h"
#include "openmv_uart.h"
#include "esp01_wifi.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "cmsis_os.h"

/* UART接收缓冲区 */
#define UART_RX_BUFFER_SIZE 256
uint8_t uart1_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart2_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart3_rx_buffer[UART_RX_BUFFER_SIZE];   /* USART3 (OpenMV/PC11) 专用 */
uint8_t uart6_rx_buffer[UART_RX_BUFFER_SIZE];   /* USART6 (ESP-01/PC7) 专用 */

/* USART1互斥锁句柄（在app_freertos.c中创建） */
extern osMutexId uart1MutexHandle;

/* ================================================================
   [DEBUG_CAPTURE] 零阻塞原始协议抓包缓冲区
   ISR只存不打印，任务上下文负责回显 → 不丢字节
   ================================================================ */
#define RX_CAP_SIZE   64
volatile uint8_t  rx_cap_buf[RX_CAP_SIZE];   /* 抓包缓冲 */
volatile uint8_t  rx_cap_idx  = 0;           /* 写指针 */
volatile uint32_t rx_cap_tick  = 0;          /* 最后收字节时刻 */
volatile uint8_t  rx_cap_new   = 0;          /* 有新数据待显示 */

/**
 * @brief 线程安全USART2发送包装器（屏幕模块）
 */
HAL_StatusTypeDef UART1_Tx_Safe(const uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef status;
    if (osMutexWait(uart1MutexHandle, osWaitForever) != osOK) {
        return HAL_ERROR;
    }
    status = HAL_UART_Transmit(&huart2, data, size, 100);
    osMutexRelease(uart1MutexHandle);
    return status;
}

/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART2;
  huart1.Init.BaudRate = 9600;   /* 语音模块 */
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USART2 (PA2=TX, PA3=RX): 语音模块 */
  HAL_UART_Receive_IT(&huart1, &uart1_rx_buffer[0], 1);

  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART1;
  huart2.Init.BaudRate = 115200;  /* 屏幕模块 */
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USART1 (PB6=TX, PB7=RX): 屏幕模块 */
  HAL_UART_Receive_IT(&huart2, &uart2_rx_buffer[0], 1);

  /* USER CODE END USART2_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */

    /* USART1 clock enable (APB2, no separate source selection on F4) */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PB6     ------> USART1_TX
    PB7     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* [DEBUG_NVIC] USART1 NVIC使能 — 屏幕模块 */
  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */

    /* USART2 clock enable (APB1, no separate source selection on F4) */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART2_MspInit 1 */

  /* [DEBUG_NVIC] USART2 NVIC使能 — 语音模块 (优先级5=FreeRTOS安全) */
  HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);

  /* USER CODE END USART2_MspInit 1 */
  }
  else if (uartHandle->Instance == USART3)
  {
  /* ---- USART3: OpenMV H7 Plus 摔倒检测 (PC10=TX, PC11=RX, 115200) ---- */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  }
  else if (uartHandle->Instance == USART6)
  {
  /* ---- USART6: ATK-ESP-01 WiFi (PC6=TX, PC7=RX, 115200) ---- */
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PB6     ------> USART1_TX
    PB7     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6|GPIO_PIN_7);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);

  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
    __HAL_RCC_USART3_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10|GPIO_PIN_11);
  }
  else if(uartHandle->Instance==USART6)
  {
    __HAL_RCC_USART6_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6|GPIO_PIN_7);
  }
}

/* USER CODE BEGIN 1 */

/**
  * @brief  Rx Transfer completed callback.
  *         USART1 (PB6=TX, PB7=RX) → 屏幕模块 HMI_ParseByte
  *         USART2 (PA2=TX, PA3=RX) → 语音模块
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  /* ---- 1. USART1 接管屏幕模块 ---- */
  if (huart->Instance == USART1) {
    uint8_t byte = uart2_rx_buffer[0];

    /* [DEBUG_GPIO] PC13 LED翻转 — 肉眼确认ISR触发 */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    /* [DEBUG_CAPTURE] 零阻塞存字节进环形缓冲区 */
    if (rx_cap_idx < RX_CAP_SIZE) {
        rx_cap_buf[rx_cap_idx++] = byte;
    }
    rx_cap_tick = HAL_GetTick();
    rx_cap_new  = 1;

    /* 保留原解析器调用（不匹配协议时会停留在P_IDLE，无害） */
    HMI_ParseByte(byte);

    /* 重开 USART2 1字节中断接收 */
    HAL_UART_Receive_IT(&huart2, &uart2_rx_buffer[0], 1);
  }
  /* ---- 2. USART2 接管语音模块 ---- */
  else if (huart->Instance == USART2) {
    /* 语音模块TXD未连接，仅丢弃接收字节并重开中断 */
    HAL_UART_Receive_IT(&huart1, &uart1_rx_buffer[0], 1);
  }
  /* ---- 3. USART3 接管 OpenMV 摔倒检测 (PC11=RX) ---- */
  else if (huart->Instance == USART3) {
    OpenMV_RxCallback(uart3_rx_buffer[0]);
    HAL_UART_Receive_IT(&huart_openmv, &uart3_rx_buffer[0], 1);
  }
  /* ---- 4. USART6 接管 ESP-01 WiFi (PC7=RX) ---- */
  else if (huart->Instance == USART6) {
    Esp01_RxCallback(uart6_rx_buffer[0]);
    HAL_UART_Receive_IT(&huart_esp01, &uart6_rx_buffer[0], 1);
  }
}

/**
  * @brief  Tx Transfer completed callback.
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  (void)huart;
}

/**
  * @brief  UART error callback.
  *         USART2 → 屏幕模块恢复, USART1 → 语音模块恢复
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    HAL_UART_Receive_IT(&huart2, &uart2_rx_buffer[0], 1);
  }
  else if (huart->Instance == USART2) {
    HAL_UART_Receive_IT(&huart1, &uart1_rx_buffer[0], 1);
  }
  else if (huart->Instance == USART3) {
    HAL_UART_Receive_IT(&huart_openmv, &uart3_rx_buffer[0], 1);
  }
  else if (huart->Instance == USART6) {
    HAL_UART_Receive_IT(&huart_esp01, &uart6_rx_buffer[0], 1);
  }
}

/* ================================================================
   LU6288 语音合成模块
   通过 USART2 (PA2=TX, PA3=RX, 9600bps, 8N1) 发送指令字符串
   参考: LU6288使用手册 — <G>语音内容 为合成命令
   注意: 中文必须使用GBK编码，UTF-8会导致乱码/无声
   ================================================================ */
void LU6288_SendString(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), 100);
}

void LU6288_SendVoice(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, data, len, 200);
}

/* USER CODE END 1 */
