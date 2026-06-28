/**
  ******************************************************************************
  * @file    openmv_uart.h
  * @brief   MaixCAM Pro UART 通信模块
  *          USART3 (PC10=TX, PC11=RX, 115200bps) 接收摔倒检测信号
  *
  *          协议: 官方 Maix 通信协议 (二进制帧)
  *          帧格式: header[4B] data_len[4B] flags[1B] cmd[1B] body[nB] CRC16[2B]
  *          header = 0xAA 0xCA 0xAC 0xBB (小端: 0xBBACCAAA)
  *          CRC16-IBM 校验 (多项式 0xA001, 初始值 0x0000)
  *
  *          自定义命令:
  *            APP_CMD_FALL_ALERT = 0x03, body: status(1B) 0=正常 1=摔倒
  *
  *          ISR → 逐字节接收 → 状态机解帧 → CRC校验 → Queue发送事件
  ******************************************************************************
  */
#ifndef __OPENMV_UART_H__
#define __OPENMV_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Exported types ------------------------------------------------------------*/

/** MaixCAM 检测事件类型 */
typedef enum {
    OPENMV_EVT_FALL_DETECTED = 0,   /**< 摔倒检测触发 (body[0]=0x01) */
    OPENMV_EVT_FALL_CLEAR    = 1,   /**< 摔倒解除/正常状态 (body[0]=0x00) */
    OPENMV_EVT_HEARTBEAT     = 2,   /**< 心跳包 (cmd=0x04) */
    OPENMV_EVT_UNKNOWN       = 3,   /**< 未知/无效帧 */
} OpenmvEventType_t;

/* Exported variables --------------------------------------------------------*/

/** USART3 句柄 (PC10=TX, PC11=RX, 115200bps) */
extern UART_HandleTypeDef huart_openmv;

/** MaixCAM 事件队列句柄（ISR → vOpenmvTask） */
extern osMessageQId openmvQueueHandle;

/** 摔倒警报队列句柄（vOpenmvTask → vEsp01Task） */
extern QueueHandle_t fallAlertQueue;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化 USART3 用于 MaixCAM 通信 (115200-8-N-1)
  * @note   引脚: PC10=TX, PC11=RX, 中断接收
  *         使用官方 Maix 二进制通信协议
  */
void MX_OPENMV_UART_Init(void);

/**
  * @brief  MaixCAM UART 接收任务
  *         阻塞等待 openmvQueueHandle 事件
  *         检测到摔倒后通过 fallAlertQueue 通知 vEsp01Task
  * @note   优先级 osPriorityNormal, 防抖间隔 30s
  */
void vOpenmvTask(void const * argument);

/**
  * @brief  触发摔倒警报（由 vOpenmvTask 调用）
  * @note   向 fallAlertQueue 发送事件, vEsp01Task 收到后通过 Server酱发送微信推送
  */
void Esp01_TriggerFallAlert(void);

/**
  * @brief  MaixCAM UART RX 字节回调（由 HAL_UART_RxCpltCallback ISR 调用）
  * @param  byte: 接收到的单字节数据
  * @note   在 ISR 上下文中运行, 执行二进制协议状态机解帧
  */
void OpenMV_RxCallback(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* __OPENMV_UART_H__ */
