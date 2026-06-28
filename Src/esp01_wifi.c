/**
  ******************************************************************************
  * @file    esp01_wifi.c
  * @brief   ESP8266-01 (5引脚版) WiFi AT 指令 + Server酱 HTTPS 推送
  *          USART6 (PC6=TX, PC7=RX, 115200bps)
  *
  *          参考: ESP-AT 用户指南 (乐鑫官方 v3.4.0)
  *          ESP-01 引脚: VCC, GND, TX, RX, RST (仅5脚)
  *
  *          SSL 连接: 官方固件直接使用 AT+CIPSTART="SSL",host,port
  *          无需也不存在 AT+CIPSSL=1 命令
  *
  *          流程: AT 测试 → Station 模式 → 连接 WiFi →
  *                等待摔倒警报 → SSL 连接 → HTTP GET → Server酱 → 微信
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "esp01_wifi.h"
#include "debug_rgb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Private defines ------------------------------------------------------------*/

/** 摔倒警报事件值 */
#define EVT_FALL_ALERT      ((uint32_t)ESP01_EVT_FALL_ALERT)

/** AT 响应行缓冲区大小 */
#define AT_LINE_SIZE        128

/** HTTP 响应超时 (ms) */
#define HTTP_RESP_TIMEOUT   15000

/* Private variables ----------------------------------------------------------*/

/** USART6 句柄 */
UART_HandleTypeDef huart_esp01;

/** ESP-01 AT 响应接收索引 */
static uint16_t esp01_rx_idx = 0;

/** ESP-01 当前 AT 响应状态 */
static volatile uint8_t  esp01_response_ready = 0;
static volatile uint8_t  esp01_response_ok    = 0;
static char     esp01_last_response[ESP01_RX_BUF_SIZE];

/** ESP-01 事件队列 (vEsp01Task 内部事件) */
QueueHandle_t esp01QueueHandle;

/** 模块当前状态 */
volatile Esp01State_t g_esp01_state = ESP01_STATE_INIT;

/** UART 发送互斥锁 */
static SemaphoreHandle_t esp01_tx_mutex = NULL;

/* Private function prototypes ------------------------------------------------*/

static void Esp01_SendCmd(const char *cmd);
static int  Esp01_WaitResponse(const char *expected, uint32_t timeout_ms);
static int  Esp01_SendCmdAndWait(const char *cmd, const char *expected, uint32_t timeout_ms);
static void Esp01_SendHttpAlert(const char *title, const char *content);

/* ================================================================
   USART6 初始化 (PC6=TX, PC7=RX, AF8)
   ================================================================ */
void MX_ESP01_UART_Init(void)
{
    /* [FIX] 强制重新配置 PC6/PC7 为 USART6，防止被 SystemInit 覆盖 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 确保 GPIOC 时钟已使能 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    
    /* 先复位 PC6/PC7 配置 */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
    
    /* 重新配置 PC6/PC7 为 USART6 (AF8) */
    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* 短暂延迟确保配置生效 */
    HAL_Delay(10);
    
    /* 初始化 USART6 */
    huart_esp01.Instance          = USART6;
    huart_esp01.Init.BaudRate     = ESP01_BAUDRATE;
    huart_esp01.Init.WordLength   = UART_WORDLENGTH_8B;
    huart_esp01.Init.StopBits     = UART_STOPBITS_1;
    huart_esp01.Init.Parity       = UART_PARITY_NONE;
    huart_esp01.Init.Mode         = UART_MODE_TX_RX;
    huart_esp01.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart_esp01.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart_esp01) != HAL_OK) {
        Error_Handler();
    }

    /* 启动单字节中断接收（USART6 专用缓冲区） */
    extern uint8_t uart6_rx_buffer[];
    HAL_UART_Receive_IT(&huart_esp01, &uart6_rx_buffer[0], 1);
}

/* ================================================================
   发送 AT 指令（线程安全）
   ================================================================ */
