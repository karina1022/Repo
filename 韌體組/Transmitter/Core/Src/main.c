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
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_host.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ===== NEC timing ===== */
#define IR_START_ON_US             9000U
#define IR_START_OFF_US            4500U
#define IR_BIT_MARK_US             562U
#define IR_ZERO_SPACE_US           562U
#define IR_ONE_SPACE_US            1687U
#define IR_STOP_MARK_US            562U

/* ===== Send strategy ===== */
#define IR_SAFE_CONFIRM_TX_COUNT   3U
#define IR_SAFE_BURST_GAP_MS       120U
#define IR_EVENT_PERIOD_MS         150U
#define IR_ERROR_PERIOD_MS         300U
#define IR_MAIN_LOOP_DELAY_MS      5U

#define IR_PWM_DUTY                26U

/* ===== Event type ===== */
#define IR_TYPE_NONE               0x00U
#define IR_TYPE_VRU_CROSS          0x01U
#define IR_TYPE_BRAKE              0x02U
#define IR_TYPE_OBSTACLE           0x03U
#define IR_TYPE_ACCIDENT           0x04U
#define IR_TYPE_LANE               0x05U
#define IR_TYPE_RESERVED           0x06U
#define IR_TYPE_ERROR              0x07U

/* ===== Warning level ===== */
#define IR_LEVEL_NONE              0x00U
#define IR_LEVEL_1                 0x01U
#define IR_LEVEL_2                 0x02U
#define IR_LEVEL_3                 0x03U
/* ===== Nano -> STM32 UART packet =====
 * Packet format: [0xAA][event code]
 */
#define NANO_PACKET_HEADER         0xAAU
#define NANO_EVENT_STOPPED         0x00U
#define NANO_EVENT_NO_WARNING      0x10U
#define NANO_EVENT_LEVEL_1         0x11U
#define NANO_EVENT_LEVEL_2         0x12U
#define NANO_EVENT_LEVEL_3         0x13U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */


/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t current_event_type = IR_TYPE_NONE;
static uint8_t current_level = IR_LEVEL_NONE;

static uint8_t safe_tx_remaining = 0U;

static uint32_t last_send_tick = 0U;
static uint8_t force_send_now = 0U;

/* 避免相同事件重複刷新 LCD */
static uint8_t lcd_last_event_type = 0xFFU;
static uint8_t lcd_last_level = 0xFFU;
/* USART2 每次以中斷接收 Nano 的一個 Byte */
static uint8_t nano_rx_byte = 0U;

/*
 * nano_rx_state：
 * 0 = 等待 0xAA
 * 1 = 等待事件碼
 */
static volatile uint8_t nano_rx_state = 0U;

/* 收到的事件碼 */
static volatile uint8_t nano_pending_code = 0U;

/* 完整封包已收到 */
static volatile uint8_t nano_packet_ready = 0U;

/* 避免相同事件重複呼叫 Set_Event() */
static uint8_t nano_last_applied_code = 0xFFU;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */
static void UART_Print(const char *msg);

static void DWT_Delay_Init(void);
static void delay_us(uint32_t us);

static void IR_CarrierOn(void);
static void IR_CarrierOff(void);
static void IR_SendStart(void);
static void IR_SendBit(uint8_t bitValue);
static void IR_SendStop(void);
static uint8_t IR_MakePayload(uint8_t event_type, uint8_t level);
static void IR_SendPayload8(uint8_t payload);
static void IR_SendCurrentEvent(void);
static void IR_Process(void);

static void LCD_UpdateEvent(uint8_t event_type, uint8_t level);

static void Set_Event(uint8_t event_type, uint8_t level);

