/**
  ******************************************************************************
  * @file           : rtc_driver.c
  * @brief          : RTC驱动实现
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026
  * All rights reserved.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "rtc_driver.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;

/* DS3231 RTC寄存器地址 */
#define DS3231_ADDR           0xD0  // DS3231的I2C地址
#define DS3231_SECOND_REG     0x00
#define DS3231_MINUTE_REG     0x01
#define DS3231_HOUR_REG       0x02
#define DS3231_DAY_REG        0x03
#define DS3231_DATE_REG       0x04
#define DS3231_MONTH_REG      0x05
#define DS3231_YEAR_REG       0x06

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief 将BCD格式转换为十进制
 * @param bcd BCD编码的值
 * @return 十进制值
 */
static uint8_t bcd_to_decimal(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * @brief 将十进制转换为BCD格式
 * @param decimal 十进制值
 * @return BCD编码的值
 */
static uint8_t decimal_to_bcd(uint8_t decimal)
{
    return ((decimal / 10) << 4) + (decimal % 10);
}

/**
 * @brief 读取RTC时间
 * @param time RTC时间结构体
 * @return 成功返回HAL_OK，失败返回错误代码
 */
HAL_StatusTypeDef RTC_ReadTime(RTC_TimeStruct *time)
{
    uint8_t data[7];
    HAL_StatusTypeDef status;
    
    // 读取7个时间寄存器
    status = HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDR, DS3231_SECOND_REG, 1, data, 7, 100);
    
    if(status == HAL_OK) {
        time->seconds = bcd_to_decimal(data[0]);
        time->minutes = bcd_to_decimal(data[1]);
        time->hours = bcd_to_decimal(data[2] & 0x3F); // 忽略12/24小时标志位
        
        // 跳过星期和日期字段，因为我们只需要时间
        time->day = bcd_to_decimal(data[4]);
        time->month = bcd_to_decimal(data[5] & 0x7F); // 忽略世纪位
        time->year = bcd_to_decimal(data[6]) + 2000; // 年份从2000年开始计算
    }
    
    return status;
}

/**
 * @brief 设置RTC时间
 * @param time RTC时间结构体
 * @return 成功返回HAL_OK，失败返回错误代码
 */
HAL_StatusTypeDef RTC_SetTime(RTC_TimeStruct *time)
{
    uint8_t data[8];
    
    // 设置寄存器起始地址
    data[0] = DS3231_SECOND_REG;
    data[1] = decimal_to_bcd(time->seconds);
    data[2] = decimal_to_bcd(time->minutes);
    data[3] = decimal_to_bcd(time->hours);
    data[4] = decimal_to_bcd(1); // 星期，固定为1
    data[5] = decimal_to_bcd(time->day);
    data[6] = decimal_to_bcd(time->month);
    data[7] = decimal_to_bcd(time->year - 2000);
    
    return HAL_I2C_Master_Transmit(&hi2c1, DS3231_ADDR, data, 8, 100);
}

/**
 * @brief 初始化RTC
 * @return 成功返回HAL_OK，失败返回错误代码
 *
 * 双重保险恢复时间：
 * 1. OSF=1（晶振曾停振）→ 无条件写默认时间
 * 2. OSF=0 但时间寄存器是垃圾（年份<2024）→ 也能检测并恢复
 * 后者覆盖了"备份电池接触不良导致时间归零但OSF未被置位"的边缘情况。
 */
HAL_StatusTypeDef RTC_Init(void)
{
    uint8_t status_reg;
    HAL_StatusTypeDef status;
    int time_bad = 0;

    status = HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDR, 0x0F, 1, &status_reg, 1, 100);

    if (status == HAL_OK) {
        /* 检查并清除 OSF */
        if (status_reg & 0x80) {
            status_reg &= ~0x80;
            HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDR, 0x0F, 1, &status_reg, 1, 100);
            time_bad = 1;  /* 晶振曾停，时间必然不可信 */
        }

        /* 即使 OSF=0，也要验证时间寄存器是否合理 */
        if (!time_bad) {
            RTC_TimeStruct check;
            if (RTC_ReadTime(&check) == HAL_OK) {
                if (check.year < 2024 || check.year > 2099) {
                    time_bad = 1;  /* 年份是垃圾值 → 备份电池失效过 */
                }
            } else {
                time_bad = 1;      /* 连读都读不出来 */
            }
        }

        /* 时间无效 → 写入编译默认值 */
        if (time_bad) {
            RTC_TimeStruct default_time = {
                .seconds = 0,
                .minutes = 5,
                .hours = 17,
                .day = 23,
                .month = 5,
                .year = 2026
            };
            status = RTC_SetTime(&default_time);
        }
    }

    return status;
}
