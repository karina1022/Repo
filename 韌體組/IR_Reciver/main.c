/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : IR RX NEC timing-based simplified packet receiver
  *
  * Protocol:
  *   Start + 8-bit Payload + Stop
  *
  * Payload:
  *   TTT LL CCC
  *
  *   TTT = 3-bit Event Type
  *   LL  = 2-bit Level
  *   CCC = 3-bit Check Code = TTT XOR 0LL
  *
  * Safety logic:
  *   - SAFE must be received 3 times before output changes to SAFE.
  *   - Event / Error must be received 2 times before output changes.
  *   - checksum error / invalid frame / no frame will not change output state.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  IR_RX_NO_FRAME = 0,
  IR_RX_OK       = 1,
  IR_RX_INVALID  = 2
} IR_RxStatus_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* =========================
 * IR Receiver GPIO
 * =========================
 * 多數 38 kHz IR receiver module：
 * 收到紅外線載波時，輸出 LOW
 * 沒收到時，輸出 HIGH
 */
#define IR_RX_PORT                 GPIOB
#define IR_RX_PIN                  GPIO_PIN_0

#define IR_ACTIVE_STATE            GPIO_PIN_RESET
#define IR_IDLE_STATE              GPIO_PIN_SET


/* =========================
 * NEC timing range
 * 單位：us
 * ========================= */
#define IR_START_MARK_MIN_US       7500U
#define IR_START_MARK_MAX_US       11000U

#define IR_START_SPACE_MIN_US      3000U
#define IR_START_SPACE_MAX_US      6000U

#define IR_BIT_MARK_MIN_US         300U
#define IR_BIT_MARK_MAX_US         900U

#define IR_ZERO_SPACE_MIN_US       300U
#define IR_ZERO_SPACE_MAX_US       1000U

#define IR_ONE_SPACE_MIN_US        1100U
#define IR_ONE_SPACE_MAX_US        2500U

#define IR_STOP_MARK_MIN_US        300U
#define IR_STOP_MARK_MAX_US        1000U

#define IR_FRAME_WAIT_TIMEOUT_US   150000U
#define IR_PULSE_TIMEOUT_US        12000U


/* =========================
 * Confirm logic
 * ========================= */
#define SAFE_CONFIRM_COUNT         3U
#define EVENT_CONFIRM_COUNT        2U
#define ERROR_CONFIRM_COUNT        2U

// 如果 candidate 太久沒有再收到同樣 payload，就清掉
#define CANDIDATE_TIMEOUT_MS       700U


/* =========================
 * UART log period
 * 你要改 UART 輸出頻率主要改這裡
 * ========================= */
#define UART_NO_FRAME_LOG_PERIOD_MS     1000U
#define UART_INVALID_LOG_PERIOD_MS      500U
#define UART_CHECKSUM_LOG_PERIOD_MS     500U

// 0 代表每次 RX OK 都印出來
#define UART_RX_OK_LOG_PERIOD_MS        0U


/* =========================
 * Event Type: TTT
 * ========================= */
#define IR_TYPE_NONE              0x00U
#define IR_TYPE_VRU_CROSS         0x01U
#define IR_TYPE_FRONT_BRAKE       0x02U
#define IR_TYPE_OBSTACLE          0x03U
#define IR_TYPE_ACCIDENT          0x04U
#define IR_TYPE_LANE_ABNORMAL     0x05U
#define IR_TYPE_RESERVED          0x06U
#define IR_TYPE_SYSTEM_ERROR      0x07U


/* =========================
 * Level: LL
 * ========================= */
#define IR_LEVEL_NONE             0x00U
#define IR_LEVEL_1                0x01U
#define IR_LEVEL_2                0x02U
#define IR_LEVEL_3                0x03U

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */

static uint8_t output_payload = 0xFFU;

static uint8_t candidate_payload = 0xFFU;
static uint8_t candidate_count = 0;
static uint32_t candidate_last_tick = 0;