static void Nano_UART_StartReceive(void);
static void Nano_ProcessPendingPacket(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART_Print(const char *msg)
{
  uint16_t len = 0U;

  while (msg[len] != '\0')
  {
    len++;
  }

  HAL_UART_Transmit(&huart4,
                    (uint8_t *)msg,
                    len,
                    HAL_MAX_DELAY);
}

/*
 * STM32F407 Cortex-M4 DWT 微秒延遲。
 * NEC 時序需要 us 等級，不能使用 HAL_Delay() 取代。
 */
static void DWT_Delay_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
  uint32_t start_tick;
  uint32_t delay_ticks;

  start_tick = DWT->CYCCNT;
  delay_ticks = us * (SystemCoreClock / 1000000U);

  while ((DWT->CYCCNT - start_tick) < delay_ticks)
  {
  }
}

/* PA8 / TIM1_CH1 38 kHz carrier */
static void IR_CarrierOn(void)
{
  __HAL_TIM_SET_COMPARE(&htim1,
                        TIM_CHANNEL_1,
                        IR_PWM_DUTY);
}

static void IR_CarrierOff(void)
{
  __HAL_TIM_SET_COMPARE(&htim1,
                        TIM_CHANNEL_1,
                        0U);
}

static void IR_SendStart(void)
{
  IR_CarrierOn();
  delay_us(IR_START_ON_US);

  IR_CarrierOff();
  delay_us(IR_START_OFF_US);
}

static void IR_SendBit(uint8_t bitValue)
{
  IR_CarrierOn();
  delay_us(IR_BIT_MARK_US);

  IR_CarrierOff();

  if (bitValue != 0U)
  {
    delay_us(IR_ONE_SPACE_US);
  }
  else
  {
    delay_us(IR_ZERO_SPACE_US);
  }
}

static void IR_SendStop(void)
{
  IR_CarrierOn();
  delay_us(IR_STOP_MARK_US);

  IR_CarrierOff();
}
static void LCD_UpdateEvent(uint8_t event_type, uint8_t level)
{
  /*
   * 狀態相同就不重新繪圖，避免 LCD 閃爍。
   */
  if ((lcd_last_event_type == event_type) &&
      (lcd_last_level == level))
  {
    return;
  }

  lcd_last_event_type = event_type;
  lcd_last_level = level;

  if (event_type == IR_TYPE_ERROR)
  {
    LCD_ShowSystemErrorScreen();
  }
  else if ((event_type == IR_TYPE_NONE) &&
           (level == IR_LEVEL_NONE))
  {
    LCD_ShowSafeScreen();
  }
  else if ((event_type == IR_TYPE_VRU_CROSS) &&
           (level == IR_LEVEL_1))
  {
    LCD_ShowLevel1Screen();
  }
  else if ((event_type == IR_TYPE_VRU_CROSS) &&
           (level == IR_LEVEL_2))
  {
    LCD_ShowLevel2Screen();
  }
  else if ((event_type == IR_TYPE_VRU_CROSS) &&
           (level == IR_LEVEL_3))
  {
    LCD_ShowLevel3Screen();
  }
  else
  {
    LCD_ShowSystemErrorScreen();
  }
}
/*
 * Payload：
 *
 * TTT LL CCC
 *
 * TTT：event type
 * LL ：warning level
 * CCC：TTT XOR 0LL
 */
static uint8_t IR_MakePayload(uint8_t event_type,
                              uint8_t level)
{
  uint8_t ttt;
  uint8_t ll;
  uint8_t ccc;

  ttt = event_type & 0x07U;
  ll = level & 0x03U;
  ccc = (ttt ^ ll) & 0x07U;

  return (uint8_t)((ttt << 5) |
                   (ll << 3) |
                   ccc);
}

/* MSB first */
static void IR_SendPayload8(uint8_t payload)
{
  int8_t i;

  for (i = 7; i >= 0; i--)
  {
    IR_SendBit((payload >> i) & 0x01U);
  }
}

