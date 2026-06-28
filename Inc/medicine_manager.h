/**
  ******************************************************************************
  * @file           : medicine_manager.h
  * @brief          : 药物管理系统接口定义
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026
  * All rights reserved.
  *
  ******************************************************************************
  */

#ifndef INC_MEDICINE_MANAGER_H_
#define INC_MEDICINE_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Includes ------------------------------------------------------------------*/
/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/**
  * @brief 服药状态枚举
  */
typedef enum {
  MED_STATE_IDLE = 0,       // 空闲状态
  MED_STATE_REMINDING,      // 提醒中
  MED_STATE_CONFIRMED,      // 已确认服药
  MED_STATE_MISSED,         // 漏服
  MED_STATE_SNOOZE          // 稍后提醒
} MedState_t;

/**
  * @brief 服药时间结构体
  */
typedef struct {
  uint8_t hour;             // 时
  uint8_t minute;           // 分
  char medicine_name[32];   // 药品名称
  uint8_t active;           // 是否激活
} MedicineTime_t;

/**
  * @brief 服药记录结构体
  */
typedef struct {
  uint32_t timestamp;       // 时间戳（秒）
  uint8_t reminder_idx;     // 提醒索引
  uint8_t confirm_method;   // 确认方式（0:按键，1:盒盖）
  uint8_t on_time;          // 是否按时（1:按时，0:迟到）
  uint8_t missed;           // 是否漏服
} MedicineRecord_t;

/* Exported constants --------------------------------------------------------*/

#define MAX_MED_TIMES 10      // 最大服药时间数量
#define MAX_LOG_RECORDS 50    // 最大记录数

/* 重定义兼容旧名称 */
#define MAX_MEDICINE_TIMES MAX_MED_TIMES
#define MAX_RECORDS MAX_LOG_RECORDS

/* Exported macro ------------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */

/**
 * @brief 初始化药物管理系统
 */
void MedicineManager_Init(void);

/**
 * @brief 获取当前设置的服药时间数量
 * @return 服药时间数量
 */
uint8_t MedicineManager_GetTimeCount(void);

/**
 * @brief 获取指定索引的服药时间
 * @param index 索引
 * @return 服药时间指针
 */
MedicineTime_t* MedicineManager_GetTime(uint8_t index);

/**
 * @brief 获取下一次最近的服药时间
 * @param hour 小时
 * @param minute 分钟
 * @param name 药品名称
 * @return 有下一个返回1，没有返回0
 */
uint8_t MedicineManager_GetNextMedicine(uint8_t* hour, uint8_t* minute, char* name);

/**
 * @brief 添加服药时间
 * @param hour 时
 * @param minute 分
 * @param name 药品名
 * @return 成功返回1，失败返回0
 */
uint8_t MedicineManager_AddTime(uint8_t hour, uint8_t minute, char* name);

/**
 * @brief 删除指定索引的服药时间
 * @param index 索引
 * @return 成功返回1，失败返回0
 */
uint8_t MedicineManager_RemoveTime(uint8_t index);

/**
 * @brief 获取当前时间
 * @param hour 时
 * @param minute 分
 * @param second 秒
 */
void MedicineManager_GetCurrentTime(uint8_t* hour, uint8_t* minute, uint8_t* second);

/**
 * @brief 设置当前时间
 * @param hour 时
 * @param minute 分
 * @param second 秒
 */
void MedicineManager_SetCurrentTime(uint8_t hour, uint8_t minute, uint8_t second);

/**
 * @brief 检查是否到达服药时间
 * @return 返回匹配的服药时间索引，如果没有返回-1
 */
int8_t MedicineManager_CheckTime(void);

/**
 * @brief 记录服药确认
 * @param idx 服药时间索引
 * @param confirm_method 确认方式（0:按键，1:盒盖检测）
 * @param on_time 是否按时
 */
void MedicineManager_RecordConfirmation(uint8_t idx, uint8_t confirm_method, uint8_t on_time);

/**
 * @brief 检查是否漏服
 * @param idx 服药时间索引
 * @return 漏服返回1，否则返回0
 */
uint8_t MedicineManager_IsMissed(uint8_t idx);

/**
 * @brief 获取服药统计信息
 * @param total 总次数
 * @param taken 已服用次数
 * @param missed 漏服次数
 */
void MedicineManager_GetStats(uint16_t* total, uint16_t* taken, uint16_t* missed);

/**
 * @brief 获取最近的服药记录
 * @param count 要获取的记录数
 * @return 记录数组
 */
MedicineRecord_t* MedicineManager_GetRecentRecords(uint8_t count);

/**
 * @brief 获取记录数量
 * @return 记录数量
 */
uint16_t MedicineManager_GetRecordCount(void);

/**
 * @brief 判断当前时间是否在指定服药时间的确认窗口内
 * @param idx 服药时间索引
 * @return 在窗口内返回1，否则返回0
 */
uint8_t MedicineManager_IsInWindow(uint8_t idx);

/**
 * @brief 设置确认窗口超时时间（秒）
 * @param seconds 超时秒数
 */
void MedicineManager_SetConfirmWindow(uint16_t seconds);

/**
 * @brief 重置指定索引的提醒标志
 * @param idx 索引
 */
void MedicineManager_ResetReminded(uint8_t idx);

/**
 * @brief 重置所有提醒标志
 */
void MedicineManager_ResetAllReminded(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_MEDICINE_MANAGER_H_ */