static uint32_t last_no_frame_log_tick = 0;
static uint32_t last_invalid_log_tick = 0;
static uint32_t last_checksum_log_tick = 0;
static uint32_t last_rx_ok_log_tick = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART4_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */

static void UART_Print(const char *msg);
static void UART_Printf(const char *fmt, ...);

static void DWT_Delay_Init(void);
static uint32_t DWT_TicksPerUs(void);

static uint8_t Time_InRange(uint32_t value, uint32_t min, uint32_t max);
static uint8_t UART_ShouldLog(uint32_t *last_tick, uint32_t period_ms);

static uint8_t IR_WaitForState(GPIO_PinState state, uint32_t timeout_us);
static uint8_t IR_MeasureState(GPIO_PinState state, uint32_t timeout_us, uint32_t *duration_us);

static IR_RxStatus_t IR_DecodeFrame(uint8_t *payload);
static uint8_t IR_CheckPayload(uint8_t payload, uint8_t *event_type, uint8_t *level);
static void IR_PayloadToBits(uint8_t payload, char *bits);

static uint8_t Get_RequiredConfirmCount(uint8_t event_type, uint8_t level);
static const char *Get_EventName(uint8_t event_type, uint8_t level);

static void Candidate_Reset(void);
static void Candidate_CheckTimeout(void);
static void Process_ValidPayload(uint8_t payload);

static void Panel_ShowUnknown(void);
static void Panel_ShowSafe(void);
static void Panel_ShowLevel1(void);
static void Panel_ShowLevel2(void);
static void Panel_ShowLevel3(void);
static void Panel_ShowError(void);
static void Panel_Update(uint8_t event_type, uint8_t level);

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

#include <stdarg.h>

static void UART_Print(const char *msg)
{
  uint16_t len = 0;

  while (msg[len] != '\0')
  {
    len++;
  }

  HAL_UART_Transmit(&huart4, (uint8_t *)msg, len, HAL_MAX_DELAY);
}

static void UART_Printf(const char *fmt, ...)
{
  char buffer[180];

  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (len < 0)
  {
    return;
  }

  if (len > (int)sizeof(buffer))
  {
    len = sizeof(buffer);
  }

  HAL_UART_Transmit(&huart4, (uint8_t *)buffer, len, HAL_MAX_DELAY);
}

static void DWT_Delay_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t DWT_TicksPerUs(void)
{
  return SystemCoreClock / 1000000U;
}

static uint8_t Time_InRange(uint32_t value, uint32_t min, uint32_t max)
{
  if (value >= min && value <= max)
  {
    return 1U;
  }

  return 0U;
}

static uint8_t UART_ShouldLog(uint32_t *last_tick, uint32_t period_ms)
{
  uint32_t now = HAL_GetTick();

  if (period_ms == 0U)
  {
    return 1U;
  }

  if ((now - *last_tick) >= period_ms)
  {
    *last_tick = now;
    return 1U;
  }

  return 0U;
}