static void IR_SendCurrentEvent(void)
{
  uint8_t payload;

  payload = IR_MakePayload(current_event_type,
                           current_level);

  IR_SendStart();
  IR_SendPayload8(payload);
  IR_SendStop();

  IR_CarrierOff();
}
static void Set_Event(uint8_t event_type, uint8_t level)
{
  /*
   * 儲存目前事件。
   * LCD 與紅外線發送都使用這兩個狀態。
   */
  current_event_type = event_type;
  current_level = level;

  /*
   * 收到新事件後，下一輪立即發送，
   * 不必先等待 150 ms。
   */
  force_send_now = 1U;
  last_send_tick = HAL_GetTick();

  if ((event_type == IR_TYPE_NONE) &&
      (level == IR_LEVEL_NONE))
  {
    /*
     * SAFE 只送三次，
     * 三次結束後停止紅外線。
     */
    safe_tx_remaining = IR_SAFE_CONFIRM_TX_COUNT;
  }
  else
  {
    /*
     * Level 1、2、3 或 Error
     * 由 IR_Process() 持續發送。
     */
    safe_tx_remaining = 0U;
  }

  /*
   * 使用同一事件切換組員做好的 LCD 畫面。
   */
  LCD_UpdateEvent(event_type, level);
}
static void Nano_UART_StartReceive(void)
{
  nano_rx_state = 0U;
  nano_packet_ready = 0U;

  if (HAL_UART_Receive_IT(&huart2,
                          &nano_rx_byte,
                          1U) != HAL_OK)
  {
    Error_Handler();
  }
}
static void Nano_ProcessPendingPacket(void)
{
  uint8_t event_code;

  /*
   * 中斷只負責收資料。
   * LCD 和紅外線相關函式放在主迴圈執行。
   */
  if (nano_packet_ready == 0U)
  {
    return;
  }

  __disable_irq();

  event_code = nano_pending_code;
  nano_packet_ready = 0U;

  __enable_irq();

  /*
   * 相同事件不重複呼叫 Set_Event()。
   */
  if (event_code == nano_last_applied_code)
  {
    return;
  }

  switch (event_code)
  {
    case NANO_EVENT_STOPPED:
      Set_Event(IR_TYPE_NONE,
                IR_LEVEL_NONE);

      nano_last_applied_code = event_code;

      UART_Print(
          "NANO AA 00: STOPPED / NO WARNING\r\n"
      );
      break;

    case NANO_EVENT_NO_WARNING:
      Set_Event(IR_TYPE_NONE,
                IR_LEVEL_NONE);

      nano_last_applied_code = event_code;

      UART_Print(
          "NANO AA 10: NO WARNING\r\n"
      );
      break;

    case NANO_EVENT_LEVEL_1:
      Set_Event(IR_TYPE_VRU_CROSS,
                IR_LEVEL_1);

      nano_last_applied_code = event_code;

      UART_Print(
          "NANO AA 11: VRU LEVEL 1\r\n"
      );
      break;

    case NANO_EVENT_LEVEL_2:
      Set_Event(IR_TYPE_VRU_CROSS,
                IR_LEVEL_2);

      nano_last_applied_code = event_code;

      UART_Print(
          "NANO AA 12: VRU LEVEL 2\r\n"
      );
      break;

    case NANO_EVENT_LEVEL_3:
      Set_Event(IR_TYPE_VRU_CROSS,
                IR_LEVEL_3);

      nano_last_applied_code = event_code;

      UART_Print(
          "NANO AA 13: VRU LEVEL 3\r\n"
      );
      break;

    default:
      /*
       * 未定義的資料直接忽略，
       * 不轉成紅外線事件。
       */
      UART_Print(
          "NANO: INVALID EVENT CODE\r\n"
      );
      break;
  }
}
static void IR_Process(void)
{
  uint32_t now;
  uint32_t period_ms;

  now = HAL_GetTick();

  /*
   * SAFE 狀態：
   * 送三次 00000000 後停止。
   */
  if ((current_event_type == IR_TYPE_NONE) &&
      (current_level == IR_LEVEL_NONE))
  {
    if (safe_tx_remaining > 0U)
    {
      if ((force_send_now != 0U) ||
          ((now - last_send_tick) >= IR_SAFE_BURST_GAP_MS))
      {
        IR_SendCurrentEvent();

        safe_tx_remaining--;
        last_send_tick = now;
        force_send_now = 0U;

        UART_Print("TX SAFE 00000000\r\n");

        if (safe_tx_remaining == 0U)
        {
          UART_Print("SAFE finished, IR OFF\r\n");
        }
      }
    }
    else
    {
      /*
       * SAFE 三次發送完成後，保持紅外線關閉。
       */
      IR_CarrierOff();
      force_send_now = 0U;
    }

    return;
  }

  /*
   * 系統錯誤每 300 ms 發送一次；
   * 一般事件每 150 ms 發送一次。
   */
  if (current_event_type == IR_TYPE_ERROR)
  {
    period_ms = IR_ERROR_PERIOD_MS;
  }
  else
  {
    period_ms = IR_EVENT_PERIOD_MS;
  }

  if ((force_send_now != 0U) ||
      ((now - last_send_tick) >= period_ms))
  {
    IR_SendCurrentEvent();

    last_send_tick = now;
    force_send_now = 0U;
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */


  /* USER CODE END 1 */

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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI2_Init();
  MX_TIM1_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_USB_HOST_Init();
  /* USER CODE BEGIN 2 */
  static const char boot_msg[] = "UART4 BOOT TEST\r\n";

HAL_UART_Transmit(&huart4,
                  (uint8_t *)boot_msg,
                  sizeof(boot_msg) - 1U,
                  1000U);

/*
 * 等待 LCD 電源及控制器穩定。
 */
HAL_Delay(1000);

ILI9341_Init();
HAL_Delay(100);

/*
 * 初始化 DWT 微秒延遲。
 * NEC 紅外線時序需要 us 等級延遲。
 */
DWT_Delay_Init();

/*
 * 啟動 TIM1 Channel 1 PWM。
 */
if (HAL_TIM_PWM_Start(&htim1,
                      TIM_CHANNEL_1) != HAL_OK)
{
  Error_Handler();
}

/*
 * 開機預設先關閉紅外線載波。
 */
IR_CarrierOff();

/*
 * 開機初始狀態設定為 SAFE。
 * 這裡會同時：
 * 1. 顯示組員做好的安全畫面
 * 2. 準備發送三次 SAFE 封包
 */
Set_Event(IR_TYPE_NONE,
          IR_LEVEL_NONE);
          /*
 * 啟動 USART2 接收 Nano。
 * UART4 保留作為電腦除錯輸出。
 */
Nano_UART_StartReceive();

UART_Print("\r\nLCD + IR TX Ready\r\n");

UART_Print(
    "USART2 waiting for Nano packets: "
    "AA 00/10/11/12/13\r\n"
);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
{
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

  /*
   * 檢查 UART4 是否收到 0、1、2、3、E。
   */
  Nano_ProcessPendingPacket();

  IR_Process();

  /*
   * 避免主迴圈跑得過快。
   */
  HAL_Delay(IR_MAIN_LOOP_DELAY_MS);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    if (nano_rx_state == 0U)
    {
      /*
       * 第一階段：等待封包表頭 0xAA。
       */
      if (nano_rx_byte == NANO_PACKET_HEADER)
      {
        nano_rx_state = 1U;
      }
    }
    else
    {
      /*
       * 若再次收到 0xAA，
       * 視為新的封包表頭，維持同步。
       */
      if (nano_rx_byte == NANO_PACKET_HEADER)
      {
        nano_rx_state = 1U;
      }
      else
      {
        /*
         * 第二個 Byte 是事件碼。
         */
        nano_pending_code = nano_rx_byte;
        nano_packet_ready = 1U;
        nano_rx_state = 0U;
      }
    }

    /*
     * 重新啟動下一個 Byte 的接收。
     */
    (void)HAL_UART_Receive_IT(&huart2,
                              &nano_rx_byte,
                              1U);
  }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    /*
     * 放棄未完成封包，重新等待 0xAA。
     */
    nano_rx_state = 0U;
    nano_packet_ready = 0U;

    (void)HAL_UART_Receive_IT(&huart2,
                              &nano_rx_byte,
                              1U);
  }
}

/* USER CODE END 4 */

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
