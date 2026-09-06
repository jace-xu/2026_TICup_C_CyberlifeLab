#ifndef __BUZZER_DRIVER_H__
#define __BUZZER_DRIVER_H__

#include "main.h"

/* ================================================================
 * 音名枚举（12 半音）
 * ================================================================ */
typedef enum {
    BUZZER_NOTE_C  = 0,   /* Do       */
    BUZZER_NOTE_CS = 1,   /* Do# / Reb */
    BUZZER_NOTE_D  = 2,   /* Re       */
    BUZZER_NOTE_DS = 3,   /* Re# / Mib */
    BUZZER_NOTE_E  = 4,   /* Mi       */
    BUZZER_NOTE_F  = 5,   /* Fa       */
    BUZZER_NOTE_FS = 6,   /* Fa# / Solb */
    BUZZER_NOTE_G  = 7,   /* Sol      */
    BUZZER_NOTE_GS = 8,   /* Sol# / Lab */
    BUZZER_NOTE_A  = 9,   /* La       */
    BUZZER_NOTE_AS = 10,  /* La# / Sib */
    BUZZER_NOTE_B  = 11,  /* Si       */
} BuzzerNote_t;

/* ================================================================
 * 常用音符频率宏（Hz）—— 方便直接传参
 * ================================================================ */
/* 低音区 */
#define BUZZER_FREQ_C3   131
#define BUZZER_FREQ_D3   147
#define BUZZER_FREQ_E3   165
#define BUZZER_FREQ_F3   175
#define BUZZER_FREQ_G3   196
#define BUZZER_FREQ_A3   220
#define BUZZER_FREQ_B3   247

/* 中音区 */
#define BUZZER_FREQ_C4   262    /* 中央 C  */
#define BUZZER_FREQ_D4   294
#define BUZZER_FREQ_E4   330
#define BUZZER_FREQ_F4   349
#define BUZZER_FREQ_G4   392
#define BUZZER_FREQ_A4   440    /* 标准音 A4 = 440Hz */
#define BUZZER_FREQ_B4   494

/* 高音区 */
#define BUZZER_FREQ_C5   523
#define BUZZER_FREQ_D5   587
#define BUZZER_FREQ_E5   659
#define BUZZER_FREQ_F5   698
#define BUZZER_FREQ_G5   784
#define BUZZER_FREQ_A5   880
#define BUZZER_FREQ_B5   988

/* 倍高音区 */
#define BUZZER_FREQ_C6   1047
#define BUZZER_FREQ_D6   1175
#define BUZZER_FREQ_E6   1319
#define BUZZER_FREQ_F6   1397
#define BUZZER_FREQ_G6   1568
#define BUZZER_FREQ_A6   1760
#define BUZZER_FREQ_B6   1976

/* ================================================================
 * API
 * ================================================================ */

/**
 * @brief  按频率和音量播放
 * @param  freq_hz   频率 (Hz)，范围约 153 ~ 10000
 * @param  volume    音量 0~100（0 = 静音，100 = 最大）
 * @note   频率过低(<153Hz) 会被钳位，过高会被钳位
 */
void Buzzer_Play(uint16_t freq_hz, uint8_t volume);

/**
 * @brief  按音名 + 八度 + 音量播放
 * @param  note      音名，见 BuzzerNote_t 枚举
 * @param  octave    八度 (1~7)
 * @param  volume    音量 0~100
 * @note   例: Buzzer_Note(BUZZER_NOTE_A, 4, 80) → A4(440Hz) 80%音量
 */
void Buzzer_Note(BuzzerNote_t note, uint8_t octave, uint8_t volume);

/**
 * @brief  停止播放（占空比置 0）
 */
void Buzzer_Stop(void);

/* ================================================================
 * 音调序列（非阻塞，由定时器 ISR 驱动）
 * ================================================================ */

/**
 * @brief  启动音调序列
 * @param  ascending  1 = 升调（感应区→迎宾区），0 = 降调（迎宾区→感应区）
 * @note   立即播放第一个音符，随后需周期性调用 Buzzer_Seq_Tick() 推进
 *         序列总长 1s（4 个音，每音 250ms），完成后自动 Stop
 *         新序列会打断当前正在播放的序列
 */
void Buzzer_Seq_Start(uint8_t ascending);

/**
 * @brief  步进音调序列状态机
 * @note   需每 50ms 调用一次（放在 TIM1 中断回调中）
 *         无活动序列时几乎零开销（仅一次判空 return）
 */
void Buzzer_Seq_Tick(void);

#endif /* __BUZZER_DRIVER_H__ */
