/**
  ******************************************************************************
  * @file    openmv_uart.c
  * @brief   MaixCAM Pro UART 通信模块 (接收摔倒检测结果)
  *          USART3 (PC10=TX, PC11=RX, 115200bps)
  *
  *          协议: 官方 Maix 通信协议 (二进制帧)
  *          =========================================
  *          帧格式:
  *            header[4B LE]  = 0xAA 0xCA 0xAC 0xBB
  *            data_len[4B LE]= flags(1) + cmd(1) + body(n) + CRC(2) 的总长度
  *            flags[1B]      = bit7:is_resp, bit6:resp_ok, bit5:is_report
  *            cmd[1B]        = 0x03 (摔倒警报, 自定义APP命令)
  *            body[nB]       = 变长数据
  *            CRC16[2B LE]   = CRC16-IBM 校验 (header+data_len+flags+cmd+body)
  *
  *          参考:
  *            https://wiki.sipeed.com/maixpy/doc/zh/comm/maix_protocol.html
  *            https://wiki.sipeed.com/maixcdk/doc/zh/convention/protocol.html
  *
  *          ISR 逐字节接收 → 状态机解帧 → CRC校验 → Queue发送事件
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "openmv_uart.h"
#include "esp01_wifi.h"
#include "debug_rgb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* Private defines ------------------------------------------------------------*/

/** 二进制协议帧头 (小端字节序: 0xBBACCAAA → 线上 0xAA,0xCA,0xAC,0xBB) */
#define PROTO_HEADER_MAGIC      0xBBACCAAAU

/** 帧头字节序列 */
static const uint8_t PROTO_HEADER_BYTES[4] = {0xAA, 0xCA, 0xAC, 0xBB};

/** 协议解析状态 */
typedef enum {
    PROTO_SYNC = 0,         /**< 同步: 寻找帧头 0xAA */
    PROTO_HEADER,           /**< 匹配帧头剩余 3 字节 */
    PROTO_DATA_LEN,         /**< 读取 data_len (4B LE) */
    PROTO_PAYLOAD,          /**< 读取 payload (flags+cmd+body+CRC) */
} ProtoState_t;

/** 最大 payload 长度 (防止缓冲区溢出) */
#define MAX_PAYLOAD_SIZE    64

/** 自定义 APP 命令: 摔倒警报 */
#define APP_CMD_FALL_ALERT  0x03U

/** Flags 位定义 */
#define FLAG_IS_RESP        0x80U   /**< bit7: 1=响应帧 */
#define FLAG_RESP_OK        0x40U   /**< bit6: 1=成功响应 */
#define FLAG_IS_REPORT      0x20U   /**< bit5: 1=主动上报 */

/** 摔倒报警冷却计数 (30s / 任务周期) */
#define FALL_COOLDOWN_TICKS (FALL_ALERT_COOLDOWN / 50)

/* Private variables ----------------------------------------------------------*/

/** USART3 句柄 */
UART_HandleTypeDef huart_openmv;

/** 协议解析状态机变量 */
static ProtoState_t proto_state     = PROTO_SYNC;
static uint8_t      header_match_idx = 0;
static uint8_t      data_len_buf[4];
static uint8_t      data_len_idx     = 0;
static uint32_t     payload_len      = 0;      /**< data_len 字段值 (flags+cmd+body+CRC 总长) */
static uint8_t      payload_buf[MAX_PAYLOAD_SIZE];
static uint32_t     payload_idx      = 0;

/** OpenMV 事件队列 (ISR → vOpenmvTask) */
osMessageQId openmvQueueHandle;

/** 摔倒警报队列 (vOpenmvTask → vEsp01Task) */
QueueHandle_t fallAlertQueue;

/** 防抖: 上次摔倒报警的时间戳 */
static TickType_t last_fall_tick = 0;

/* Private function prototypes ------------------------------------------------*/

static uint16_t CRC16_IBM(const uint8_t *data, uint32_t len);
static uint32_t LE_To_U32(const uint8_t *buf);
static uint16_t LE_To_U16(const uint8_t *buf);

/* ================================================================
   CRC16-IBM (CRC-16/MODBUS)
   多项式: 0xA001 (reversed 0x8005)
   初始值: 0x0000
   输入/输出反转: true
   校验范围: header + data_len + flags + cmd + body
   ================================================================ */
