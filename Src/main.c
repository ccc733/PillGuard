/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 智能服药系统 — FreeRTOS + FSM 混合架构
  *                   PB12 = DS3231 SQW/INT (EXTI, falling, pull-up)
  *                   PF3  = 49E霍尔传感器 (ADC3_IN9, analog)
  *                   PB3  = 已弃用
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "soft_i2c_ds3231.h"
#include "hmi_screen.h"
#include "hall_sensor.h"
#include "lu6288_voice.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "cmsis_os.h"

#include "debug_rgb.h"

#include <stdio.h>
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* 系统状态枚举 */
typedef enum {
    SYS_INIT,
    SYS_STANDBY,
    SYS_REMINDING,
    SYS_CONFIRMED,
    SYS_MISSED,
    SYS_SNOOZE
} SysState_t;

/* Private define ------------------------------------------------------------*/
#define REMINDER_TIMEOUT_MS     300000U     /* 5分钟提醒超时 */
#define MISSED_TIMEOUT_MS       30000U      /* 未服药页面30秒自动返回 */
#define AGING_OFFSET            0           /* DS3231老化校准值（正=变慢, 负=变快）*/

/* Private variables ---------------------------------------------------------*/

/* 全局时间（由vSensorTask更新） */
uint8_t  current_hour   = 0;
uint8_t  current_minute = 0;
uint8_t  current_second = 0;

/* 系统当前状态（vSensorTask读取以决定是否更新屏幕） */
volatile SysState_t g_sys_state = SYS_INIT;

/* 硬件中断标志 */
volatile uint8_t g_alarm_triggered = 0;   /* PB12 SQW中断置1 */
volatile uint8_t is_box_opened     = 0;   /* 开盖锁存标志（ADC轮询Hall_GetState置位），仅在REMINDING期间有效 */

/* 外部引用 */
extern uint8_t uart2_rx_buffer[];  /* usart.c 屏幕接收缓冲区 (USART2) */

/* IPC句柄 */
SemaphoreHandle_t xHmiRxSemaphore = NULL;
QueueHandle_t     xVoiceQueue     = NULL;

extern volatile uint8_t g_hmi_diag_code;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
void vMainHmiTask(void const * argument);
void vVoiceTask(void const * argument);
void vSensorTask(void const * argument);
void MX_FREERTOS_Init(void);

static void HMI_SendRemindingDiag(const char *text)
{
    char clipped[16];
    char cmd[40];
    size_t len = strlen(text);

    if (len > 15U) {
        len = 15U;
    }
    memcpy(clipped, text, len);
    clipped[len] = '\0';

    snprintf(cmd, sizeof(cmd), "reminding.t1.txt=\"%s\"", clipped);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), 100);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\xFF\xFF\xFF", 3, 100);
}

static void ResetReminderRuntimeFlags(void)
{
    g_alarm_triggered = 0;
    is_box_opened = 0;
    g_screen_confirmed = 0;
    g_snooze_pressed = 0;
    hmi_frame_cmd = 0;
}

/* 熄灭两个RGB LED绿灯，复位LED状态 */
static void LED_BothOff(void)
{
    HAL_GPIO_WritePin(LED_BP_PORT, LED_BP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_SUGAR_PORT, LED_SUGAR_PIN, GPIO_PIN_RESET);
    g_led_trigger = 0;
}

static BaseType_t SendVoiceEvent(uint32_t evt)
{
    return xQueueSend(xVoiceQueue, &evt, 0);
}

