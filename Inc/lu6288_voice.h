/**
  ******************************************************************************
  * @file           : lu6288_voice.h
  * @brief          : LU6288语音合成模块 — GBK编码语音数据
  *                   中文文本必须以GBK/GB2312编码发送，UTF-8会导致乱码/无声
  *                   以下数组 = "<G>"前缀(ASCII) + 中文GBK字节序列
  ******************************************************************************
  */

#ifndef INC_LU6288_VOICE_H_
#define INC_LU6288_VOICE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "<G>该吃药了吃两颗" — 闹钟触发播报 (降压药/血糖药通用) */
static const uint8_t VOICE_GBK_REMIND[] = {
    0x3C, 0x47, 0x3E,                         /* <G> */
    0xB8, 0xC3, 0xB3, 0xD4, 0xD2, 0xA9, 0xC1, 0xCB,  /* 该吃药了 */
    0xB3, 0xD4, 0xC1, 0xBD, 0xBF, 0xC5                 /* 吃两颗 */
};
#define VOICE_GBK_REMIND_LEN  (sizeof(VOICE_GBK_REMIND))

/* "<G>已服药祝您身体健康" — 确认服药播报 */
static const uint8_t VOICE_GBK_CONFIRM[] = {
    0x3C, 0x47, 0x3E,                         /* <G> */
    0xD2, 0xD1, 0xB7, 0xFE, 0xD2, 0xA9,       /* 已服药 */
    0xD7, 0xA3, 0xC4, 0xFA,                   /* 祝您 */
    0xC9, 0xED, 0xCC, 0xE5, 0xBD, 0xA1, 0xBF, 0xB5  /* 身体健康 */
};
#define VOICE_GBK_CONFIRM_LEN  (sizeof(VOICE_GBK_CONFIRM))

/* "<G>记得服药记得服药记得服药" — 超时未服药播报 */
static const uint8_t VOICE_GBK_WARN[] = {
    0x3C, 0x47, 0x3E,                         /* <G> */
    0xBC, 0xC7, 0xB5, 0xC3, 0xB7, 0xFE, 0xD2, 0xA9,  /* 记得服药 */
    0xBC, 0xC7, 0xB5, 0xC3, 0xB7, 0xFE, 0xD2, 0xA9,  /* 记得服药 */
    0xBC, 0xC7, 0xB5, 0xC3, 0xB7, 0xFE, 0xD2, 0xA9   /* 记得服药 */
};
#define VOICE_GBK_WARN_LEN  (sizeof(VOICE_GBK_WARN))

#ifdef __cplusplus
}
#endif

#endif /* INC_LU6288_VOICE_H_ */
