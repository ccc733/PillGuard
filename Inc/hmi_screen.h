/**
  ******************************************************************************
  * @file           : hmi_screen.h
  * @brief          : TJC串口屏驱动 + 0x55协议帧解析器接口
  ******************************************************************************
  */

#ifndef INC_HMI_SCREEN_H_
#define INC_HMI_SCREEN_H_

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TJC串口屏协议终止符 */
#define TJC_END                 "\xff\xff\xff"

/* 串口屏页面名称 */
#define HMI_PAGE_BOOTING        "booting"
#define HMI_PAGE_STANDBY        "standby"
#define HMI_PAGE_PAGE1          "page1"
#define HMI_PAGE_REMINDING      "reminding"
#define HMI_PAGE_CONFIRMED      "confirmed"
#define HMI_PAGE_MISSED         "missed"
#define HMI_PAGE_SNOOZE         "snooze"

/* 药品类型 — b11/b22按钮下发的服药类别 */
#define MED_TYPE_BP             0x01   /* 降压药 → LED1绿灯亮 */
#define MED_TYPE_SUGAR          0x02   /* 血糖药 → LED2绿灯亮 */

/* 语音事件ID（经队列传递） */
#define VOICE_EVT_REMIND        1      /* "该吃药了吃两颗" */
#define VOICE_EVT_CONFIRM       2      /* "已服药祝您身体健康" */
#define VOICE_EVT_WARN          3      /* "记得服药"×3 */

/* HMI接收帧数据（ISR中填充） */
extern uint8_t  hmi_alarm_data[2];   /* [0]=hour, [1]=minute */
extern uint8_t  hmi_frame_cmd;       /* 帧命令词: 0x02=设闹钟, 0x03=确认服药 */
extern volatile uint8_t g_screen_confirmed;  /* 串口屏确认服药标志 */
extern volatile uint8_t g_snooze_pressed;    /* missed页面b111按钮按下 → snooze */
extern volatile uint8_t g_medicine_type;     /* 当前服药类型: MED_TYPE_BP/MED_TYPE_SUGAR */
extern volatile uint8_t g_led_trigger;       /* LED触发: bit0=降压药LED, bit1=血糖药LED */

/* HMI发送函数（线程安全，UART1互斥锁保护） */
void HMI_SendPage(const char *page);
void HMI_SendRaw(const char *cmd);
void HMI_SetText(const char *widget, const char *fmt, ...);

/* HMI接收解析器（在UART ISR中逐字节调用） */
void HMI_ParseByte(uint8_t byte);

/* HMI初始化 */
void HMI_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_HMI_SCREEN_H_ */