static uint16_t CRC16_IBM(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0x0000;
    uint32_t i;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* ================================================================
   小端字节序转换
   ================================================================ */
static uint32_t LE_To_U32(const uint8_t *buf)
{
    return ((uint32_t)buf[0])
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static uint16_t LE_To_U16(const uint8_t *buf)
{
    return ((uint16_t)buf[0])
         | ((uint16_t)buf[1] << 8);
}

/* ================================================================
   USART3 初始化 (PC10=TX, PC11=RX, AF7)
   ================================================================ */
void MX_OPENMV_UART_Init(void)
{
    huart_openmv.Instance          = USART3;
    huart_openmv.Init.BaudRate     = 115200;
    huart_openmv.Init.WordLength   = UART_WORDLENGTH_8B;
    huart_openmv.Init.StopBits     = UART_STOPBITS_1;
    huart_openmv.Init.Parity       = UART_PARITY_NONE;
    huart_openmv.Init.Mode         = UART_MODE_TX_RX;
    huart_openmv.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart_openmv.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart_openmv) != HAL_OK) {
        Error_Handler();
    }

    /* 启动单字节中断接收 */
    extern uint8_t uart3_rx_buffer[];
    HAL_UART_Receive_IT(&huart_openmv, &uart3_rx_buffer[0], 1);
}

/* ================================================================
   Maix 二进制协议帧解析 — ISR 中调用 (逐字节驱动)
   ================================================================ */
static void ParseProtoByte(uint8_t byte)
{
    switch (proto_state) {

    /* ---- 同步: 寻找帧头第一个字节 0xAA ---- */
    case PROTO_SYNC:
        if (byte == 0xAA) {
            header_match_idx = 1;
            proto_state = PROTO_HEADER;
        }
        /* 否则保持 PROTO_SYNC，等待 0xAA */
        break;

    /* ---- 匹配帧头剩余 3 字节 ---- */
    case PROTO_HEADER:
        if (byte == PROTO_HEADER_BYTES[header_match_idx]) {
            header_match_idx++;
            if (header_match_idx >= 4) {
                /* 帧头完整 */
                data_len_idx = 0;
                proto_state = PROTO_DATA_LEN;
            }
        } else {
            /* 帧头不匹配, 回退: 检查当前字节是否为新帧头 */
            if (byte == 0xAA) {
                header_match_idx = 1;
                /* 保持 PROTO_HEADER */
            } else {
                header_match_idx = 0;
                proto_state = PROTO_SYNC;
            }
        }
        break;

    /* ---- 读取 data_len (4 字节小端) ---- */
    case PROTO_DATA_LEN:
        data_len_buf[data_len_idx++] = byte;
        if (data_len_idx >= 4) {
            payload_len = LE_To_U32(data_len_buf);

            /* 边界检查: 至少需要 flags+cmd+CRC = 4 字节 */
            if (payload_len < 4 || payload_len > MAX_PAYLOAD_SIZE) {
                /* 非法长度, 丢弃整帧, 重新同步 */
                proto_state = PROTO_SYNC;
                header_match_idx = 0;
                break;
            }

            payload_idx = 0;
            proto_state = PROTO_PAYLOAD;
        }
        break;

    /* ---- 读取 payload (flags + cmd + body + CRC) ---- */
    case PROTO_PAYLOAD:
        if (payload_idx < MAX_PAYLOAD_SIZE) {
            payload_buf[payload_idx++] = byte;
        }

        if (payload_idx >= payload_len) {
            /* 整帧接收完毕, 进行校验和解析 */

            /* --- CRC16 校验 ---
               CRC 覆盖范围: header(4B) + data_len(4B) + (payload 中除 CRC 外的部分)
               payload 结构: [flags][cmd][body...][CRC16_LE(2B)]
               所以 CRC 计算范围不含 payload 的最后 2 字节 (CRC 自身)
            */
            uint32_t crc_data_len = 4 + 4 + payload_len - 2;  /* header + data_len + flags+cmd+body */
            uint8_t crc_buf[4 + 4 + MAX_PAYLOAD_SIZE];

            /* 组装 CRC 计算缓冲区 */
            memcpy(&crc_buf[0], PROTO_HEADER_BYTES, 4);       /* header */
            memcpy(&crc_buf[4], data_len_buf, 4);             /* data_len */
            memcpy(&crc_buf[8], payload_buf, payload_len - 2); /* flags+cmd+body */

            uint16_t crc_calc = CRC16_IBM(crc_buf, crc_data_len);
            uint16_t crc_recv = LE_To_U16(&payload_buf[payload_len - 2]);

            if (crc_calc == crc_recv) {
                /* CRC 校验通过 — 提取字段 */
                uint8_t flags = payload_buf[0];
                uint8_t cmd   = payload_buf[1];

                /* [DEBUG_RGB] 蓝灯: PC11 收到有效帧 */
                RGB_Set(RGB_BLUE);

                /* 只处理主动上报帧: is_report=1
                   (不检查 is_resp — Maix 协议中任何设备→主机帧都带 is_resp=1) */
                if (flags & FLAG_IS_REPORT) {

                    if (cmd == APP_CMD_FALL_ALERT) {
                        /* 摔倒警报命令 */
                        uint8_t fall_status = 0;
                        if (payload_len >= 5) {  /* flags(1)+cmd(1)+body(1)+CRC(2) */
                            fall_status = payload_buf[2];  /* body[0]: 0=正常 1=摔倒 */
                        }

                        /* [DEBUG_RGB] 区分真实摔倒 vs 状态上报 */
                        if (fall_status == 1) {
                            RGB_Set(RGB_RED);    /* 🔴 真实摔倒! */
                        } else {
                            RGB_Set(RGB_MAGENTA); /* 🟣 cmd匹配但status=0(正常) */
                        }

                        OpenmvEventType_t evt_type;
                        if (fall_status == 1) {
                            evt_type = OPENMV_EVT_FALL_DETECTED;
                        } else {
                            evt_type = OPENMV_EVT_FALL_CLEAR;
                        }

                        /* ISR 安全发送到队列 */
                        if (openmvQueueHandle != NULL) {
                            osMessagePut(openmvQueueHandle,
                                         (uint32_t)(evt_type << 16 | (fall_status & 0xFFFF)),
                                         0);
                        }
                    }
                    /* 其他自定义命令可在此扩展 */
                }
            }
            /* CRC 校验失败 — 静默丢弃该帧 */

            /* 准备接收下一帧 */
            proto_state = PROTO_SYNC;
            header_match_idx = 0;
        }
        break;
    }
}

