/**
  ******************************************************************************
  * @file           : soft_i2c_ds3231.h
  * @brief          : DS3231软件模拟I2C驱动接口
  ******************************************************************************
  */

#ifndef INC_SOFT_I2C_DS3231_H_
#define INC_SOFT_I2C_DS3231_H_

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 软件I2C引脚定义 */
#define SOFT_I2C_SCL_PORT       GPIOB
#define SOFT_I2C_SCL_PIN        GPIO_PIN_10
#define SOFT_I2C_SDA_PORT       GPIOB
#define SOFT_I2C_SDA_PIN        GPIO_PIN_11

/* DS3231 I2C地址 */
#define DS3231_ADDR_W           0xD0
#define DS3231_ADDR_R           0xD1

/* DS3231寄存器 */
#define DS3231_REG_SECOND       0x00
#define DS3231_REG_MINUTE       0x01
#define DS3231_REG_HOUR         0x02
#define DS3231_REG_DAY          0x03
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH        0x05
#define DS3231_REG_YEAR         0x06
#define DS3231_REG_ALM1_SEC     0x07
#define DS3231_REG_ALM1_MIN     0x08
#define DS3231_REG_ALM1_HOUR    0x09
#define DS3231_REG_ALM1_DATE    0x0A
#define DS3231_REG_CONTROL      0x0E
#define DS3231_REG_STATUS       0x0F
#define DS3231_REG_AGING        0x10

/* DS3231 控制寄存器位 */
#define DS3231_EOSC             (1 << 7)   /* Enable Oscillator: 0=run */
#define DS3231_INTCN            (1 << 2)   /* 方波/中断选择: 1=中断 */
#define DS3231_A1IE             (1 << 0)   /* Alarm 1 中断使能 */
#define DS3231_OSF              (1 << 7)   /* 晶振停振标志 */
#define DS3231_A1F              (1 << 0)   /* Alarm 1 标志位 */

/* RTC时间结构体（十进制值） */
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} RTC_TimeStruct;

/* 软件I2C基本函数 */
void SoftI2C_Init(void);
void SoftI2C_Start(void);
void SoftI2C_Stop(void);
uint8_t SoftI2C_WaitAck(void);
void SoftI2C_SendAck(void);
void SoftI2C_SendNack(void);
void SoftI2C_WriteByte(uint8_t data);
uint8_t SoftI2C_ReadByte(uint8_t ack);

/* DS3231 Alarm1 读回校验值（DS3231_SetAlarm1内部填充） */
extern uint8_t g_readback_hour;
extern uint8_t g_readback_min;

/* DS3231功能函数 */
uint8_t DS3231_ReadTime(RTC_TimeStruct *time);
uint8_t DS3231_SetTime(RTC_TimeStruct *time);
uint8_t DS3231_SetAlarm1(uint8_t hour, uint8_t minute);
uint8_t DS3231_ClearAlarmFlag(void);
uint8_t DS3231_SetAgingOffset(int8_t offset);
uint8_t DS3231_CheckOSF(void);
uint8_t DS3231_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_SOFT_I2C_DS3231_H_ */
