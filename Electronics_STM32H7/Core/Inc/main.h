/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
typedef struct {
    uint8_t if_detect;
    float distance;
    float angle;
} data_packet_t;

/* 钥匙UWB数据包 (0x2001协议) */
typedef struct {
    uint32_t tag_id;     /* 信标ID (Tag ID) */
    uint32_t distance;   /* 距离 (cm) */
    int16_t  azimuth;    /* 方位角 (度) */
    int16_t  elevation;  /* 仰角 (度) */
} key_data_t;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern volatile uint8_t use_raspi;  /* 1=树莓派数据, 0=UWB模块数据 */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define flag_Pin GPIO_PIN_13
#define flag_GPIO_Port GPIOC
#define en_Pin GPIO_PIN_15
#define en_GPIO_Port GPIOC
#define bit3_Pin GPIO_PIN_0
#define bit3_GPIO_Port GPIOA
#define bit2_Pin GPIO_PIN_2
#define bit2_GPIO_Port GPIOA
#define bit1_Pin GPIO_PIN_9
#define bit1_GPIO_Port GPIOE
#define bit0_Pin GPIO_PIN_13
#define bit0_GPIO_Port GPIOE
#define open_Pin GPIO_PIN_9
#define open_GPIO_Port GPIOA
#define close_Pin GPIO_PIN_10
#define close_GPIO_Port GPIOA
#define botton_Pin GPIO_PIN_15
#define botton_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
