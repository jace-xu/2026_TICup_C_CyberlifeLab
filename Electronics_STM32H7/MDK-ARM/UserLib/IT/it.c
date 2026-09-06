#include "it.h"
#include <string.h>
#include <stdio.h>
#include "ws2812.h"
#include "buzzer_driver.h"

uint8_t rx_buffer[64];  /* UWB 变长接收缓冲区: 0x2001=37B, 0x2002=16B */
uint8_t rx_buffer2[11];

/* WS2812 灯控标志（ISR 只写标志，主循环执行 SPI 传输） */
volatile uint8_t ws2812_update = 0;
volatile uint8_t ws2812_r = 0;
volatile uint8_t ws2812_g = 0;
volatile uint8_t ws2812_b = 0;
volatile uint8_t ws2812_last_r = 0xFF;  /* 上次实际写入的 RGB，初始 0xFF 确保首次刷新 */
volatile uint8_t ws2812_last_g = 0xFF;
volatile uint8_t ws2812_last_b = 0xFF;

/* 蜂鸣器区域追踪（检测区域转换，触发音调序列） */
typedef enum {
    ZONE_NONE = 0,        /* 无钥匙 / ID 不匹配 */
    ZONE_UNLOCK,          /* ≤100cm  开锁区 */
    ZONE_WELCOME,         /* 100~200 迎宾区 */
    ZONE_SENSING,         /* ≥200cm  感应区 */
} DoorZone_t;

static DoorZone_t prev_zone = ZONE_NONE;

// ===== 外部中断回调（botton 按键：消抖 + 切换数据源） =====
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == botton_Pin)
    {
        static uint32_t last_trigger = 0;
        uint32_t now = HAL_GetTick();

        /* 消抖 50ms：距离上次触发不足 50ms 则视为抖动，忽略 */
        if (now - last_trigger >= 50)
        {
            use_raspi = !use_raspi;
            HAL_GPIO_WritePin(flag_GPIO_Port, flag_Pin, use_raspi ? GPIO_PIN_SET : GPIO_PIN_RESET);
            last_trigger = now;
        }
    }
}

// ===== 串口接收完成回调（兜底：缓冲区满时仍确保重新接收） =====
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
    if (huart == &huart10) {
        HAL_UARTEx_ReceiveToIdle_IT(&huart10, rx_buffer, sizeof(rx_buffer));
    }
}

// ===== UART 空闲帧回调（UWB 变长包 0x2001/0x2002 分包接收） =====
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == &huart10)
    {
        /* Size < 4 说明是空闲触发但无有效数据，直接重新接收避免死循环 */
        if (Size >= 4)
        {
            /* 校验帧头 */
            if ((rx_buffer[0] == 0xFF) && (rx_buffer[1] == 0xFF) &&
                (rx_buffer[2] == 0xFF) && (rx_buffer[3] == 0xFF))
        {
            if (Size == 37)
            {
                /* ── 0x2001 定位数据包 ── */
                uint16_t cmd = ((uint16_t)rx_buffer[8] << 8) | rx_buffer[9];
                if (cmd == 0x2001)
                {
                    uint8_t xor_val = 0;
                    for (uint8_t i = 0; i < 36; i++) { xor_val ^= rx_buffer[i]; }
                    if (xor_val == rx_buffer[36])
                    {
                        /* 大端转小端 —— 提取数据 */
                        key_data.tag_id = ((uint32_t)rx_buffer[16] << 24) |
                                          ((uint32_t)rx_buffer[17] << 16) |
                                          ((uint32_t)rx_buffer[18] << 8)  |
                                          (uint32_t)rx_buffer[19];

                        key_data.distance = ((uint32_t)rx_buffer[20] << 24) |
                                            ((uint32_t)rx_buffer[21] << 16) |
                                            ((uint32_t)rx_buffer[22] << 8)  |
                                            (uint32_t)rx_buffer[23];

                        /* 减去基站到天线面板的固定偏移 30cm */
                        key_data.distance = (key_data.distance >= 30) ? (key_data.distance - 30) : 0;

                        key_data.azimuth = (int16_t)(((uint16_t)rx_buffer[24] << 8) |
                                                      (uint16_t)rx_buffer[25]);

                        key_data.elevation = (int16_t)(((uint16_t)rx_buffer[26] << 8) |
                                                        (uint16_t)rx_buffer[27]);

                        id_receive = (uint8_t)(key_data.tag_id & 0x0F);
                    }
                    else { id_receive = 0xFF; }  /* XOR校验失败 */
                }
                else { id_receive = 0xFF; }      /* 命令码不匹配 */
            }
            else if (Size == 16)
            {
                /* ── 0x2002 心跳包：基站未检测到信标 ── */
                uint16_t cmd = ((uint16_t)rx_buffer[8] << 8) | rx_buffer[9];
                if (cmd == 0x2002)
                {
                    id_receive = 0xFF;  /* 无钥匙 */
                }
            }
            /* 其他长度忽略 */
            }
        }

        HAL_UARTEx_ReceiveToIdle_IT(&huart10, rx_buffer, sizeof(rx_buffer));
    }
}

