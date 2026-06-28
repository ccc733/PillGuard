/**
  * @brief  USART6 测试程序
  *         用于验证 PC6/PC7 是否能正常收发数据
  */

#include "main.h"
#include "usart.h"

extern UART_HandleTypeDef huart_esp01;
extern uint8_t uart6_rx_buffer[];

/* 测试标志 */
volatile uint8_t g_test_ok = 0;

/**
  * @brief  发送字符串
  */
void USART6_SendString(const char *str)
{
    HAL_UART_Transmit(&huart_esp01, (uint8_t *)str, 
                      (uint16_t)strlen(str), 1000);
}

/**
  * @brief  测试主函数
  */
void USART6_Test(void)
{
    char cmd[64];
    
    /* 发送测试字符串 */
    USART6_SendString("\r\n[USART6 TEST START]\r\n");
    
    /* 发送 AT 命令 */
    snprintf(cmd, sizeof(cmd), "AT\r\n");
    USART6_SendString(">>> Sending: AT\\r\\n\r\n");
    HAL_UART_Transmit(&huart_esp01, (uint8_t *)cmd, strlen(cmd), 1000);
    
    /* 等待响应 */
    HAL_Delay(500);
    
    USART6_SendString("\r\n[USART6 TEST END]\r\n");
}
