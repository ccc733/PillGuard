/**
  ******************************************************************************
  * @file           : medicine_manager.c
  * @brief          : 药物管理系统实现
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026
  * All rights reserved.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "medicine_manager.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static MedicineTime_t med_times_local[MAX_MED_TIMES];
static uint8_t med_times_count_local = 0;
static MedicineRecord_t records_local[MAX_LOG_RECORDS];
static uint16_t record_count_local = 0;
static uint8_t current_hour_local = 0;
static uint8_t current_minute_local = 0;
static uint8_t current_second_local = 0;
static uint16_t confirm_window_seconds = 300; // 默认5分钟确认窗口

/* 用于追踪每个服药时间是否已经提醒过 */
static uint8_t reminded_flag[MAX_MED_TIMES];

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/

/**
 * @brief 初始化药物管理系统
 */
void MedicineManager_Init(void)
{
    med_times_count_local = 0;
    record_count_local = 0;
    
    // 初始化默认的服药时间
    MedicineManager_AddTime(8, 0, "降压药");
    MedicineManager_AddTime(12, 0, "维生素");
    MedicineManager_AddTime(20, 0, "降糖药");
    
    // 清空提醒标志
    for (int i = 0; i < MAX_MED_TIMES; i++) {
        reminded_flag[i] = 0;
    }
}

/**
 * @brief 添加服药时间
 * @param hour 时
 * @param minute 分
 * @param name 药品名
 * @return 成功返回1，失败返回0
 */
uint8_t MedicineManager_AddTime(uint8_t hour, uint8_t minute, char* name)
{
    if (med_times_count_local >= MAX_MED_TIMES) {
        return 0; // 达到最大限制
    }
    
    if (hour > 23 || minute > 59) {
        return 0; // 时间无效
    }
    
    med_times_local[med_times_count_local].hour = hour;
    med_times_local[med_times_count_local].minute = minute;
    strncpy(med_times_local[med_times_count_local].medicine_name, name, sizeof(med_times_local[med_times_count_local].medicine_name) - 1);
    med_times_local[med_times_count_local].medicine_name[sizeof(med_times_local[med_times_count_local].medicine_name) - 1] = '\0';
    
    med_times_count_local++;
    return 1;
}

/**
 * @brief 删除指定索引的服药时间
 * @param index 索引
 * @return 成功返回1，失败返回0
 */
uint8_t MedicineManager_RemoveTime(uint8_t index)
{
    if (index >= med_times_count_local) {
        return 0; // 索引超出范围
    }
    
    // 将后面的元素向前移动
    for (int i = index; i < med_times_count_local - 1; i++) {
        med_times_local[i] = med_times_local[i + 1];
    }
    
    med_times_count_local--;
    return 1;
}

/**
 * @brief 获取当前时间
 * @param hour 时
 * @param minute 分
 * @param second 秒
 */
void MedicineManager_GetCurrentTime(uint8_t* hour, uint8_t* minute, uint8_t* second)
{
    *hour = current_hour_local;
    *minute = current_minute_local;
    *second = current_second_local;
}

/**
 * @brief 设置当前时间
 * @param hour 时
 * @param minute 分
 * @param second 秒
 */
void MedicineManager_SetCurrentTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    if (hour <= 23 && minute <= 59 && second <= 59) {
        current_hour_local = hour;
        current_minute_local = minute;
        current_second_local = second;
    }
}

/**
 * @brief 检查是否到达服药时间
 * @return 返回匹配的服药时间索引，如果没有返回-1
 */
int8_t MedicineManager_CheckTime(void)
{
    for (int i = 0; i < med_times_count_local; i++) {
        if (med_times_local[i].hour == current_hour_local && 
            med_times_local[i].minute == current_minute_local &&
            !reminded_flag[i]) {
            reminded_flag[i] = 1; // 标记已提醒，防止重复触发
            return i; // 找到匹配的服药时间
        }
    }
    return -1; // 没有匹配的服药时间
}

/**
 * @brief 记录服药确认
 * @param idx 服药时间索引
 * @param confirm_method 确认方式（0:按键，1:盒盖检测）
 * @param on_time 是否按时
 */
void MedicineManager_RecordConfirmation(uint8_t idx, uint8_t confirm_method, uint8_t on_time)
{
    if (idx >= med_times_count_local) {
        return; // 参数错误
    }
    
    // 如果记录已满，循环覆盖最早的记录
    if (record_count_local >= MAX_LOG_RECORDS) {
        // 将记录向前移动
        for (int i = 0; i < MAX_LOG_RECORDS - 1; i++) {
            records_local[i] = records_local[i + 1];
        }
        record_count_local = MAX_LOG_RECORDS - 1;
    }
    
    records_local[record_count_local].timestamp = (current_hour_local * 3600) + (current_minute_local * 60) + current_second_local;
    records_local[record_count_local].reminder_idx = idx;
    records_local[record_count_local].confirm_method = confirm_method;
    records_local[record_count_local].on_time = on_time;
    records_local[record_count_local].missed = (on_time == 0) ? 1 : 0;
    
    record_count_local++;
}

/**
 * @brief 检查是否漏服
 * @param idx 服药时间索引
 * @return 漏服返回1，否则返回0
 */