// ===== 发送屏幕数据包（USART7） =====
static void send_screen_data(int dist, int angle, uint8_t valid, uint8_t rx_id, uint8_t set_id){
    char buf[96];
    int pos = 0;

    // valid=值 + 帧尾 0xFF 0xFF 0xFF
    pos += sprintf(buf + pos, "valid=%d", valid);
    buf[pos++] = 0xFF; buf[pos++] = 0xFF; buf[pos++] = 0xFF;

    // id_receive=值 + 帧尾
    pos += sprintf(buf + pos, "id_receive=%d", rx_id);
    buf[pos++] = 0xFF; buf[pos++] = 0xFF; buf[pos++] = 0xFF;

    // id_set=值 + 帧尾
    pos += sprintf(buf + pos, "id_set=%d", set_id);
    buf[pos++] = 0xFF; buf[pos++] = 0xFF; buf[pos++] = 0xFF;

    // dist=值 + 帧尾
    pos += sprintf(buf + pos, "dist=%d", dist);
    buf[pos++] = 0xFF; buf[pos++] = 0xFF; buf[pos++] = 0xFF;

    // angle=值 + 帧尾
    pos += sprintf(buf + pos, "angle=%d", angle);
    buf[pos++] = 0xFF; buf[pos++] = 0xFF; buf[pos++] = 0xFF;

    HAL_UART_Transmit(&huart7, (uint8_t *)buf, pos, 100);
}

// ===== 定时器中断回调 =====
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
    if(htim == &htim1){

        // 设置门锁id（拨码开关）
        id_set = 0x0F & ((!HAL_GPIO_ReadPin(bit3_GPIO_Port, bit3_Pin) << 3) | (!HAL_GPIO_ReadPin(bit2_GPIO_Port, bit2_Pin) << 2) |
                        (!HAL_GPIO_ReadPin(bit1_GPIO_Port, bit1_Pin) << 1) | (!HAL_GPIO_ReadPin(bit0_GPIO_Port, bit0_Pin)));

        // 发送数据默认值
        uint8_t valid = 0;
        int dist = 0;
        int angle = 0;
        uint8_t key_detected = 0;

        /* ---- 数据源切换 ---- */
        if (use_raspi)
        {
            /* 树莓派 (USB CDC)：if_detect + 借用 UWB 的 id_receive 做 ID 匹配 */
            key_detected = (data_packet.if_detect == 1);
            if (key_detected && (id_set == id_receive))
            {
                valid = 1;
                dist  = (int)data_packet.distance;
                angle = (int)data_packet.angle;
            }
        }
        else
        {
            /* UWB 模块 (UART10)：只看自己的 id_receive + ID 匹配 */
            key_detected = (id_receive != 0xFF);
            if (key_detected && (id_set == id_receive))
            {
                valid = 1;
                dist  = (int)key_data.distance;   /* uint32 cm → int */
                angle = (int)key_data.azimuth;     /* int16 度 → int */
            }
        }

        /* ---- LED 灯控 ---- */
        if (key_detected)
        {
            if (valid)  /* ID 匹配 */
            {
                if (dist <= 100)
                {
                    ws2812_r = 0;  ws2812_g = 255; ws2812_b = 0;   /* 绿灯: 开锁区 */
                    HAL_GPIO_WritePin(open_GPIO_Port,open_Pin,GPIO_PIN_SET);
                    HAL_GPIO_WritePin(close_GPIO_Port,close_Pin,GPIO_PIN_RESET);
                }
                else if (dist < 200)
                {
                    ws2812_r = 255; ws2812_g = 255; ws2812_b = 0;   /* 黄灯: 迎宾区 */
                    HAL_GPIO_WritePin(open_GPIO_Port,open_Pin,GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(close_GPIO_Port,close_Pin,GPIO_PIN_SET);
                }
                else
                {
                    ws2812_r = 0;  ws2812_g = 0;  ws2812_b = 255;  /* 蓝灯: 感应区 */
                    HAL_GPIO_WritePin(open_GPIO_Port,open_Pin,GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(close_GPIO_Port,close_Pin,GPIO_PIN_SET);
                }
            }
            else  /* ID 不匹配 */
            {
                ws2812_r = 255; ws2812_g = 0; ws2812_b = 0;         /* 红灯 */
                HAL_GPIO_WritePin(open_GPIO_Port,open_Pin,GPIO_PIN_RESET);
                HAL_GPIO_WritePin(close_GPIO_Port,close_Pin,GPIO_PIN_SET);
            }
        }
        else  /* 没有检测到钥匙 */
        {
            ws2812_r = 0; ws2812_g = 0; ws2812_b = 0;               /* 不发光 */
            HAL_GPIO_WritePin(open_GPIO_Port,open_Pin,GPIO_PIN_RESET);
            HAL_GPIO_WritePin(close_GPIO_Port,close_Pin,GPIO_PIN_SET);
        }

        /* 只有 RGB 与上次实际写入不同时，才通知主循环刷新 */
        if (ws2812_r != ws2812_last_r ||
            ws2812_g != ws2812_last_g ||
            ws2812_b != ws2812_last_b)
        {
            ws2812_update = 1;
        }

        /* ---- 蜂鸣器：区域转换检测 + 音调序列步进 ---- */
        {
            DoorZone_t current_zone;

            if (!key_detected || !valid)
            {
                current_zone = ZONE_NONE;
            }
            else if (dist <= 100)
            {
                current_zone = ZONE_UNLOCK;
            }
            else if (dist < 200)
            {
                current_zone = ZONE_WELCOME;
            }
            else
            {
                current_zone = ZONE_SENSING;
            }

            /* 检测区域转换 → 触发音调序列 */
            if (current_zone != prev_zone)
            {
                if (prev_zone == ZONE_SENSING && current_zone == ZONE_WELCOME)
                {
                    Buzzer_Seq_Start(1);  /* 升调：感应区→迎宾区 */
                }
                else if (prev_zone == ZONE_WELCOME && current_zone == ZONE_SENSING)
                {
                    Buzzer_Seq_Start(0);  /* 降调：迎宾区→感应区 */
                }
                prev_zone = current_zone;
            }

            /* 每 50ms 步进一次序列状态机（无活动序列时仅一次判空 return） */
            Buzzer_Seq_Tick();
        }

        send_screen_data(dist, angle, valid, id_receive, id_set);
    }
}