void vMainHmiTask(void const * argument)
{
    (void)argument;

    SysState_t state = SYS_INIT;
    uint8_t state_enter = 1;
    uint8_t reminding_lid_diag = 0;
    uint16_t reminding_loop_cnt = 0;
    uint8_t reminding_hold_diag = 0;
    TickType_t remind_start_tick = 0;
    TickType_t remind_diag_tick = 0;
    TickType_t state_start_tick = 0;

    for (;;)
    {
        uint8_t skip_delay = 0;

        switch (state)
        {
        case SYS_INIT:
            if (state_enter) {
                state_enter = 0;

                if (xHmiRxSemaphore == NULL) {
                    xHmiRxSemaphore = xSemaphoreCreateBinary();
                }

                __HAL_UART_CLEAR_FLAG(&huart2,
                    UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
                HAL_UART_Receive_IT(&huart2, &uart2_rx_buffer[0], 1);

                SoftI2C_Init();
                {
                    uint8_t osf_was_set = DS3231_CheckOSF();
                    DS3231_Init();
                    if (osf_was_set) {
                        /* 仅当 OSF 被设置（电池曾失效）时才写入默认时间 */
                        RTC_TimeStruct init_time = {
                            .seconds = 0, .minutes = 15, .hours = 16,
                            .day = 1, .month = 6, .year = 2026
                        };
                        DS3231_SetTime(&init_time);
                    }
                }
                DS3231_SetAgingOffset(AGING_OFFSET);

                HMI_SendPage(HMI_PAGE_BOOTING);
                state_start_tick = xTaskGetTickCount();
            }

            if ((xTaskGetTickCount() - state_start_tick) >= pdMS_TO_TICKS(2000U)) {
                HMI_SendPage(HMI_PAGE_STANDBY);
                HMI_SendRaw("standby.t_beside.txt=\"INIT OK\"");

                ResetReminderRuntimeFlags();
                g_sys_state = SYS_STANDBY;
                state = SYS_STANDBY;
                state_enter = 1;
            }
            break;

        case SYS_STANDBY:
            if (state_enter) {
                state_enter = 0;
                g_sys_state = SYS_STANDBY;
            }

            if (xSemaphoreTake(xHmiRxSemaphore, 0) == pdTRUE) {
                if (hmi_frame_cmd == 0x02 ||
                    hmi_frame_cmd == MED_TYPE_BP ||
                    hmi_frame_cmd == MED_TYPE_SUGAR) {
                    uint8_t hour = hmi_alarm_data[0];
                    uint8_t minute = hmi_alarm_data[1];
                    uint8_t med_type = hmi_frame_cmd;  /* 保存药品类型 */
                    uint8_t ret = DS3231_SetAlarm1(hour, minute);
                    char rb_buf[48];

                    /* 回显时带上药品类型 */
                    snprintf(rb_buf, sizeof(rb_buf),
                             "standby.t_beside.txt=\"OK:%02d:%02d T%d\"",
                             g_readback_hour, g_readback_min, med_type);
                    HMI_SendRaw(rb_buf);
                    (void)ret;
                }
                hmi_frame_cmd = 0;
                g_screen_confirmed = 0;
            }

            if (g_alarm_triggered) {
                state = SYS_REMINDING;
                state_enter = 1;
                skip_delay = 1;
            }
            break;

        case SYS_REMINDING:
            if (state_enter) {
                state_enter = 0;
                remind_start_tick = xTaskGetTickCount();
                remind_diag_tick = remind_start_tick;
                reminding_lid_diag = 0;
                reminding_loop_cnt = 0;
                reminding_hold_diag = 0;

                ResetReminderRuntimeFlags();
                g_sys_state = SYS_REMINDING;

                /* 根据药品类型点亮对应LED绿灯 */
                if (g_medicine_type == MED_TYPE_BP) {
                    HAL_GPIO_WritePin(LED_BP_PORT, LED_BP_PIN, GPIO_PIN_SET);
                    g_led_trigger = 1;   /* bit0: 降压药LED亮 */
                } else if (g_medicine_type == MED_TYPE_SUGAR) {
                    HAL_GPIO_WritePin(LED_SUGAR_PORT, LED_SUGAR_PIN, GPIO_PIN_SET);
                    g_led_trigger = 2;   /* bit1: 血糖药LED亮 */
                }

                HMI_SendPage(HMI_PAGE_REMINDING);
                HMI_SendRemindingDiag("Waiting");
                osDelay(80);
                HMI_SendRemindingDiag("VOICE OFF");
                (void)SendVoiceEvent(VOICE_EVT_REMIND);  /* LU6288播报"该吃药了吃两颗" */
            }

            if (g_hmi_diag_code != 0U) {
                uint8_t diag = g_hmi_diag_code;
                g_hmi_diag_code = 0;

                if (diag == 1U) {
                    HMI_SendRemindingDiag("RX55");
                    reminding_hold_diag = 50;
                } else if (diag == 2U) {
                    HMI_SendRemindingDiag("RX B0");
                    reminding_hold_diag = 50;
                    osDelay(80);
                } else if (diag == 3U) {
                    HMI_SendRemindingDiag("RX ALARM");
                    reminding_hold_diag = 50;
                } else if (diag == 5U) {
                    /* 药品类型闹钟: 显示降压药/血糖药 */
                    if (g_medicine_type == MED_TYPE_BP) {
                        HMI_SendRemindingDiag("MED: BP");
                    } else if (g_medicine_type == MED_TYPE_SUGAR) {
                        HMI_SendRemindingDiag("MED: SUGAR");
                    }
                    reminding_hold_diag = 50;
                }
            }

            reminding_loop_cnt++;
            if (reminding_hold_diag > 0U) {
                reminding_hold_diag--;
            } else if (reminding_loop_cnt >= 100U) {
                char loop_msg[16];
                reminding_loop_cnt = 0;
                snprintf(loop_msg, sizeof(loop_msg), "LOOP %u",
                         (unsigned int)(xTaskGetTickCount() & 0xFFU));
                HMI_SendRemindingDiag(loop_msg);
            }

            if (is_box_opened && reminding_lid_diag == 0U) {
                reminding_lid_diag = 1;
                HMI_SendRemindingDiag("LID IRQ");
            }

            /* PF3 ADC3_IN9轮询：检测到霍尔触发则锁存开盖标志 */
            if (!is_box_opened && Hall_GetState() == HALL_TRIGGERED) {
                is_box_opened = 1;
            }

            if (xSemaphoreTake(xHmiRxSemaphore, 0) == pdTRUE) {
                HMI_SendRemindingDiag("SEM TAKE");
                osDelay(80);

                if (g_screen_confirmed) {
                    uint16_t hall_adc = Hall_GetAdc();

                    HMI_SendRemindingDiag("B0 FLAG");
                    osDelay(80);
                    {
                        char adc_msg[16];
                        snprintf(adc_msg, sizeof(adc_msg), "ADC %u", hall_adc);
                        HMI_SendRemindingDiag(adc_msg);
                    }
                    osDelay(80);
                    HMI_SendRemindingDiag(is_box_opened ? "LATCH 1" : "LATCH 0");
                    osDelay(80);

                    if (is_box_opened) {
                        HMI_SendRemindingDiag("GO CONF");
                        osDelay(120);
                        (void)SendVoiceEvent(VOICE_EVT_CONFIRM);
                        LED_BothOff();   /* 服药确认，熄灭LED */
                        DS3231_ClearAlarmFlag();
                        HMI_SendPage(HMI_PAGE_CONFIRMED);
                        ResetReminderRuntimeFlags();
                        g_sys_state = SYS_CONFIRMED;
                        state = SYS_CONFIRMED;
                        state_enter = 1;
                        break;
                    }

                    g_screen_confirmed = 0;
                    hmi_frame_cmd = 0;
                    HMI_SendRemindingDiag("NO LID");
                } else {
                    HMI_SendRemindingDiag("NO B0 FLAG");
                    hmi_frame_cmd = 0;
                }
            }

            if ((xTaskGetTickCount() - remind_start_tick) >= pdMS_TO_TICKS(REMINDER_TIMEOUT_MS)) {
                HMI_SendRemindingDiag("Timeout Err");
                LED_BothOff();   /* 超时未服药，熄灭LED */
                DS3231_ClearAlarmFlag();
                HMI_SendPage(HMI_PAGE_MISSED);
                ResetReminderRuntimeFlags();
                g_sys_state = SYS_MISSED;
                state = SYS_MISSED;
                state_enter = 1;
                break;
            }

            if ((xTaskGetTickCount() - remind_diag_tick) >= pdMS_TO_TICKS(500U)) {
                char hb[16];
                remind_diag_tick = xTaskGetTickCount();
                snprintf(hb, sizeof(hb), "H%lu",
                         (unsigned long)((xTaskGetTickCount() - remind_start_tick) / pdMS_TO_TICKS(500U)));
                HMI_SendRemindingDiag(hb);
            }
            break;

        case SYS_CONFIRMED:
            if (state_enter) {
                state_enter = 0;
                state_start_tick = xTaskGetTickCount();
                g_sys_state = SYS_CONFIRMED;
            }

            if ((xTaskGetTickCount() - state_start_tick) >= pdMS_TO_TICKS(5000U)) {
                HMI_SendPage(HMI_PAGE_STANDBY);
                g_sys_state = SYS_STANDBY;
                state = SYS_STANDBY;
                state_enter = 1;
            }
            break;

        case SYS_MISSED:
            if (state_enter) {
                state_enter = 0;
                state_start_tick = xTaskGetTickCount();
                g_sys_state = SYS_MISSED;
                (void)SendVoiceEvent(VOICE_EVT_WARN);
            }

            if (xSemaphoreTake(xHmiRxSemaphore, 0) == pdTRUE) {
                if (g_snooze_pressed) {
                    /* b111按钮 → 跳转snooze页面 */
                    g_snooze_pressed = 0;
                    hmi_frame_cmd = 0;
                    g_screen_confirmed = 0;
                    LED_BothOff();   /* 稍后提醒，熄灭LED */
                    HMI_SendPage(HMI_PAGE_SNOOZE);
                    g_sys_state = SYS_SNOOZE;
                    state = SYS_SNOOZE;
                    state_enter = 1;
                    break;
                }
                hmi_frame_cmd = 0;
                g_screen_confirmed = 0;
            }

            if ((xTaskGetTickCount() - state_start_tick) >= pdMS_TO_TICKS(MISSED_TIMEOUT_MS)) {
                HMI_SendPage(HMI_PAGE_STANDBY);
                g_sys_state = SYS_STANDBY;
                state = SYS_STANDBY;
                state_enter = 1;
            }
            break;

        case SYS_SNOOZE:
            if (state_enter) {
                state_enter = 0;
                state_start_tick = xTaskGetTickCount();
                g_sys_state = SYS_SNOOZE;
            }

            /* 停留5秒自动返回standby */
            if ((xTaskGetTickCount() - state_start_tick) >= pdMS_TO_TICKS(5000U)) {
                HMI_SendPage(HMI_PAGE_STANDBY);
                g_sys_state = SYS_STANDBY;
                state = SYS_STANDBY;
                state_enter = 1;
            }
            break;

        default:
            g_sys_state = SYS_STANDBY;
            state = SYS_STANDBY;
            state_enter = 1;
            break;
        }

        if (skip_delay == 0U) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

#if 0
/**
  * @brief 主HMI任务 — 运行业务状态机
  *        权限：唯一写 g_sys_state 的任务
  *              SYS_REMINDING 子循环以 xSemaphoreTake(50ms) 为节拍器
  */
static void vMainHmiTask_old(void const * argument)
{
    (void)argument;

    SysState_t state = SYS_INIT;
    uint32_t   remind_start_tick = 0;

    for (;;)
    {
        switch (state)
        {
        /* ================================================
           SYS_INIT: 初始化I2C → booting → 2s → standby
           ================================================ */
        case SYS_INIT:
            /* 防线1: 信号量空指针兜底 */
            if (xHmiRxSemaphore == NULL) {
                xHmiRxSemaphore = xSemaphoreCreateBinary();
            }

            /* 防线2: 清除UART残留错误标志 */
            __HAL_UART_CLEAR_FLAG(&huart2,
                UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);

            /* 防线3: 无条件重开UART RX中断 */
            HAL_UART_Receive_IT(&huart2, &uart2_rx_buffer[0], 1);

            /* 初始化DS3231 RTC */
            SoftI2C_Init();
            {
                uint8_t osf_was_set = DS3231_CheckOSF();
                DS3231_Init();
                if (osf_was_set) {
                    RTC_TimeStruct init_time = {
                        .seconds = 0, .minutes = 15, .hours = 19,
                        .day = 26, .month = 5, .year = 2026
                    };
                    DS3231_SetTime(&init_time);
                }
            }
            DS3231_SetAgingOffset(AGING_OFFSET);

            HMI_SendPage(HMI_PAGE_BOOTING);
            vTaskDelay(pdMS_TO_TICKS(2000));
            HMI_SendPage(HMI_PAGE_STANDBY);
            HMI_SendRaw("standby.t_beside.txt=\"INIT OK\"");

            g_sys_state = SYS_STANDBY;
            state = SYS_STANDBY;
            break;

        /* ================================================
           SYS_STANDBY: 等待HMI指令 或 闹钟中断
           ================================================ */
        case SYS_STANDBY:
            /* 带超时阻塞等待串口屏下发帧 */
            if (xSemaphoreTake(xHmiRxSemaphore, pdMS_TO_TICKS(200)) == pdTRUE) {
                if (hmi_frame_cmd == 0x02) {
                    uint8_t hour   = hmi_alarm_data[0];
                    uint8_t minute = hmi_alarm_data[1];
                    uint8_t ret = DS3231_SetAlarm1(hour, minute);
                    {
                        char rb_buf[48];
                        snprintf(rb_buf, sizeof(rb_buf),
                                 "standby.t_beside.txt=\"OK:%02d:%02d\"",
                                 g_readback_hour, g_readback_min);
                        HMI_SendRaw(rb_buf);
                    }
                    (void)ret;
                }
                hmi_frame_cmd = 0;
            }

            /* 闹钟触发：先切状态再TX，阻止vSensorTask并发写UART */
            if (g_alarm_triggered) {
                g_alarm_triggered = 0;
                is_box_opened = 0;
                g_sys_state = SYS_REMINDING;
                remind_start_tick = HAL_GetTick();
                HMI_SendPage(HMI_PAGE_REMINDING);
                state = SYS_REMINDING;
            }
            break;

        /* ================================================
           SYS_REMINDING: 到点提醒 → 开盖+屏幕确认 / 超时
           子循环以 xSemaphoreTake(50ms) 为唯一节拍器
           ================================================ */
        case SYS_REMINDING:
            {
                uint32_t evt = VOICE_EVT_REMIND;
                xQueueSend(xVoiceQueue, &evt, 0);
            }

            {
                uint32_t hb_tick = HAL_GetTick();
                uint32_t hb_cnt  = 0;

                while (g_sys_state == SYS_REMINDING)
                {
                    /*
                     * 信号量即节拍器：
                     * - 50ms内有信号量 → 消费帧，检查确认门控
                     * - 50ms超时      → 心跳 + 5分钟超时检测
                     */
                    if (xSemaphoreTake(xHmiRxSemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
                        /* 确认服药门控：屏幕确认 + 开盖检测 缺一不可 */
                        if (g_screen_confirmed && is_box_opened) {
                            is_box_opened = 0;
                            g_screen_confirmed = 0;
                            hmi_frame_cmd = 0;
                            DS3231_ClearAlarmFlag();
                            HMI_SendPage(HMI_PAGE_CONFIRMED);
                            g_sys_state = SYS_CONFIRMED;
                            state = SYS_CONFIRMED;
                            break;
                        }
                        if (g_screen_confirmed) {
                            /* 屏幕点了确认但没开盖：拒绝并提示 */
                            g_screen_confirmed = 0;
                            HMI_SendRaw("reminding.t1.txt=\"NO\"");
                        }
                        hmi_frame_cmd = 0;
                    }

                    /* 5分钟超时 → MISSED */
                    if ((HAL_GetTick() - remind_start_tick) >= REMINDER_TIMEOUT_MS) {
                        DS3231_ClearAlarmFlag();
                        HMI_SendPage(HMI_PAGE_MISSED);
                        g_sys_state = SYS_MISSED;
                        state = SYS_MISSED;
                        break;
                    }

                    /* 每500ms心跳，确认子循环存活 */
                    if ((HAL_GetTick() - hb_tick) >= 500) {
                        hb_tick = HAL_GetTick();
                        hb_cnt++;
                        char hb[32];
                        snprintf(hb, sizeof(hb),
                                 "reminding.t1.txt=\"H%d\"", (int)hb_cnt);
                        HMI_SendRaw(hb);
                    }
                }
            }
            break;

        /* ================================================
           SYS_CONFIRMED: 已服药 → 停留5秒 → STANDBY
           ================================================ */
        case SYS_CONFIRMED:
            {
                uint32_t evt = VOICE_EVT_CONFIRM;
                xQueueSend(xVoiceQueue, &evt, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
            HMI_SendPage(HMI_PAGE_STANDBY);
            g_sys_state = SYS_STANDBY;
            state = SYS_STANDBY;
            break;

        /* ================================================
           SYS_MISSED: 超时未服药 → 30秒超时自动返回 STANDBY
           ================================================ */
        case SYS_MISSED:
            {
                uint32_t evt = VOICE_EVT_WARN;
                xQueueSend(xVoiceQueue, &evt, 0);
            }
            {
                uint32_t missed_start = HAL_GetTick();
                while ((HAL_GetTick() - missed_start) < MISSED_TIMEOUT_MS) {
                    if (xSemaphoreTake(xHmiRxSemaphore, pdMS_TO_TICKS(200)) == pdTRUE) {
                        hmi_frame_cmd = 0;
                    }
                }
            }
            HMI_SendPage(HMI_PAGE_STANDBY);
            g_sys_state = SYS_STANDBY;
            state = SYS_STANDBY;
            break;

        default:
            state = SYS_STANDBY;
            break;
        }
    }
}

/**
  * @brief 语音任务 — LU6288语音模块框架（预留）
  */
#endif

void vVoiceTask(void const * argument)
{
    (void)argument;

    uint32_t event_id;

    for (;;)
    {
        if (xQueueReceive(xVoiceQueue, &event_id, portMAX_DELAY) == pdTRUE) {
            switch (event_id) {
            case VOICE_EVT_REMIND:
                /* 闹钟触发 → 播报"该用药了" (GBK编码) */
                LU6288_SendVoice(VOICE_GBK_REMIND, VOICE_GBK_REMIND_LEN);
                break;
            case VOICE_EVT_CONFIRM:
                /* 用户确认服药 → 播报"已服药祝您身体健康" (GBK编码) */
                LU6288_SendVoice(VOICE_GBK_CONFIRM, VOICE_GBK_CONFIRM_LEN);
                break;
            case VOICE_EVT_WARN:
                /* 5分钟超时未服药 → 播报"记得服药"×3 (GBK编码) */
                LU6288_SendVoice(VOICE_GBK_WARN, VOICE_GBK_WARN_LEN);
                break;
            default:
                break;
            }
        }
    }
}

/**
  * @brief 传感器任务 — 每500ms读DS3231时间
  *        契约：g_sys_state 为只读，vSensorTask 永远不写入
  *        仅在 SYS_STANDBY 状态下更新 t0 控件
  */
void vSensorTask(void const * argument)
{
    (void)argument;

    RTC_TimeStruct rtc_time;
    extern osMutexId uart1MutexHandle;
    uint32_t loop_cnt = 0;

    for (;;)
    {
        int rtc_ok;
        loop_cnt++;
        taskENTER_CRITICAL();
        rtc_ok = (DS3231_ReadTime(&rtc_time) == 0);
        taskEXIT_CRITICAL();
        if (rtc_ok) {
            current_hour   = rtc_time.hours;
            current_minute = rtc_time.minutes;
            current_second = rtc_time.seconds;

            /* debug: show RTC read + state on t_beside every 2s (4 cycles) */
            if ((loop_cnt & 0x03U) == 0U) {
                char dbg[64];
                snprintf(dbg, sizeof(dbg),
                         "standby.t_beside.txt=\"%02d:%02d:%02d S%d\"",
                         current_hour, current_minute, current_second,
                         (int)g_sys_state);
                if (osMutexWait(uart1MutexHandle, osWaitForever) == osOK) {
                    HAL_UART_Transmit(&huart2, (uint8_t *)dbg,
                                      (uint16_t)strlen(dbg), 100);
                    HAL_UART_Transmit(&huart2,
                                      (uint8_t *)"\xFF\xFF\xFF", 3, 100);
                    osMutexRelease(uart1MutexHandle);
                }
            }

            if (g_sys_state == SYS_STANDBY) {
                char cmd[48];
                int len = snprintf(cmd, sizeof(cmd),
                                   "standby.t0.txt=\"%02d:%02d:%02d\"",
                                   current_hour, current_minute, current_second);
                if (osMutexWait(uart1MutexHandle, osWaitForever) == osOK) {
                    HAL_UART_Transmit(&huart2, (uint8_t *)cmd,
                                      (uint16_t)len, 100);
                    HAL_UART_Transmit(&huart2,
                                      (uint8_t *)"\xFF\xFF\xFF", 3, 100);
                    osMutexRelease(uart1MutexHandle);
                }
            }
        } else {
            /* RTC read failed — show error on t_beside */
            if ((loop_cnt & 0x03U) == 0U) {
                char dbg[32];
                snprintf(dbg, sizeof(dbg),
                         "standby.t_beside.txt=\"RTC ERR\"");
                if (osMutexWait(uart1MutexHandle, osWaitForever) == osOK) {
                    HAL_UART_Transmit(&huart2, (uint8_t *)dbg,
                                      (uint16_t)strlen(dbg), 100);
                    HAL_UART_Transmit(&huart2,
                                      (uint8_t *)"\xFF\xFF\xFF", 3, 100);
                    osMutexRelease(uart1MutexHandle);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
  * @brief EXTI 中断回调
  *        PB12 = DS3231 SQW/INT（闹钟触发）
  *        PB3  = 已弃用（原按键KEY，不再处理）
  *        PB4  = 已迁移至PF3 ADC3_IN9（不再使用EXTI）
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_12) {
        /* DS3231 SQW 闹钟触发 — 直接置位 */
        g_alarm_triggered = 1;
    }

    /* PB4 霍尔已迁移至 PF3 ADC3_IN9，不再使用EXTI */
    /* PB3 (GPIO_PIN_3) 已弃用，不再处理 */
}

/**
  * @brief  The application entry point.
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* 纯硬件初始化（无阻塞调用，无外设通信） */
    MX_GPIO_Init();
    RGB_Init();                 /* RGB调试灯: PA5=R, PA6=G, PA7=B */
    RGB_Flash(RGB_WHITE, 300);  /* 白灯闪一下 = 上电OK */
    Hall_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_OPENMV_UART_Init();    /* USART3: OpenMV H7 Plus 摔倒检测 */
    MX_ESP01_UART_Init();     /* USART6: ATK-ESP-01 WiFi 模块 */
    HMI_Init();

    /* 创建任务 + IPC，然后启动调度器 */
    MX_FREERTOS_Init();

    /* ====== [探针] 启动前最后一道防线 ====== */
    if (xHmiRxSemaphore == NULL) {
        xHmiRxSemaphore = xSemaphoreCreateBinary();
    }
    __HAL_UART_CLEAR_FLAG(&huart2,
        UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
    HAL_UART_Receive_IT(&huart2, &uart2_rx_buffer[0], 1);
    /* ===================================== */

    osKernelStart();

    while (1) {}
}

/**
  * @brief System Clock Configuration
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE=8MHz → PLLM=8 / PLLN=336 / PLLP=2 → SYSCLK=168MHz (F407 max) */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        /* HSE failed — fallback to HSI 16MHz */
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
            Error_Handler();
        }

        RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                    | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
        RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
        RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
        RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
        if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
            Error_Handler();
        }
        return;
    }

    /* PLL as system clock: 168MHz, Flash=5WS (150-168MHz range) */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(120);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(120);
    }
}