static void Esp01_SendCmd(const char *cmd)
{
    if (esp01_tx_mutex == NULL) {
        HAL_UART_Transmit(&huart_esp01, (uint8_t *)cmd,
                          (uint16_t)strlen(cmd), 1000);
        return;
    }

    if (xSemaphoreTake(esp01_tx_mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        HAL_UART_Transmit(&huart_esp01, (uint8_t *)cmd,
                          (uint16_t)strlen(cmd), 1000);
        xSemaphoreGive(esp01_tx_mutex);
    }
}

/* ================================================================
   等待 AT 响应（任务上下文轮询，不阻塞中断）
   ================================================================ */
static int Esp01_WaitResponse(const char *expected, uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        if (esp01_response_ready) {
            esp01_response_ready = 0;

            /* 检查响应是否包含预期字符串 */
            if (expected == NULL) {
                return esp01_response_ok ? 1 : -1;
            }

            if (strstr(esp01_last_response, expected) != NULL) {
                return 1;   /* 成功 */
            }
            if (strstr(esp01_last_response, "ERROR") != NULL) {
                return -1;  /* 失败 */
            }
            return 0;       /* 不匹配，继续等待 */
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return -2;  /* 超时 */
}

/* ================================================================
   Esp01_SendCmdAndWait — 发送 AT 指令并等待响应
   返回: 1=匹配expected, 0=不匹配, -1=ERROR, -2=超时
   严格对应 test_esp01.py 的 at_send() 函数行为
   ================================================================ */
static int Esp01_SendCmdAndWait(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    Esp01_SendCmd(cmd);
    return Esp01_WaitResponse(expected, timeout_ms);
}

/* ================================================================
   Esp01_SendHttpAlert — 收到摔倒警报后执行完整AT序列
   严格对应 test_esp01.py 的 6 个步骤 (每步已验证可推送成功)
   ================================================================ */
static void Esp01_SendHttpAlert(const char *title, const char *content)
{
    char cmd[ESP01_TX_BUF_SIZE];
    char http_req[ESP01_TX_BUF_SIZE * 2];
    int ret, req_len;
    const char *host = "sctapi.ftqq.com";
    const char *sendkey = SERVERCHAN_SENDKEY;

    /* ================================================================
       步骤 1/6: 测试 AT 通信
       test_esp01.py: at_send("AT", timeout=3)
       ================================================================ */
    g_esp01_state = ESP01_STATE_WIFI_CONNECTING;
    ret = Esp01_SendCmdAndWait("AT\r\n", "OK", 3000);
    if (ret <= 0) {
        /* [DEBUG_RGB] 品红闪: AT 通信异常 */
        RGB_Flash(RGB_MAGENTA, 200);
        /* 重试一次 (test_esp01.py 无此重试，但增加容错) */
        vTaskDelay(pdMS_TO_TICKS(500));
        Esp01_SendCmdAndWait("AT\r\n", "OK", 3000);
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    /* ================================================================
       步骤 2/6: 查看固件版本 (对应 test_esp01.py Step 2)
       test_esp01.py: at_send("AT+GMR", timeout=3)
       ================================================================ */
    Esp01_SendCmdAndWait("AT+GMR\r\n", "OK", 3000);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* ================================================================
       步骤 3/6: 连接 WiFi
       test_esp01.py:
         at_send("AT+CWMODE=1")                        # timeout=15s(默认)
         time.sleep(1)
         at_send('AT+CWJAP="QQNeNe","Cc20060618"',
                 wait_for="WIFI GOT IP", timeout=20)
         # 失败则重试 wait_for="OK", timeout=5
       ================================================================ */
    Esp01_SendCmdAndWait("AT+CWMODE=1\r\n", "OK", 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));  /* test_esp01.py: time.sleep(1) */

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n",
             WIFI_SSID, WIFI_PASSWORD);
    ret = Esp01_SendCmdAndWait(cmd, "WIFI GOT IP", ESP01_AT_TIMEOUT);  /* 20s */
    if (ret <= 0) {
        /* 可能已连接 → 重试 with "OK" (test_esp01.py: timeout=5) */
        ret = Esp01_SendCmdAndWait(cmd, "OK", 5000);
    }
    if (ret > 0) {
        g_esp01_state = ESP01_STATE_WIFI_CONNECTED;
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    /* ================================================================
       步骤 4/6: 建立 SSL 连接
       test_esp01.py:
         at_send("AT+CIPMUX=0")                        # timeout=15s(默认)
         time.sleep(1)
         at_send('AT+CIPSTART="SSL","sctapi.ftqq.com",443',
                 wait_for="CONNECT", timeout=20)
       ================================================================ */
    g_esp01_state = ESP01_STATE_TCP_CONNECTING;
    Esp01_SendCmdAndWait("AT+CIPMUX=0\r\n", "OK", 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));  /* test_esp01.py: time.sleep(1) */

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"SSL\",\"%s\",443\r\n", host);
    ret = Esp01_SendCmdAndWait(cmd, "CONNECT", ESP01_AT_TIMEOUT);  /* 20s */
    if (ret > 0) {
        g_esp01_state = ESP01_STATE_TCP_CONNECTED;
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    /* ================================================================
       步骤 5/6: 发送 HTTP GET 请求
       test_esp01.py:
         http_request = f"GET /{SENDKEY}.send?title=...&desp=... HTTP/1.1\r\n..."
         req_len = len(http_request.encode())
         at_send(f"AT+CIPSEND={req_len}", timeout=5)  # 特殊处理: 等 ">"
         time.sleep(0.5)
         at_send_raw(req_bytes)                        # 原始发送
       ================================================================ */
    snprintf(http_req, sizeof(http_req),
             "GET /%s.send?title=%s&desp=%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             sendkey, title,
             (content != NULL) ? content : "",
             host);
    req_len = (int)strlen(http_req);

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", req_len);
    /* CIPSEND 特殊处理: 等 ">" 符号 (test_esp01.py 中 at_send 对 CIPSEND 有特殊逻辑) */
    Esp01_SendCmd(cmd);
    ret = Esp01_WaitResponse(">", 5000);
    /* 不等 OK, 只等 ">" 即可发送数据 */
    vTaskDelay(pdMS_TO_TICKS(500));  /* test_esp01.py: time.sleep(0.5) */

    /* 发送 HTTP 请求体 (test_esp01.py: at_send_raw, 不带\r\n) */
    g_esp01_state = ESP01_STATE_SENDING;
    Esp01_SendCmd(http_req);

    /* 等待发送完成 (test_esp01.py: time.sleep(3) 读取响应) */
    g_esp01_state = ESP01_STATE_WAIT_RESPONSE;
    vTaskDelay(pdMS_TO_TICKS(3000));  /* test_esp01.py: time.sleep(3) */
    /* 尝试读取响应 (可能包含 SEND OK / CLOSED 等) */
    Esp01_WaitResponse("CLOSED", 5000);
    /* 额外等待 SSL 关闭通知 */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ================================================================
       步骤 6/6: 关闭连接
       test_esp01.py: at_send("AT+CIPCLOSE", timeout=5)
       ================================================================ */
    Esp01_SendCmdAndWait("AT+CIPCLOSE\r\n", "OK", 5000);

    g_esp01_state = ESP01_STATE_WIFI_CONNECTED;
}

/* ================================================================
   vEsp01Task — ESP-01 WiFi 控制任务
   等待 fallAlertQueue → 收到摔倒警报 → 执行完整AT序列
   ================================================================ */
void vEsp01Task(void const * argument)
{
    (void)argument;
    uint32_t event;

    /* 创建发送互斥锁 */
    if (esp01_tx_mutex == NULL) {
        esp01_tx_mutex = xSemaphoreCreateMutex();
    }

    /* 等待 ESP-01 上电稳定 */
    g_esp01_state = ESP01_STATE_INIT;
    vTaskDelay(pdMS_TO_TICKS(3000));
    g_esp01_state = ESP01_STATE_READY;

    /* [DEBUG_RGB] 白灯快闪: ESP01 任务就绪 */
    RGB_Flash(RGB_WHITE, 100);
    RGB_Set(RGB_OFF);

    /* === 主循环 === */
    for (;;) {
        /* 永久阻塞，等待摔倒警报 */
        if (xQueueReceive(fallAlertQueue, &event, portMAX_DELAY) == pdTRUE) {

            if (event == EVT_FALL_ALERT) {
                /* [DEBUG_RGB] 黄灯: 正在处理 AT 序列 */
                RGB_Set(RGB_YELLOW);

                /* ★ 收到摔倒警报 — 先发标记证明链条通畅 ★ */
                Esp01_SendCmd("\r\n[FALL]\r\n");

                /* 构建推送内容 */
                char title[64];
                char content[128];

                snprintf(title, sizeof(title), "摔倒警报");
                snprintf(content, sizeof(content),
                         "提醒：检测到老人摔倒请注意！");

                /* ★ 执行完整 AT 指令序列 (严格对应 test_esp01.py) ★ */
                Esp01_SendHttpAlert(title, content);

                /* 冷却：防止短时间内重复推送 */
                vTaskDelay(pdMS_TO_TICKS(FALL_ALERT_COOLDOWN));

                /* [DEBUG_RGB] 灭灯: AT 序列处理完毕 */
                RGB_Set(RGB_OFF);
            }
        }
    }
}

/* ================================================================
   ISR 回调: USART6 RX (ESP-01 响应数据)
   由 HAL_UART_RxCpltCallback(user_byte) 调用
   ================================================================ */
void Esp01_RxCallback(uint8_t user_byte)
{
    char byte = (char)user_byte;

    /* 行缓冲区存储（非阻塞，ISR 安全） */
    if (esp01_rx_idx < ESP01_RX_BUF_SIZE - 1) {
        esp01_last_response[esp01_rx_idx] = byte;
        esp01_rx_idx++;
    }

    /* 检测 AT 响应结束标记: "\r\nOK\r\n" 或 "\r\nERROR\r\n" */
    if (esp01_rx_idx >= 4) {
        char *tail = &esp01_last_response[esp01_rx_idx - 4];
        if (strncmp(tail, "\r\nOK", 4) == 0 ||
            strncmp(tail, "K\r\n", 3) == 0) {   /* 处理 "OK\r\n" */
            esp01_response_ready = 1;
            esp01_response_ok    = 1;
            esp01_last_response[esp01_rx_idx] = '\0';
            esp01_rx_idx = 0;
        } else if (esp01_rx_idx >= 7) {
            char *tail7 = &esp01_last_response[esp01_rx_idx - 7];
            if (strncmp(tail7, "\r\nERROR", 7) == 0) {
                esp01_response_ready = 1;
                esp01_response_ok    = 0;
                esp01_last_response[esp01_rx_idx] = '\0';
                esp01_rx_idx = 0;
            }
        }
    }

    /* 注意: IT 接收已在 usart.c 的 HAL_UART_RxCpltCallback 中重启 */
}
