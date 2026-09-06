#ifndef __IT_H
#define __IT_H

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

extern uint8_t id_receive; // 声明全局变量id
extern uint8_t id_set;
extern data_packet_t data_packet; // 声明全局变量data_packet
extern key_data_t key_data;       // 声明全局变量key_data (UWB钥匙数据)
#endif // __IT_H