static uint8_t IR_WaitForState(GPIO_PinState state, uint32_t timeout_us)
{
  uint32_t start_tick = DWT->CYCCNT;
  uint32_t timeout_ticks = timeout_us * DWT_TicksPerUs();

  while (HAL_GPIO_ReadPin(IR_RX_PORT, IR_RX_PIN) != state)
  {
    if ((DWT->CYCCNT - start_tick) >= timeout_ticks)
    {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t IR_MeasureState(GPIO_PinState state, uint32_t timeout_us, uint32_t *duration_us)
{
  uint32_t start_tick;
  uint32_t elapsed_ticks;
  uint32_t timeout_ticks = timeout_us * DWT_TicksPerUs();

  if (HAL_GPIO_ReadPin(IR_RX_PORT, IR_RX_PIN) != state)
  {
    return 0U;
  }

  start_tick = DWT->CYCCNT;

  while (HAL_GPIO_ReadPin(IR_RX_PORT, IR_RX_PIN) == state)
  {
    elapsed_ticks = DWT->CYCCNT - start_tick;

    if (elapsed_ticks >= timeout_ticks)
    {
      if (duration_us != NULL)
      {
        *duration_us = elapsed_ticks / DWT_TicksPerUs();
      }

      return 0U;
    }
  }

  elapsed_ticks = DWT->CYCCNT - start_tick;

  if (duration_us != NULL)
  {
    *duration_us = elapsed_ticks / DWT_TicksPerUs();
  }

  return 1U;
}

/*
 * 解碼：
 * Start = 9ms LOW + 4.5ms HIGH
 * bit 0 = 562us LOW + 562us HIGH
 * bit 1 = 562us LOW + 1687us HIGH
 * Stop  = 562us LOW
 *
 * TX 端 payload 是 MSB first，所以 RX 也用 MSB first 組回來。
 */
static IR_RxStatus_t IR_DecodeFrame(uint8_t *payload)
{
  uint32_t frame_start_tick = DWT->CYCCNT;
  uint32_t frame_timeout_ticks = IR_FRAME_WAIT_TIMEOUT_US * DWT_TicksPerUs();

  uint32_t duration = 0;
  uint8_t data = 0;
  uint8_t found_start = 0;

  if (payload == NULL)
  {
    return IR_RX_INVALID;
  }

  while ((DWT->CYCCNT - frame_start_tick) < frame_timeout_ticks)
  {
    if (!IR_WaitForState(IR_ACTIVE_STATE, 2000U))
    {
      continue;
    }

    if (!IR_MeasureState(IR_ACTIVE_STATE, IR_PULSE_TIMEOUT_US, &duration))
    {
      return IR_RX_INVALID;
    }

    if (Time_InRange(duration, IR_START_MARK_MIN_US, IR_START_MARK_MAX_US))
    {
      found_start = 1U;
      break;
    }
  }

  if (!found_start)
  {
    return IR_RX_NO_FRAME;
  }

  if (!IR_MeasureState(IR_IDLE_STATE, IR_PULSE_TIMEOUT_US, &duration))
  {
    return IR_RX_INVALID;
  }

  if (!Time_InRange(duration, IR_START_SPACE_MIN_US, IR_START_SPACE_MAX_US))
  {
    return IR_RX_INVALID;
  }

  for (int8_t bit_index = 7; bit_index >= 0; bit_index--)
  {
    if (!IR_WaitForState(IR_ACTIVE_STATE, 3000U))
    {
      return IR_RX_INVALID;
    }

    if (!IR_MeasureState(IR_ACTIVE_STATE, IR_PULSE_TIMEOUT_US, &duration))
    {
      return IR_RX_INVALID;
    }

    if (!Time_InRange(duration, IR_BIT_MARK_MIN_US, IR_BIT_MARK_MAX_US))
    {
      return IR_RX_INVALID;
    }

    if (!IR_MeasureState(IR_IDLE_STATE, IR_PULSE_TIMEOUT_US, &duration))
    {
      return IR_RX_INVALID;
    }

    if (Time_InRange(duration, IR_ZERO_SPACE_MIN_US, IR_ZERO_SPACE_MAX_US))
    {
      // bit = 0
    }
    else if (Time_InRange(duration, IR_ONE_SPACE_MIN_US, IR_ONE_SPACE_MAX_US))
    {
      data |= (1U << bit_index);
    }
    else
    {
      return IR_RX_INVALID;
    }
  }

  if (!IR_WaitForState(IR_ACTIVE_STATE, 3000U))
  {
    return IR_RX_INVALID;
  }

  if (!IR_MeasureState(IR_ACTIVE_STATE, IR_PULSE_TIMEOUT_US, &duration))
  {
    return IR_RX_INVALID;
  }

  if (!Time_InRange(duration, IR_STOP_MARK_MIN_US, IR_STOP_MARK_MAX_US))
  {
    return IR_RX_INVALID;
  }

  *payload = data;
  return IR_RX_OK;
}

static uint8_t IR_CheckPayload(uint8_t payload, uint8_t *event_type, uint8_t *level)
{
  uint8_t ttt = (payload >> 5) & 0x07U;
  uint8_t ll  = (payload >> 3) & 0x03U;
  uint8_t ccc = payload & 0x07U;

  uint8_t expected_ccc = (ttt ^ ll) & 0x07U;

  if (event_type != NULL)
  {
    *event_type = ttt;
  }

  if (level != NULL)
  {
    *level = ll;
  }

  if (ccc == expected_ccc)
  {
    return 1U;
  }

  return 0U;
}

static void IR_PayloadToBits(uint8_t payload, char *bits)
{
  if (bits == NULL)
  {
    return;
  }

  for (uint8_t i = 0; i < 8; i++)
  {
    if ((payload & (1U << (7U - i))) != 0U)
    {
      bits[i] = '1';
    }
    else
    {
      bits[i] = '0';
    }
  }

  bits[8] = '\0';
}

static uint8_t Get_RequiredConfirmCount(uint8_t event_type, uint8_t level)
{
  if (event_type == IR_TYPE_NONE && level == IR_LEVEL_NONE)
  {
    return SAFE_CONFIRM_COUNT;
  }

  if (event_type == IR_TYPE_SYSTEM_ERROR)
  {
    return ERROR_CONFIRM_COUNT;
  }

  return EVENT_CONFIRM_COUNT;
}

static const char *Get_EventName(uint8_t event_type, uint8_t level)
{
  if (event_type == IR_TYPE_NONE && level == IR_LEVEL_NONE)
  {
    return "SAFE";
  }

  if (event_type == IR_TYPE_VRU_CROSS && level == IR_LEVEL_1)
  {
    return "VRU CROSS Level 1";
  }

  if (event_type == IR_TYPE_VRU_CROSS && level == IR_LEVEL_2)
  {
    return "VRU CROSS Level 2";
  }

  if (event_type == IR_TYPE_VRU_CROSS && level == IR_LEVEL_3)
  {
    return "VRU CROSS Level 3";
  }

  if (event_type == IR_TYPE_SYSTEM_ERROR)
  {
    return "SYSTEM ERROR";
  }

  return "RESERVED / FUTURE EVENT";
}

static void Candidate_Reset(void)
{
  candidate_payload = 0xFFU;
  candidate_count = 0;
  candidate_last_tick = 0;
}

static void Candidate_CheckTimeout(void)
{
  uint32_t now = HAL_GetTick();

  if (candidate_count > 0U)
  {
    if ((now - candidate_last_tick) > CANDIDATE_TIMEOUT_MS)
    {
      Candidate_Reset();
    }
  }
}

static void Process_ValidPayload(uint8_t payload)
{
  uint8_t event_type = 0;
  uint8_t level = 0;
  uint8_t required_count = 0;

  char bits[9];

  if (!IR_CheckPayload(payload, &event_type, &level))
  {
    // checksum 錯誤不改變輸出狀態，也不印出，避免 UART 洗版
    Candidate_Reset();
    return;
  }

  Candidate_CheckTimeout();

  required_count = Get_RequiredConfirmCount(event_type, level);

  if (payload == candidate_payload)
  {
    if (candidate_count < 255U)
    {
      candidate_count++;
    }
  }
  else
  {
    candidate_payload = payload;
    candidate_count = 1U;
  }

  candidate_last_tick = HAL_GetTick();

  if (candidate_count >= required_count)
  {
    if (output_payload != payload)
    {
      output_payload = payload;
      Panel_Update(event_type, level);

      IR_PayloadToBits(payload, bits);

      // 只有「狀態真的改變」時才輸出 UART
      UART_Printf("STATE CHANGED: payload = %s, type = %u, level = %u, event = %s\r\n",
                  bits,
                  event_type,
                  level,
                  Get_EventName(event_type, level));
    }

    if (candidate_count > required_count)
    {
      candidate_count = required_count;
    }
  }
}

/* =========================
 * LED Panel
 * =========================
 * F407 Discovery:
 * LD4 = Green
 * LD3 = Orange
 * LD5 = Red
 * LD6 = Blue
 */
static void Panel_ShowUnknown(void)
{
  HAL_GPIO_WritePin(GPIOD, LD6_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, LD3_Pin | LD4_Pin | LD5_Pin, GPIO_PIN_RESET);
}

static void Panel_ShowSafe(void)
{
  HAL_GPIO_WritePin(GPIOD, LD4_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, LD3_Pin | LD5_Pin | LD6_Pin, GPIO_PIN_RESET);
}

static void Panel_ShowLevel1(void)
{
  HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, LD4_Pin | LD5_Pin | LD6_Pin, GPIO_PIN_RESET);
}

static void Panel_ShowLevel2(void)
{
  HAL_GPIO_WritePin(GPIOD, LD5_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, LD3_Pin | LD4_Pin | LD6_Pin, GPIO_PIN_RESET);
}

static void Panel_ShowLevel3(void)
{
  HAL_GPIO_WritePin(GPIOD, LD5_Pin | LD6_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, LD3_Pin | LD4_Pin, GPIO_PIN_RESET);
}

static void Panel_ShowError(void)
{
  HAL_GPIO_WritePin(GPIOD, LD3_Pin | LD5_Pin | LD6_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, LD4_Pin, GPIO_PIN_RESET);
}

static void Panel_Update(uint8_t event_type, uint8_t level)
{
  if (event_type == IR_TYPE_NONE && level == IR_LEVEL_NONE)
  {
    Panel_ShowSafe();
  }
  else if (event_type == IR_TYPE_VRU_CROSS && level == IR_LEVEL_1)
  {
    Panel_ShowLevel1();
  }
  else if (event_type == IR_TYPE_VRU_CROSS && level == IR_LEVEL_2)
  {
    Panel_ShowLevel2();
  }
  else if (event_type == IR_TYPE_VRU_CROSS && level == IR_LEVEL_3)
  {
    Panel_ShowLevel3();
  }
  else if (event_type == IR_TYPE_SYSTEM_ERROR)
  {
    Panel_ShowError();
  }
  else
  {
    Panel_ShowUnknown();
  }
}

/* USER CODE END 0 */

int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_UART4_Init();
  MX_USB_HOST_Init();

  /* USER CODE BEGIN 2 */

  DWT_Delay_Init();

  output_payload = 0xFFU;
  Candidate_Reset();
  Panel_ShowUnknown();

  UART_Print("\r\nSTM32 IR RX NEC Decoder Ready v4 - state change only\r\n");
  UART_Print("Protocol: Start + 8-bit Payload + Stop\r\n");
  UART_Print("Payload format: TTT LL CCC, CCC = TTT XOR 0LL\r\n");
  UART_Print("Confirm rule:\r\n");
  UART_Print("SAFE  : 3 same valid payloads required\r\n");
  UART_Print("EVENT : 2 same valid payloads required\r\n");
  UART_Print("ERROR : 2 same valid payloads required\r\n");
  UART_Print("Known payloads:\r\n");
  UART_Print("00000000 = SAFE\r\n");
  UART_Print("00101000 = VRU CROSS Level 1\r\n");
  UART_Print("00110011 = VRU CROSS Level 2\r\n");
  UART_Print("00111010 = VRU CROSS Level 3\r\n");
  UART_Print("11100111 = SYSTEM ERROR\r\n\r\n");

  /* USER CODE END 2 */

  while (1)
  {
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

    uint8_t payload = 0;
    IR_RxStatus_t rx_status;

    Candidate_CheckTimeout();

    rx_status = IR_DecodeFrame(&payload);

    if (rx_status == IR_RX_OK)
    {
      Process_ValidPayload(payload);
    }
    else if (rx_status == IR_RX_INVALID)
    {
      // 無效 frame 不改變輸出狀態，也不印出，避免 UART 洗版
      Candidate_Reset();
    }
    else
    {
      // no frame 不改變輸出狀態，也不印出
    }

    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{
  __HAL_RCC_UART4_CLK_ENABLE();

  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOD, LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  // PB0 = IR receiver input
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // UART4: PC10 = TX, PC11 = RX
  GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
    HAL_GPIO_WritePin(GPIOD, LD6_Pin, GPIO_PIN_SET);
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif