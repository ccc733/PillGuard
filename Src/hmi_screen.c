/**
  ******************************************************************************
  * @file           : hmi_screen.c
  * @brief          : TJC串口屏驱动 + 0x55协议帧解析器实现
  *                   支持命令: 0x02(设闹钟,4字节小端整型), 0x03(确认服药,0字节)
  ******************************************************************************
  */

#include "hmi_screen.h"
#include "usart.h"
#include "soft_i2c_ds3231.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"

/* ---- 全局变量 ---- */
uint8_t  hmi_alarm_data[2] = {0};  /* [0]=hour, [1]=minute */
uint8_t  hmi_frame_cmd      = 0;
volatile uint8_t g_screen_confirmed = 0;
volatile uint8_t g_snooze_pressed   = 0;
volatile uint8_t g_hmi_diag_code = 0;
volatile uint8_t g_medicine_type  = 0;   /* 当前药品类型: MED_TYPE_BP/MED_TYPE_SUGAR */
volatile uint8_t g_led_trigger    = 0;   /* LED触发: bit0=降压药LED, bit1=血糖药LED */

/* ---- 屏幕UART互斥锁（USART2, 在app_freertos.c中创建）---- */
extern osMutexId uart1MutexHandle;

/* ---- 信号量句柄（在app_freertos.c中创建）---- */
extern SemaphoreHandle_t xHmiRxSemaphore;

/* ================================================================
   发送函数（线程安全）
   ================================================================ */

/**
 * @brief 线程安全的屏幕UART发送包装器（USART2）
 */
static HAL_StatusTypeDef UART1_Tx_Protected(const uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef status;
    if (osMutexWait(uart1MutexHandle, osWaitForever) != osOK) {
        return HAL_ERROR;
    }
    status = HAL_UART_Transmit(&huart2, data, size, 200);
    osMutexRelease(uart1MutexHandle);
    return status;
}
/**
 * @brief 发送原始指令到串口屏
 */
void HMI_SendRaw(const char *cmd)
{
    UART1_Tx_Protected((const uint8_t *)cmd, strlen(cmd));
    /* 帧尾 \xFF\xFF\xFF 独立发送，避免编译器对字符串字面量中 0xFF 的转义截断 */
    UART1_Tx_Protected((const uint8_t *)"\xFF\xFF\xFF", 3);
}

/**
 * @brief 切换串口屏页面
 */
void HMI_SendPage(const char *page)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "page %s", page);
    HMI_SendRaw(cmd);
    osDelay(40);
}

/**
 * @brief 设置文本控件内容（可变参数格式化）
 */
void HMI_SetText(const char *widget, const char *fmt, ...)
{
    char buf[64];
    char cmd[80];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", widget, buf);
    HMI_SendRaw(cmd);
}

/* ================================================================
   UART接收帧解析 — 固定长度帧（无需复杂状态机）
   协议: 0x55 | hour | minute | 0xFF | 0xFF | 0xFF  (6字节固定)
     hour   = 闹钟小时 (0-23)
     minute = 闹钟分钟 (0-59)
   帧尾三个 0xFF 用于帧校验，不匹配则丢弃整帧
   ================================================================ */

#define FRAME_LEN 7
static volatile uint8_t  fbuf[FRAME_LEN];
static volatile uint8_t  fidx = 0;

void HMI_Init(void)
{
    fidx = 0;
    hmi_frame_cmd = 0;
    g_screen_confirmed = 0;
    g_snooze_pressed = 0;
    g_hmi_diag_code = 0;
    g_medicine_type = 0;
    g_led_trigger = 0;
}

