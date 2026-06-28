/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines (STM32F407ZGT6).
  *          FreeRTOS-aware: SysTick → xPortSysTickHandler
  *          Peripherals: USART1(screen), USART2(voice), EXTI3(key), EXTI15_10(SQW)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_it.h"
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

void NMI_Handler(void)
{
  while (1) {}
}

void HardFault_Handler(void)
{
  while (1) {}
}

void MemManage_Handler(void)
{
  while (1) {}
}

void BusFault_Handler(void)
{
  while (1) {}
}

void UsageFault_Handler(void)
{
  while (1) {}
}

/* SVC_Handler and PendSV_Handler are defined in FreeRTOS port.c */
/* DebugMon_Handler stub */
void DebugMon_Handler(void)
{
}

/**
  * @brief System tick timer handler (FreeRTOS tick).
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
#if (INCLUDE_xTaskGetSchedulerState == 1 )
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
#endif
  xPortSysTickHandler();
#if (INCLUDE_xTaskGetSchedulerState == 1 )
  }
#endif
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/**
  * @brief This function handles USART1 global interrupt (PB6/PB7 → 屏幕模块).
  */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);  /* huart2 = screen on USART1 */
}

/**
  * @brief This function handles USART2 global interrupt (PA2/PA3 → 语音模块).
  */
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);  /* huart1 = voice on USART2 */
}

/**
  * @brief This function handles EXTI line 3 interrupt (PB3 — 按键KEY，已弃用).
  */
void EXTI3_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}

/**
  * @brief This function handles EXTI line 12 interrupt (PB12 — DS3231 SQW).
  *        Shares EXTI15_10 IRQ with lines 10-15.
  */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
}

/**
  * @brief This function handles USART3 global interrupt (PC10/PC11 → OpenMV).
  */
void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart_openmv);
}

/**
  * @brief This function handles USART6 global interrupt (PC6/PC7 → ESP-01).
  */
void USART6_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart_esp01);
}

/* USER CODE END 1 */
