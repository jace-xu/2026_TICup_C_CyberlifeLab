#include "buzzer_driver.h"
#include "tim.h"
#include <math.h>

/* ================================================================
 * 硬件参数（基于 TIM12 当前配置）
 * TIM12 计数器时钟 = 240MHz / 24 = 10MHz
 * PWM 频率 = 10MHz / (ARR + 1)
 * ARR     = 10MHz / freq - 1
 * ================================================================ */
#define BUZZER_TIM           (&htim12)
#define BUZZER_CHANNEL        TIM_CHANNEL_2
#define BUZZER_TIMER_CLK_HZ   10000000UL   /* PSC=24 → 10MHz */
#define BUZZER_ARR_MAX        65535UL

/* ================================================================
 * 音量映射：volume 0~100 → 占空比 0~50%
 * 50% 占空比时基频分量最强，故 volume=100 对应 50% duty
 * ================================================================ */
static void buzzer_set_arr_ccr(uint32_t arr, uint8_t volume)
{
    uint32_t ccr;

    if (volume > 100) volume = 100;

    /* CCR = (ARR+1) * volume / 200   →  volume=100 时占空比=50% */
    ccr = (arr + 1UL) * volume / 200UL;

    __HAL_TIM_SET_AUTORELOAD(BUZZER_TIM, arr);
    __HAL_TIM_SET_COMPARE(BUZZER_TIM, BUZZER_CHANNEL, ccr);

    /* 触发更新事件，立即装载影子寄存器 */
    BUZZER_TIM->Instance->EGR = TIM_EGR_UG;
    (void)BUZZER_TIM->Instance->SR; /* 清除可能挂起的更新标志 */
}

/* ================================================================
 * 公开 API
 * ================================================================ */

/**
 * @brief  按频率和音量播放
 */
void Buzzer_Play(uint16_t freq_hz, uint8_t volume)
{
    uint32_t arr;

    if (freq_hz == 0 || volume == 0) {
        Buzzer_Stop();
        return;
    }

    /* ARR = 计数器时钟 / 频率 - 1 */
    arr = BUZZER_TIMER_CLK_HZ / freq_hz;

    if (arr > 0) {
        arr -= 1;
    }
    if (arr > BUZZER_ARR_MAX) {
        arr = BUZZER_ARR_MAX;   /* 钳位到最低频率 ≈ 153Hz */
    }

    buzzer_set_arr_ccr(arr, volume);
}

/**
 * @brief  按音名 + 八度 + 音量播放
 * @note   基准: A4 = 440Hz
 *         半音距离 = (octave-4)*12 + (note-9)
 *         频率 = 440 * 2^(半音距离/12)
 */
void Buzzer_Note(BuzzerNote_t note, uint8_t octave, uint8_t volume)
{
    float freq;
    int8_t semitones;

    /* 计算目标音与 A4(440Hz) 的半音距离 */
    semitones = (int8_t)(octave - 4) * 12 + ((int8_t)note - (int8_t)BUZZER_NOTE_A);

    /* 频率 = 440 * 2^(semitones/12) */
    freq = 440.0f * powf(2.0f, (float)semitones / 12.0f);

    Buzzer_Play((uint16_t)(freq + 0.5f), volume);
}

/**
 * @brief  停止（占空比清零，IO 输出低电平）
 */
void Buzzer_Stop(void)
{
    __HAL_TIM_SET_COMPARE(BUZZER_TIM, BUZZER_CHANNEL, 0);
    BUZZER_TIM->Instance->EGR = TIM_EGR_UG;
    (void)BUZZER_TIM->Instance->SR;
}

/* ================================================================
 * 音调序列状态机（非阻塞，由外部 50ms Tick 驱动）
 * ================================================================ */
typedef enum {
    BUZZER_SEQ_NONE = 0,
    BUZZER_SEQ_ASCEND,    /* 升调：感应区→迎宾区 */
    BUZZER_SEQ_DESCEND,   /* 降调：迎宾区→感应区 */
} BuzzerSeq_t;

static BuzzerSeq_t buzzer_seq = BUZZER_SEQ_NONE;
static uint8_t     buzzer_step = 0;    /* 当前音符索引 0~3 */
static uint8_t     buzzer_tick = 0;    /* 当前音符已持续的 tick 数 */

/* 音符序列（4 音阶，每音 250ms @50ms/tick，总长 1s） */
static const uint16_t ascend_notes[] = {
    BUZZER_FREQ_C4, BUZZER_FREQ_E4, BUZZER_FREQ_G4, BUZZER_FREQ_C5
};
static const uint16_t descend_notes[] = {
    BUZZER_FREQ_C5, BUZZER_FREQ_G4, BUZZER_FREQ_E4, BUZZER_FREQ_C4
};

#define BUZZER_NOTE_COUNT      4
#define BUZZER_TICKS_PER_NOTE  5    /* 5 × 50ms = 250ms 每音 */
#define BUZZER_SEQ_VOLUME      100

/**
 * @brief  启动音调序列（立即播放第一个音符）
 */
void Buzzer_Seq_Start(uint8_t ascending)
{
    buzzer_seq  = ascending ? BUZZER_SEQ_ASCEND : BUZZER_SEQ_DESCEND;
    buzzer_step = 0;
    buzzer_tick = 0;

    /* 立即播放第一个音符（仅写寄存器，非阻塞） */
    Buzzer_Play(ascending ? ascend_notes[0] : descend_notes[0], BUZZER_SEQ_VOLUME);
}

/**
 * @brief  步进序列状态机（每 50ms 调用一次）
 */
void Buzzer_Seq_Tick(void)
{
    if (buzzer_seq == BUZZER_SEQ_NONE) {
        return;
    }

    buzzer_tick++;
    if (buzzer_tick >= BUZZER_TICKS_PER_NOTE) {
        buzzer_tick = 0;
        buzzer_step++;

        if (buzzer_step >= BUZZER_NOTE_COUNT) {
            /* 序列完成，停止并复位 */
            Buzzer_Stop();
            buzzer_seq = BUZZER_SEQ_NONE;
        } else {
            const uint16_t *notes = (buzzer_seq == BUZZER_SEQ_ASCEND)
                                   ? ascend_notes : descend_notes;
            Buzzer_Play(notes[buzzer_step], BUZZER_SEQ_VOLUME);
        }
    }
}