void HMI_ParseByte(uint8_t byte)
{
    if (fidx > 0U && byte == 0x55U) {
        fidx = 0;
    }

    if (fidx == 0U) {
        if (byte == 0x55U) {
            fbuf[fidx++] = byte;
            g_hmi_diag_code = 1;
        }
        return;
    }

    fbuf[fidx++] = byte;

    if (fidx >= 6U &&
        fbuf[fidx - 3U] == 0xFFU &&
        fbuf[fidx - 2U] == 0xFFU &&
        fbuf[fidx - 1U] == 0xFFU) {

        if (fidx == 6U) {
            uint8_t b1 = fbuf[1];
            uint8_t b2 = fbuf[2];

            if (b1 == 0xEEU && b2 == 0x01U) {
                /* missed页面b111按钮 → snooze */
                g_snooze_pressed = 1;
                g_hmi_diag_code = 4;
            } else if (b1 == 0xFFU && b2 == 0xFFU) {
                g_screen_confirmed = 1;
                g_hmi_diag_code = 2;
            } else if (b1 < 24U && b2 < 60U) {
                hmi_alarm_data[0] = b1;
                hmi_alarm_data[1] = b2;
                hmi_frame_cmd = 0x02;
                g_hmi_diag_code = 3;
            } else {
                fidx = 0;
                return;
            }
        } else if (fidx == 7U) {
            if (fbuf[1] == 0xAAU && fbuf[2] == 0x01U && fbuf[3] == 0x0DU) {
                /* reminding页面b0确认服药: 55 AA 01 0D FF FF FF */
                g_screen_confirmed = 1;
                g_hmi_diag_code = 2;
            } else if (fbuf[1] < 24U && fbuf[2] < 60U &&
                       (fbuf[3] == MED_TYPE_BP || fbuf[3] == MED_TYPE_SUGAR)) {
                /* page1 b11/b22设闹钟: 55 HH MM TYPE FF FF FF
                   TYPE = 0x01(降压药) / 0x02(血糖药) */
                hmi_alarm_data[0] = fbuf[1];   /* hour */
                hmi_alarm_data[1] = fbuf[2];   /* minute */
                hmi_frame_cmd    = fbuf[3];    /* MED_TYPE_BP or MED_TYPE_SUGAR */
                g_medicine_type  = fbuf[3];
                g_hmi_diag_code  = 5;          /* 新诊断码: 药品类型闹钟 */
            } else {
                fidx = 0;
                return;
            }
        }

        if (xHmiRxSemaphore != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xHmiRxSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        fidx = 0;
        return;
    }

    if (fidx >= FRAME_LEN) {
        fidx = 0;
    }
}

/**
 * @brief HMI帧解析器（在 USART2 RX ISR 中逐字节调用）
 *        固定6字节帧: [0]=0x55 [1]=hour [2]=minute [3-5]=0xFF
 *        确认帧: [0]=0x55 [1]=0xFF [2]=0xFF [3-5]=0xFF
 *        无阻塞、无复杂状态机、ISR安全
 */
#if 0
static void HMI_ParseByte_old(uint8_t byte)
{
    if (fidx == 0) {
        /* 等待帧头 0x55 */
        if (byte == 0x55) {
            fbuf[fidx++] = byte;
        }
        return;
    }

    /* fidx = 1..5: 填充剩余字节 */
    fbuf[fidx++] = byte;

    if (fidx == FRAME_LEN) {
        /* 满6字节：校验帧尾三个0xFF */
        if (fbuf[3] == 0xFF && fbuf[4] == 0xFF && fbuf[5] == 0xFF) {
            uint8_t b1 = fbuf[1];
            uint8_t b2 = fbuf[2];

            if (b1 == 0xFF && b2 == 0xFF) {
                /* 确认服药帧: 55 FF FF FF FF FF */
                g_screen_confirmed = 1;
                if (xHmiRxSemaphore != NULL) {
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    xSemaphoreGiveFromISR(xHmiRxSemaphore, &xHigherPriorityTaskWoken);
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                }
            } else if (b1 < 24 && b2 < 60) {
                /* 设闹钟帧: 55 HH MM FF FF FF */
                hmi_alarm_data[0] = b1;
                hmi_alarm_data[1] = b2;
                hmi_frame_cmd = 0x02;

                if (xHmiRxSemaphore != NULL) {
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    xSemaphoreGiveFromISR(xHmiRxSemaphore, &xHigherPriorityTaskWoken);
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                }
            }
        }
        /* 无论校验成败，重置索引准备下一帧 */
        fidx = 0;
    }
}
#endif