uint8_t MedicineManager_IsMissed(uint8_t idx)
{
    if (idx >= med_times_count_local) {
        return 0; // 索引超出范围
    }
    
    // 简单检查：如果过去1小时内没有该服药时间的记录，则认为可能漏服
    uint32_t current_timestamp = (current_hour_local * 3600) + (current_minute_local * 60);
    
    for (int i = 0; i < record_count_local; i++) {
        if (records_local[i].reminder_idx == idx) {
            // 如果最近的记录时间接近当前时间，则认为已确认
            if (current_timestamp - records_local[i].timestamp < 3600) { // 1小时内
                return records_local[i].missed;
            }
        }
    }
    
    // 如果没有找到最近的记录，且已经超过服药时间几分钟，认为是漏服
    MedicineTime_t med_time = med_times_local[idx];
    uint32_t med_timestamp = (med_time.hour * 3600) + (med_time.minute * 60);
    
    if (current_timestamp > med_timestamp && (current_timestamp - med_timestamp) > 300) { // 5分钟后
        return 1; // 漏服
    }
    
    return 0;
}

/**
 * @brief 获取服药统计信息
 * @param total 总次数
 * @param taken 已服用次数
 * @param missed 漏服次数
 */
void MedicineManager_GetStats(uint16_t* total, uint16_t* taken, uint16_t* missed)
{
    *total = 0;
    *taken = 0;
    *missed = 0;
    
    for (int i = 0; i < record_count_local; i++) {
        (*total)++;
        if (records_local[i].missed) {
            (*missed)++;
        } else {
            (*taken)++;
        }
    }
}

/**
 * @brief 获取最近的服药记录
 * @param count 要获取的记录数
 * @return 记录数组
 */
MedicineRecord_t* MedicineManager_GetRecentRecords(uint8_t count)
{
    if (count > record_count_local) {
        count = record_count_local;
    }
    
    // 返回最近的count条记录
    if (count == 0) {
        return NULL;
    }
    
    return &records_local[record_count_local - count];
}

/**
 * @brief 获取记录数量
 * @return 记录数量
 */
uint16_t MedicineManager_GetRecordCount(void)
{
    return record_count_local;
}

/**
 * @brief 判断当前时间是否在指定服药时间的确认窗口内
 * @param idx 服药时间索引
 * @return 在窗口内返回1，否则返回0
 */
uint8_t MedicineManager_IsInWindow(uint8_t idx)
{
    if (idx >= med_times_count_local) {
        return 0;
    }
    
    MedicineTime_t* med = &med_times_local[idx];
    uint32_t med_timestamp = (med->hour * 3600) + (med->minute * 60);
    uint32_t current_timestamp = (current_hour_local * 3600) + (current_minute_local * 60);
    
    // 当前时间在服药时间之后，且在确认窗口内
    if (current_timestamp >= med_timestamp && 
        (current_timestamp - med_timestamp) <= confirm_window_seconds) {
        return 1;
    }
    return 0;
}

/**
 * @brief 设置确认窗口超时时间（秒）
 * @param seconds 超时秒数
 */
void MedicineManager_SetConfirmWindow(uint16_t seconds)
{
    confirm_window_seconds = seconds;
}

/**
 * @brief 获取当前设置的服药时间数量
 * @return 服药时间数量
 */
uint8_t MedicineManager_GetTimeCount(void)
{
    return med_times_count_local;
}

/**
 * @brief 获取指定索引的服药时间
 * @param index 索引
 * @return 服药时间指针
 */
MedicineTime_t* MedicineManager_GetTime(uint8_t index)
{
    if (index >= med_times_count_local) {
        return NULL;
    }
    return &med_times_local[index];
}

/**
 * @brief 获取下一次最近的服药时间
 * @param hour 小时
 * @param minute 分钟
 * @param name 药品名称
 * @return 有下一个返回1，没有返回0
 */
uint8_t MedicineManager_GetNextMedicine(uint8_t* hour, uint8_t* minute, char* name)
{
    if (med_times_count_local == 0) {
        return 0;
    }
    
    uint32_t current_timestamp = (current_hour_local * 3600) + (current_minute_local * 60);
    int16_t closest_idx = -1;
    uint32_t min_diff = 86400; // 一天的秒数
    
    for (int i = 0; i < med_times_count_local; i++) {
        if (reminded_flag[i]) continue; // 已提醒过的跳过
        
        uint32_t med_timestamp = (med_times_local[i].hour * 3600) + (med_times_local[i].minute * 60);
        uint32_t diff;
        
        if (med_timestamp > current_timestamp) {
            diff = med_timestamp - current_timestamp;
        } else {
            // 今天的已过，算到明天
            diff = (86400 - current_timestamp) + med_timestamp;
        }
        
        if (diff < min_diff) {
            min_diff = diff;
            closest_idx = i;
        }
    }
    
    // 如果所有都已提醒，找第一个明天的
    if (closest_idx < 0) {
        // 重置所有提醒标志，找最近的下一次
        for (int i = 0; i < MAX_MED_TIMES; i++) {
            reminded_flag[i] = 0;
        }
        return MedicineManager_GetNextMedicine(hour, minute, name);
    }
    
    *hour = med_times_local[closest_idx].hour;
    *minute = med_times_local[closest_idx].minute;
    strcpy(name, med_times_local[closest_idx].medicine_name);
    return 1;
}

/**
 * @brief 重置提醒标志（在确认或漏服后调用）
 */
void MedicineManager_ResetReminded(uint8_t idx)
{
    if (idx < MAX_MED_TIMES) {
        reminded_flag[idx] = 0;
    }
}

/**
 * @brief 重置所有提醒标志
 */
void MedicineManager_ResetAllReminded(void)
{
    for (int i = 0; i < MAX_MED_TIMES; i++) {
        reminded_flag[i] = 0;
    }
}