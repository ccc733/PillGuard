/**
  ******************************************************************************
  * @file    esp01_wifi.h
  * @brief   ESP8266-01 (5引脚版: VCC,GND,TX,RX,RST) WiFi AT 指令模块
  *          Server酱 HTTPS 推送 → 微信通知
  *          USART6 (PC6=TX, PC7=RX, 115200bps)
  *
  *          参考: 乐鑫 ESP-AT 用户指南 v3.4.0
  *          SSL 连接: AT+CIPSTART="SSL","host",443 (官方固件原生支持)
  *          注意: 官方 AT 固件中不存在 AT+CIPSSL 命令
  *
  *          硬件接线 (5引脚 ESP-01):
  *            ESP-01 VCC → 3.3V (独立LDO, 峰值>300mA)
  *            ESP-01 GND → GND
  *            ESP-01 TX  → STM32 PC7 (USART6_RX)
  *            ESP-01 RX  → STM32 PC6 (USART6_TX)
  *            ESP-01 RST → 3.3V (上拉, 正常运行)
  *
  *          Server酱 SendKey: SCT359372TJL0R2eT8Nvqk2naBmWV0zyde
  ******************************************************************************
  */
#ifndef __ESP01_WIFI_H__
#define __ESP01_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* ====================================================================
   用户配置区 — 请填入你的 WiFi 信息
   ==================================================================== */

/** WiFi SSID（2.4GHz，纯英文以避免编码问题） */
#define WIFI_SSID           "QQNeNe"

/** WiFi 密码 */
#define WIFI_PASSWORD       "Cc20060618"

/** Server酱 SendKey（已填入） */
#define SERVERCHAN_SENDKEY  "SCT359372TJL0R2eT8Nvqk2naBmWV0zyde"

/* ====================================================================
   系统配置 — 通常无需修改
   ==================================================================== */

/** ESP-01 波特率 */
#define ESP01_BAUDRATE      115200

/** AT 指令超时 (ms) — 匹配 test_esp01.py 的 20s */
#define ESP01_AT_TIMEOUT    20000

/** WiFi 连接最大重试 */
#define ESP01_WIFI_RETRY    5

/** 摔倒报警冷却时间 (ms)，防止短时间内重复推送 */
#define FALL_ALERT_COOLDOWN 30000

/** AT 响应接收缓冲区大小 */
#define ESP01_RX_BUF_SIZE   512

/** AT 指令发送缓冲区大小 */
#define ESP01_TX_BUF_SIZE   256

/* Exported types ------------------------------------------------------------*/

/** ESP-01 AT 状态机 */
typedef enum {
    ESP01_STATE_INIT = 0,           /* 上电初始化，等待模块就绪 */
    ESP01_STATE_READY,              /* 模块就绪，等待 WiFi 连接 */
    ESP01_STATE_WIFI_CONNECTING,    /* 正在连接 WiFi */
    ESP01_STATE_WIFI_CONNECTED,     /* WiFi 已连接 */
    ESP01_STATE_TCP_CONNECTING,     /* 正在连接 TCP */
    ESP01_STATE_TCP_CONNECTED,      /* TCP 已连接 */
    ESP01_STATE_SENDING,            /* 正在发送 HTTP 请求 */
    ESP01_STATE_WAIT_RESPONSE,      /* 等待服务器响应 */
    ESP01_STATE_ERROR,              /* 错误状态 */
} Esp01State_t;

/** ESP-01 任务事件 */
typedef enum {
    ESP01_EVT_FALL_ALERT = 0,       /* 摔倒警报 → 发送 HTTP */
    ESP01_EVT_WIFI_CONNECT,         /* 连接 WiFi */
    ESP01_EVT_AT_RESPONSE,          /* AT 指令响应到达 */
} Esp01Event_t;

/* Exported variables --------------------------------------------------------*/

/** USART6 句柄 (PC6=TX, PC7=RX, 115200bps) */
extern UART_HandleTypeDef huart_esp01;

/** ESP-01 事件队列句柄（vEsp01Task 内部使用） */
extern QueueHandle_t esp01QueueHandle;

/** ESP-01 当前状态（调试用） */
extern volatile Esp01State_t g_esp01_state;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化 USART6 用于 ESP-01 通信 (115200-8-N-1)
  * @note   引脚: PC6=TX, PC7=RX, 中断接收
  */
void MX_ESP01_UART_Init(void);

/**
  * @brief  ESP-01 WiFi 控制任务
  *         阻塞等待 fallAlertQueue 或 esp01QueueHandle
  *         收到摔倒警报 → AT 指令序列 → HTTP GET → Server酱
  * @note   优先级 osPriorityNormal, 堆栈 512 words
  */
void vEsp01Task(void const * argument);

/**
  * @brief  触发摔倒警报（由 vOpenmvTask 调用）
  * @note   向 fallAlertQueue 发送事件
  */
void Esp01_TriggerFallAlert(void);

/**
  * @brief  ESP-01 UART RX 字节回调（由 HAL_UART_RxCpltCallback ISR 调用）
  * @param  user_byte: 接收到的单字节数据
  * @note   在 ISR 上下文中运行，组装 AT 响应数据
  */
void Esp01_RxCallback(uint8_t user_byte);

#ifdef __cplusplus
}
#endif

#endif /* __ESP01_WIFI_H__ */