/* ================================================================
   vOpenmvTask — MaixCAM 数据接收处理任务
   ================================================================ */
void vOpenmvTask(void const * argument)
{
    (void)argument;

    osEvent rx_event;
    uint32_t last_hb_ticks = xTaskGetTickCount();

    /* [DEBUG] PB0 按键防抖 */
    TickType_t last_btn_tick = 0;

    for (;;) {
        /* 阻塞等待事件（超时 2s 用于心跳超时检测） */
        rx_event = osMessageGet(openmvQueueHandle, pdMS_TO_TICKS(2000));

        /* ================================================================
           [DEBUG] PB0 按键手动触发摔倒 (低电平有效, 内部上拉)
           按一下 PB0 → 模拟摔倒 → 全链路测试 (PC11→PC6→AT)
           ================================================================ */
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
            TickType_t now = xTaskGetTickCount();
            /* 防抖: 距上次触发 > 1s */
            if ((now - last_btn_tick) > pdMS_TO_TICKS(1000)) {
                last_btn_tick = now;
                vTaskDelay(pdMS_TO_TICKS(30));  /* 消抖 */
                if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
                    /* [DEBUG_RGB] 红→绿: 手动触发摔倒! */
                    RGB_Set(RGB_RED);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    RGB_Set(RGB_GREEN);
                    Esp01_TriggerFallAlert();
                }
            }
        }

        if (rx_event.status == osEventMessage) {
            uint32_t payload  = (uint32_t)rx_event.value.v;
            OpenmvEventType_t type = (OpenmvEventType_t)((payload >> 16) & 0xFF);

            switch (type) {
            case OPENMV_EVT_FALL_DETECTED: {
                /* 防抖：距上次报警需超过冷却时间
                   (last_fall_tick==0 表示首次检测，允许立即触发) */
                TickType_t now = xTaskGetTickCount();
                if (last_fall_tick == 0 ||
                    (now - last_fall_tick) >= pdMS_TO_TICKS(FALL_ALERT_COOLDOWN)) {
                    last_fall_tick = now;

                    /* ★ 向 ESP-01 任务发送摔倒警报 (触发微信推送) ★ */
                    RGB_Set(RGB_GREEN);  /* [DEBUG_RGB] 绿灯: 摔倒事件已转发 */
                    Esp01_TriggerFallAlert();
                }
                break;
            }
            case OPENMV_EVT_FALL_CLEAR:
                /* [DEBUG_RGB] 青灯: 收到FALL_CLEAR (status=0) */
                RGB_Set(RGB_CYAN);
                /* 摔倒解除，允许下次检测 */
                break;

            case OPENMV_EVT_HEARTBEAT:
                last_hb_ticks = xTaskGetTickCount();
                break;

            case OPENMV_EVT_UNKNOWN:
            default:
                break;
            }
        } else {
            /* 2秒超时 — 检查 MaixCAM 是否离线 */
            TickType_t now = xTaskGetTickCount();
            if ((now - last_hb_ticks) > pdMS_TO_TICKS(30000)) {
                /* 30秒无心跳/数据, MaixCAM 可能离线 */
                /* (预留告警逻辑) */
            }
        }
    }
}

/* ================================================================
   Esp01_TriggerFallAlert — 向 ESP-01 任务发送摔倒警报
   (由 vOpenmvTask 调用)
   ================================================================ */
void Esp01_TriggerFallAlert(void)
{
    uint32_t evt = (uint32_t)ESP01_EVT_FALL_ALERT;

    if (fallAlertQueue != NULL) {
        xQueueSend(fallAlertQueue, &evt, 0);
    }
}

/* ================================================================
   OpenMV_RxCallback — ISR 调用的逐字节协议解析入口
   由 usart.c 中 HAL_UART_RxCpltCallback 分发调用
   ================================================================ */
void OpenMV_RxCallback(uint8_t byte)
{
    ParseProtoByte(byte);
}
