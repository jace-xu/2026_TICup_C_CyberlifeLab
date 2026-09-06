/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ws2812.h"
#include "buzzer_driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t id_receive = 0xFF;
uint8_t id_set = 0;
volatile data_packet_t data_packet = {0};
volatile key_data_t key_data = {0};
volatile uint8_t use_raspi = 1;  /* 1=树莓派数据(USB CDC), 0=UWB模块数据(UART10) */
volatile uint8_t raspi_first_beep = 0;  /* 0=等待首次数据, 1=待蜂鸣, 2=已完成 */
extern uint8_t rx_buffer[];
extern uint8_t rx_buffer2[];

/* WS2812 灯控标志（ISR 写，主循环读） */
extern volatile uint8_t ws2812_update;
extern volatile uint8_t ws2812_r;
extern volatile uint8_t ws2812_g;
extern volatile uint8_t ws2812_b;
extern volatile uint8_t ws2812_last_r;
extern volatile uint8_t ws2812_last_g;
extern volatile uint8_t ws2812_last_b;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_UART7_Init();
  MX_USART10_UART_Init();
  MX_SPI6_Init();
  MX_TIM12_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
	/* 清空 USART10 RX 残留，避免字节错位导致永久偏移 */
	/* 空闲帧分包接收 UWB 变长数据包（0x2001=37B / 0x2002=16B） */
	HAL_UARTEx_ReceiveToIdle_IT(&huart10, rx_buffer, 64);
	// 开启TIM1 更新中断
	HAL_TIM_Base_Start_IT(&htim1);
  // 开启蜂鸣器
  HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);
  /* 上电测试音：响 300ms 验证蜂鸣器硬件正常 */
  Buzzer_Play(BUZZER_FREQ_C4, 100);
  HAL_Delay(300);
  Buzzer_Stop();
  /* 初始化 flag 引脚：默认树莓派模式 → 高电平 */
  HAL_GPIO_WritePin(flag_GPIO_Port, flag_Pin, GPIO_PIN_SET);
  /* 使能 botton 按键的 EXTI 中断（PA15 → EXTI15_10） */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
  HAL_GPIO_WritePin(en_GPIO_Port,en_Pin,GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ═══════════════════════════════════════════════════════════════
     * 测试模式：手动注入数据，不依赖串口/UWB
     * 每 5 秒自动轮转一个场景，测试 LED + 蜂鸣器动作
     * 测试完成后将 #if 1 改为 #if 0 即可关闭
     * ═══════════════════════════════════════════════════════════════ */
#if 0
    {
        static uint8_t  test_phase = 0;
        static uint32_t last_tick  = 0;
        uint32_t now = HAL_GetTick();

        if (now - last_tick >= 1000)   /* 每 1 秒切换场景 */
        {
            last_tick = now;

            switch (test_phase)
            {
            case 0:  /* 无钥匙 → LED 灭 */
                id_receive             = 0xFF;
                data_packet.if_detect  = 0;
                data_packet.distance   = 0.0f;
                data_packet.angle      = 0.0f;
                break;

            case 1:  /* 感应区 ≥200cm → 蓝灯 */
                id_receive             = id_set;
                data_packet.if_detect  = 1;
                data_packet.distance   = 250.0f;
                data_packet.angle      = 45.0f;
                break;

            case 2:  /* 迎宾区 100~200cm → 黄灯 + 升调（感应→迎宾） */
                id_receive             = id_set;
                data_packet.if_detect  = 1;
                data_packet.distance   = 150.0f;
                data_packet.angle      = 30.0f;
                break;

            case 3:  /* 开锁区 ≤100cm → 绿灯 */
                id_receive             = id_set;
                data_packet.if_detect  = 1;
                data_packet.distance   = 50.0f;
                data_packet.angle      = 15.0f;
                break;

            case 4:  /* 迎宾区 → 黄灯 */
                id_receive             = id_set;
                data_packet.if_detect  = 1;
                data_packet.distance   = 150.0f;
                data_packet.angle      = 30.0f;
                break;

            case 5:  /* 感应区 → 蓝灯 + 降调（迎宾→感应） */
                id_receive             = id_set;
                data_packet.if_detect  = 1;
                data_packet.distance   = 250.0f;
                data_packet.angle      = 45.0f;
                break;

            case 6:  /* ID 不匹配 → 红灯 */
                {
                    uint8_t fake_id = (id_set == 0) ? 1 : 0; /* 故意错 */
                    id_receive             = fake_id;
                    data_packet.if_detect  = 1;
                    data_packet.distance   = 100.0f;
                    data_packet.angle      = 0.0f;
                }
                break;
            }

            test_phase++;
            if (test_phase > 6) { test_phase = 0; }
        }
    }
#endif

    /* 首次收到树莓派有效串口数据 → 蜂鸣器确认（高音短促，与开机自检低音区分） */
    if (raspi_first_beep == 1)
    {
        raspi_first_beep = 2;
        Buzzer_Play(BUZZER_FREQ_E6, 100);
        HAL_Delay(150);
        Buzzer_Stop();
    }

    /* WS2812 灯控：ISR 设标志，主循环执行 SPI 传输 */
    if (ws2812_update)
    {
        ws2812_update = 0;
        WS2812_Ctrl(ws2812_r, ws2812_g, ws2812_b);
        /* 记录本次实际写入的 RGB，供 ISR 判断是否需要下次刷新 */
        ws2812_last_r = ws2812_r;
        ws2812_last_g = ws2812_g;
        ws2812_last_b = ws2812_b;
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
