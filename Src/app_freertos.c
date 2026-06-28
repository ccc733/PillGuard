/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS 任务创建 + IPC 初始化
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "hmi_screen.h"
#include "openmv_uart.h"
#include "esp01_wifi.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* UART1互斥锁 (防止多任务同时发送导致串口屏指令损坏) */
osMutexId uart1MutexHandle;

/* IPC句柄 (定义在main.c，此处extern供其他模块引用) */
extern SemaphoreHandle_t xHmiRxSemaphore;
extern QueueHandle_t     xVoiceQueue;

/* 任务函数声明（实现在main.c中） */
extern void vMainHmiTask(void const * argument);
extern void vVoiceTask(void const * argument);
extern void vSensorTask(void const * argument);

/* 新增任务（实现在 openmv_uart.c 和 esp01_wifi.c 中） */
extern void vOpenmvTask(void const * argument);
extern void vEsp01Task(void const * argument);

/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);

void MX_FREERTOS_Init(void);

/**
  * @brief  FreeRTOS initialization
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* UART1互斥锁 */
  osMutexDef(uart1Mutex);
  uart1MutexHandle = osMutexCreate(osMutex(uart1Mutex));
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* HMI接收二进制信号量 (初始为空，ISR中Give) */
  xHmiRxSemaphore = xSemaphoreCreateBinary();
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* 5分钟超时由vMainHmiTask中的xTaskGetTickCount()实现，无需软件定时器 */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* 语音事件队列 (深度8，元素为uint32_t事件ID) */
  xVoiceQueue = xQueueCreate(8, sizeof(uint32_t));

  /* OpenMV 事件消息队列 (深度16，ISR安全) */
  osMessageQDef(openmvQueue, 16, uint32_t);
  openmvQueueHandle = osMessageCreate(osMessageQ(openmvQueue), NULL);

  /* 摔倒警报队列 (深度4，vOpenmvTask → vEsp01Task) */
  fallAlertQueue = xQueueCreate(4, sizeof(uint32_t));

  /* ESP-01 内部事件队列 (深度8) */
  esp01QueueHandle = xQueueCreate(8, sizeof(uint32_t));
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */

  /* vMainHmiTask: 主状态机FSM (高优先级, 256字) */
  osThreadDef(MainHmiTask, vMainHmiTask, osPriorityHigh, 0, 256);
  osThreadCreate(osThread(MainHmiTask), NULL);

  /* vVoiceTask: 语音模块框架 (中优先级, 256字) */
  osThreadDef(VoiceTask, vVoiceTask, osPriorityNormal, 0, 256);
  osThreadCreate(osThread(VoiceTask), NULL);

  /* vSensorTask: DS3231时间读取+屏幕更新 (低优先级, 128字) */
  osThreadDef(SensorTask, vSensorTask, osPriorityLow, 0, 128);
  osThreadCreate(osThread(SensorTask), NULL);

  /* vOpenmvTask: OpenMV 摔倒检测数据接收 (中优先级, 256字) */
  osThreadDef(OpenmvTask, vOpenmvTask, osPriorityNormal, 0, 256);
  osThreadCreate(osThread(OpenmvTask), NULL);

  /* vEsp01Task: ESP-01 WiFi + Server酱推送 (中优先级, 512字) */
  osThreadDef(Esp01Task, vEsp01Task, osPriorityNormal, 0, 512);
  osThreadCreate(osThread(Esp01Task), NULL);

  /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE END Application */
