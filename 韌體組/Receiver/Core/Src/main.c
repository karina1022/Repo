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
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
    ALERT_SAFE = 0,
    ALERT_LEVEL1,
    ALERT_LEVEL2,
    ALERT_LEVEL3,
    ALERT_SYSTEM_ERROR,
    ALERT_INVALID
} AlertEvent_t;
/* 紅外線接收一次後可能得到的結果 */
typedef enum
{
    IR_RX_NO_FRAME = 0,   // 沒找到一個完整封包
    IR_RX_OK,             // 成功解出8-bit
    IR_RX_INVALID         // 有收到訊號，但時序不正確
} IR_RxStatus_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* =========================
 * 紅外線接收器腳位
 * ========================= */

/* 紅外線接收器OUT接到PB0 */
#define IR_RX_PORT                 GPIOB
#define IR_RX_PIN                  GPIO_PIN_0

/*
 * 一般38kHz接收器：
 * 收到紅外線載波時，OUT輸出LOW
 * 沒有載波時，OUT輸出HIGH
 */
#define IR_ACTIVE_STATE            GPIO_PIN_RESET
#define IR_IDLE_STATE              GPIO_PIN_SET


/* =========================
 * NEC時序範圍，單位為µs
 * ========================= */

/* Start：理論值9 ms LOW */
#define IR_START_MARK_MIN_US       7500U
#define IR_START_MARK_MAX_US       11000U

/* Start：理論值4.5 ms HIGH */
#define IR_START_SPACE_MIN_US      3000U
#define IR_START_SPACE_MAX_US      6000U

/* 每個bit前面的562 µs LOW */
#define IR_BIT_MARK_MIN_US         300U
#define IR_BIT_MARK_MAX_US         900U

/* bit 0後面的562 µs HIGH */
#define IR_ZERO_SPACE_MIN_US       300U
#define IR_ZERO_SPACE_MAX_US       1000U

/* bit 1後面的1687 µs HIGH */
#define IR_ONE_SPACE_MIN_US        1100U
#define IR_ONE_SPACE_MAX_US        2500U

/* Stop：562 µs LOW */
#define IR_STOP_MARK_MIN_US        300U
#define IR_STOP_MARK_MAX_US        1000U

/* 等待一個封包的最長時間 */
#define IR_FRAME_WAIT_TIMEOUT_US   150000U

/* 等待單一脈衝的最長時間 */
#define IR_PULSE_TIMEOUT_US        12000U


/* =========================
 * 連續封包確認規則
 * ========================= */

#define SAFE_CONFIRM_COUNT         3U
#define EVENT_CONFIRM_COUNT        2U
#define ERROR_CONFIRM_COUNT        2U

/* 超過700 ms沒再收到相同候選碼，就重新計算 */
#define CANDIDATE_TIMEOUT_MS       700U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

I2S_HandleTypeDef hi2s3;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static AlertEvent_t last_voice_event = ALERT_SAFE;
/*
 * 目前系統已經正式確認的Payload。
 * 0xFF不是你們使用的正常Payload，因此用作初始值。
 */
static uint8_t output_payload = 0xFFU;

/*
 * 紅外線剛收到的Payload先當作候選值。
 * 連續收到足夠次數後，才成為正式狀態。
 */
static uint8_t candidate_payload = 0xFFU;
static uint8_t candidate_count = 0U;
static uint32_t candidate_last_tick = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */
/* DWT微秒計時 */
static void DWT_Delay_Init(void);
static uint32_t DWT_TicksPerUs(void);

/* 判斷量到的時間是否落在範圍內 */
static uint8_t Time_InRange(
    uint32_t value,
    uint32_t minimum,
    uint32_t maximum);

/* 等待GPIO變成指定狀態 */
static uint8_t IR_WaitForState(
    GPIO_PinState state,
    uint32_t timeout_us);

/* 量測GPIO維持指定狀態多久 */
static uint8_t IR_MeasureState(
    GPIO_PinState state,
    uint32_t timeout_us,
    uint32_t *duration_us);

/* 接收並解出一個完整紅外線封包 */
static IR_RxStatus_t IR_DecodeFrame(uint8_t *payload);

/* 候選封包確認邏輯 */
static void Candidate_Reset(void);
static void Candidate_CheckTimeout(void);
static uint8_t Get_RequiredConfirmCount(AlertEvent_t alert);
static void Process_ValidPayload(uint8_t payload);

/* 板載LED狀態顯示 */
static void Panel_ShowUnknown(void);
static void Panel_ShowSafe(void);
static void Panel_ShowLevel1(void);
static void Panel_ShowLevel2(void);
static void Panel_ShowLevel3(void);
static void Panel_ShowError(void);
static void Panel_Update(AlertEvent_t alert);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void DFPlayer_SendCommand(uint8_t command, uint16_t parameter)
{
    uint8_t frame[10];

    frame[0] = 0x7E;                         // 起始碼
    frame[1] = 0xFF;                         // 版本
    frame[2] = 0x06;                         // 資料長度
    frame[3] = command;                      // 命令
    frame[4] = 0x00;                         // 不要求回覆
    frame[5] = (uint8_t)(parameter >> 8);    // 參數高位
    frame[6] = (uint8_t)(parameter & 0xFF);  // 參數低位

    uint16_t checksum = (uint16_t)(
        0 - (frame[1] + frame[2] + frame[3]
           + frame[4] + frame[5] + frame[6])
    );

    frame[7] = (uint8_t)(checksum >> 8);
    frame[8] = (uint8_t)(checksum & 0xFF);
    frame[9] = 0xEF;                         // 結束碼

    HAL_UART_Transmit(&huart2, frame, 10, 100);

    /* 避免命令傳送太密集 */
    HAL_Delay(100);
}


/* 設定音量，範圍0～30 */
static void DFPlayer_SetVolume(uint8_t volume)
{
    if (volume > 30)
    {
        volume = 30;
    }

    DFPlayer_SendCommand(0x06, volume);
}


/* 選擇microSD卡作為播放來源 */
static void DFPlayer_SelectSD(void)
{
    DFPlayer_SendCommand(0x09, 0x0002);
}


/* 播放 /mp3/0001.mp3、0002.mp3等 */
static void DFPlayer_PlayMp3(uint16_t track)
{
    DFPlayer_SendCommand(0x12, track);
}


/* 停止播放 */
static void DFPlayer_Stop(void)
{
    DFPlayer_SendCommand(0x16, 0x0000);
}
static void Monitor_Print(const char *message)
{
    HAL_UART_Transmit(
        &huart3,
        (uint8_t *)message,
        strlen(message),
        100
    );
}

static void Monitor_PrintAlert(AlertEvent_t alert)
{
    switch (alert)
    {
        case ALERT_SAFE:
            Monitor_Print("SAFE\r\n");
            break;

        case ALERT_LEVEL1:
            Monitor_Print("LEVEL1\r\n");
            break;

        case ALERT_LEVEL2:
            Monitor_Print("LEVEL2\r\n");
            break;

        case ALERT_LEVEL3:
            Monitor_Print("LEVEL3\r\n");
            break;

        case ALERT_SYSTEM_ERROR:
            Monitor_Print("ERROR\r\n");
            break;

        default:
            Monitor_Print("INVALID\r\n");
            break;
    }
}

static AlertEvent_t IR_DecodePayload(uint8_t payload)
{
    uint8_t event_type;
    uint8_t level;
    uint8_t received_check;
    uint8_t calculated_check;

    /*
     * 格式：TTT LL CCC
     *
     * bit7～bit5：TTT
     * bit4～bit3：LL
     * bit2～bit0：CCC
     */
    event_type = (payload >> 5) & 0x07;
    level = (payload >> 3) & 0x03;
    received_check = payload & 0x07;

    /*
     * CCC = TTT XOR 0LL
     * level為2 bit，放在uint8_t中前面自然補0。
     */
    calculated_check = event_type ^ level;

    /* 檢查碼不一致，表示資料可能傳錯 */
    if (received_check != calculated_check)
    {
        return ALERT_INVALID;
    }

    /* 安全：000 00 000 */
    if ((event_type == 0x00) && (level == 0x00))
    {
        return ALERT_SAFE;
    }

    /* 弱勢用路人穿越：TTT = 001 */
    if (event_type == 0x01)
    {
        switch (level)
        {
            case 1:
                return ALERT_LEVEL1;

            case 2:
                return ALERT_LEVEL2;

            case 3:
                return ALERT_LEVEL3;

            default:
                return ALERT_INVALID;
        }
    }

    /* 系統錯誤：TTT = 111、LL = 00 */
    if ((event_type == 0x07) && (level == 0x00))
    {
        return ALERT_SYSTEM_ERROR;
    }

    return ALERT_INVALID;
}
static void Process_AlertEvent(AlertEvent_t alert)
{
    /* 檢查碼錯誤或未知事件，直接忽略 */
    if (alert == ALERT_INVALID)
    {
        return;
    }

    /*
     * 安全與Level 1不播放語音，
     * 同時解除上一次語音事件鎖定。
     */
    if ((alert == ALERT_SAFE) ||
    (alert == ALERT_LEVEL1))
{
    DFPlayer_Stop();
    last_voice_event = ALERT_SAFE;
    return;
}

    /* 相同事件持續收到時，不重新播放 */
    if (alert == last_voice_event)
    {
        return;
    }

    last_voice_event = alert;

    switch (alert)
    {
        case ALERT_LEVEL2:
            DFPlayer_PlayMp3(1);
            break;

        case ALERT_LEVEL3:
            DFPlayer_PlayMp3(2);
            break;

        case ALERT_SYSTEM_ERROR:
            DFPlayer_PlayMp3(3);
            break;

        default:
            break;
    }
}
static void DWT_Delay_Init(void)
{
    /*
     * 啟用Cortex-M4內部的cycle counter。
     * 它會隨著CPU clock一直累加。
     */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0U;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t DWT_TicksPerUs(void)
{
    /*
     * 你的CPU為168 MHz：
     * 每1 µs大約有168個cycle。
     */
    return SystemCoreClock / 1000000U;
}

static uint8_t Time_InRange(
    uint32_t value,
    uint32_t minimum,
    uint32_t maximum)
{
    if ((value >= minimum) && (value <= maximum))
    {
        return 1U;
    }

    return 0U;
}
static uint8_t IR_WaitForState(
    GPIO_PinState state,
    uint32_t timeout_us)
{
    uint32_t start_tick;
    uint32_t timeout_ticks;

    start_tick = DWT->CYCCNT;

    timeout_ticks =
        timeout_us * DWT_TicksPerUs();

    /*
     * 一直等待PB0變成指定狀態。
     * 但不能無限等待，所以加入timeout。
     */
    while (HAL_GPIO_ReadPin(IR_RX_PORT, IR_RX_PIN) != state)
    {
        if ((DWT->CYCCNT - start_tick) >= timeout_ticks)
        {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t IR_MeasureState(
    GPIO_PinState state,
    uint32_t timeout_us,
    uint32_t *duration_us)
{
    uint32_t start_tick;
    uint32_t elapsed_ticks;
    uint32_t timeout_ticks;

    timeout_ticks =
        timeout_us * DWT_TicksPerUs();

    /*
     * 呼叫此函式時，GPIO應該已經處於要量測的狀態。
     */
    if (HAL_GPIO_ReadPin(IR_RX_PORT, IR_RX_PIN) != state)
    {
        return 0U;
    }

    start_tick = DWT->CYCCNT;

    /*
     * GPIO仍然維持此狀態，就繼續計時。
     */
    while (HAL_GPIO_ReadPin(IR_RX_PORT, IR_RX_PIN) == state)
    {
        elapsed_ticks = DWT->CYCCNT - start_tick;

        if (elapsed_ticks >= timeout_ticks)
        {
            if (duration_us != NULL)
            {
                *duration_us =
                    elapsed_ticks / DWT_TicksPerUs();
            }

            return 0U;
        }
    }

    elapsed_ticks = DWT->CYCCNT - start_tick;

    if (duration_us != NULL)
    {
        *duration_us =
            elapsed_ticks / DWT_TicksPerUs();
    }

    return 1U;
}
static IR_RxStatus_t IR_DecodeFrame(uint8_t *payload)
{
    uint32_t frame_start_tick;
    uint32_t frame_timeout_ticks;
    uint32_t duration = 0U;

    uint8_t data = 0U;
    uint8_t found_start = 0U;

    if (payload == NULL)
    {
        return IR_RX_INVALID;
    }

    frame_start_tick = DWT->CYCCNT;

    frame_timeout_ticks =
        IR_FRAME_WAIT_TIMEOUT_US * DWT_TicksPerUs();

    /*
     * 第一部分：尋找9 ms LOW起始碼。
     */
    while ((DWT->CYCCNT - frame_start_tick) <
           frame_timeout_ticks)
    {
        if (!IR_WaitForState(IR_ACTIVE_STATE, 2000U))
        {
            continue;
        }

        if (!IR_MeasureState(
                IR_ACTIVE_STATE,
                IR_PULSE_TIMEOUT_US,
                &duration))
        {
            return IR_RX_INVALID;
        }

        if (Time_InRange(
                duration,
                IR_START_MARK_MIN_US,
                IR_START_MARK_MAX_US))
        {
            found_start = 1U;
            break;
        }
    }

    if (!found_start)
    {
        return IR_RX_NO_FRAME;
    }

    /*
     * 第二部分：量測Start後面的4.5 ms HIGH。
     */
    if (!IR_MeasureState(
            IR_IDLE_STATE,
            IR_PULSE_TIMEOUT_US,
            &duration))
    {
        return IR_RX_INVALID;
    }

    if (!Time_InRange(
            duration,
            IR_START_SPACE_MIN_US,
            IR_START_SPACE_MAX_US))
    {
        return IR_RX_INVALID;
    }

    /*
     * 第三部分：接收8個bit。
     *
     * 發射端由bit7開始傳，也就是MSB first。
     */
    for (int8_t bit_index = 7;
         bit_index >= 0;
         bit_index--)
    {
        /*
         * 每一個bit都先出現約562 µs LOW。
         */
        if (!IR_WaitForState(IR_ACTIVE_STATE, 3000U))
        {
            return IR_RX_INVALID;
        }

        if (!IR_MeasureState(
                IR_ACTIVE_STATE,
                IR_PULSE_TIMEOUT_US,
                &duration))
        {
            return IR_RX_INVALID;
        }

        if (!Time_InRange(
                duration,
                IR_BIT_MARK_MIN_US,
                IR_BIT_MARK_MAX_US))
        {
            return IR_RX_INVALID;
        }

        /*
         * 接著量測HIGH時間：
         *
         * 約562 µs  → bit 0
         * 約1687 µs → bit 1
         */
        if (!IR_MeasureState(
                IR_IDLE_STATE,
                IR_PULSE_TIMEOUT_US,
                &duration))
        {
            return IR_RX_INVALID;
        }

        if (Time_InRange(
                duration,
                IR_ZERO_SPACE_MIN_US,
                IR_ZERO_SPACE_MAX_US))
        {
            /* bit為0，data預設就是0，不必修改 */
        }
        else if (Time_InRange(
                     duration,
                     IR_ONE_SPACE_MIN_US,
                     IR_ONE_SPACE_MAX_US))
        {
            /*
             * 這一位是1，就把對應bit設成1。
             */
            data |= (uint8_t)(1U << bit_index);
        }
        else
        {
            return IR_RX_INVALID;
        }
    }

    /*
     * 第四部分：檢查最後約562 µs LOW停止碼。
     */
    if (!IR_WaitForState(IR_ACTIVE_STATE, 3000U))
    {
        return IR_RX_INVALID;
    }

    if (!IR_MeasureState(
            IR_ACTIVE_STATE,
            IR_PULSE_TIMEOUT_US,
            &duration))
    {
        return IR_RX_INVALID;
    }

    if (!Time_InRange(
            duration,
            IR_STOP_MARK_MIN_US,
            IR_STOP_MARK_MAX_US))
    {
        return IR_RX_INVALID;
    }

    /*
     * 到這裡代表完整封包成功。
     */
    *payload = data;

    return IR_RX_OK;
}
static void Candidate_Reset(void)
{
    candidate_payload = 0xFFU;
    candidate_count = 0U;
    candidate_last_tick = 0U;
}

static void Candidate_CheckTimeout(void)
{
    uint32_t now;

    now = HAL_GetTick();

    /*
     * 有正在確認的候選資料時，
     * 超過700 ms沒再收到，就取消。
     */
    if (candidate_count > 0U)
    {
        if ((now - candidate_last_tick) >
            CANDIDATE_TIMEOUT_MS)
        {
            Candidate_Reset();
        }
    }
}

static uint8_t Get_RequiredConfirmCount(AlertEvent_t alert)
{
    if (alert == ALERT_SAFE)
    {
        return SAFE_CONFIRM_COUNT;
    }

    if (alert == ALERT_SYSTEM_ERROR)
    {
        return ERROR_CONFIRM_COUNT;
    }

    if ((alert == ALERT_LEVEL1) ||
        (alert == ALERT_LEVEL2) ||
        (alert == ALERT_LEVEL3))
    {
        return EVENT_CONFIRM_COUNT;
    }

    return 0U;
}
static void Panel_ShowUnknown(void)
{
    /* 藍燈：尚未收到有效狀態 */
    HAL_GPIO_WritePin(
        GPIOD,
        LD6_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        GPIOD,
        LD3_Pin | LD4_Pin | LD5_Pin,
        GPIO_PIN_RESET);
}

static void Panel_ShowSafe(void)
{
    /* 綠燈：安全 */
    HAL_GPIO_WritePin(
        GPIOD,
        LD4_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        GPIOD,
        LD3_Pin | LD5_Pin | LD6_Pin,
        GPIO_PIN_RESET);
}

static void Panel_ShowLevel1(void)
{
    /* 橘燈：Level 1 */
    HAL_GPIO_WritePin(
        GPIOD,
        LD3_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        GPIOD,
        LD4_Pin | LD5_Pin | LD6_Pin,
        GPIO_PIN_RESET);
}

static void Panel_ShowLevel2(void)
{
    /* 紅燈：Level 2 */
    HAL_GPIO_WritePin(
        GPIOD,
        LD5_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        GPIOD,
        LD3_Pin | LD4_Pin | LD6_Pin,
        GPIO_PIN_RESET);
}

static void Panel_ShowLevel3(void)
{
    /* 紅燈＋藍燈：Level 3 */
    HAL_GPIO_WritePin(
        GPIOD,
        LD5_Pin | LD6_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        GPIOD,
        LD3_Pin | LD4_Pin,
        GPIO_PIN_RESET);
}

static void Panel_ShowError(void)
{
    /* 橘＋紅＋藍：系統錯誤 */
    HAL_GPIO_WritePin(
        GPIOD,
        LD3_Pin | LD5_Pin | LD6_Pin,
        GPIO_PIN_SET);

    HAL_GPIO_WritePin(
        GPIOD,
        LD4_Pin,
        GPIO_PIN_RESET);
}

static void Panel_Update(AlertEvent_t alert)
{
    switch (alert)
    {
        case ALERT_SAFE:
            Panel_ShowSafe();
            break;

        case ALERT_LEVEL1:
            Panel_ShowLevel1();
            break;

        case ALERT_LEVEL2:
            Panel_ShowLevel2();
            break;

        case ALERT_LEVEL3:
            Panel_ShowLevel3();
            break;

        case ALERT_SYSTEM_ERROR:
            Panel_ShowError();
            break;

        default:
            Panel_ShowUnknown();
            break;
    }
}
static void Process_ValidPayload(uint8_t payload)
{
    AlertEvent_t alert;
    uint8_t required_count;

    /*
     * 使用你原本的函式：
     * 拆出TTT、LL、CCC並驗證檢查碼。
     */
    alert = IR_DecodePayload(payload);

    /*
     * 檢查碼錯誤或未知事件：
     * 這一包不用，也清除候選次數。
     */
    if (alert == ALERT_INVALID)
    {
        Candidate_Reset();
        return;
    }

    Candidate_CheckTimeout();

    required_count =
        Get_RequiredConfirmCount(alert);

    if (required_count == 0U)
    {
        Candidate_Reset();
        return;
    }

    /*
     * 這次Payload和上次候選Payload相同：
     * 確認次數加1。
     */
    if (payload == candidate_payload)
    {
        if (candidate_count < 255U)
        {
            candidate_count++;
        }
    }
    else
    {
        /*
         * 收到不同Payload：
         * 改成新的候選狀態，重新從1開始。
         */
        candidate_payload = payload;
        candidate_count = 1U;
    }

    candidate_last_tick = HAL_GetTick();

    /*
     * 收到足夠次數才正式確認。
     */
    if (candidate_count >= required_count)
    {
        /*
         * 和目前已輸出的狀態不同，才更新。
         * 避免發射器持續傳送時一直重播語音。
         */
        if (output_payload != payload)
        {
            output_payload = payload;

            /* 更新STM32板上的LED */
            /* 更新STM32板上的LED */
            Panel_Update(alert);

            /* Level 2、Level 3、Error觸發語音 */
            Process_AlertEvent(alert);

            /* USART3傳給電腦Python監控 */
            Monitor_PrintAlert(alert);
        }

        /*
         * 不讓candidate_count無限增加。
         */
        if (candidate_count > required_count)
        {
            candidate_count = required_count;
        }
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
  /* 儲存紅外線解出的8-bit Payload */
  uint8_t received_payload = 0U;

  /* 儲存這次紅外線接收結果 */
  IR_RxStatus_t rx_status = IR_RX_NO_FRAME;


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
  MX_I2C1_Init();
  MX_I2S3_Init();
  MX_SPI1_Init();
  MX_USB_HOST_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

/*
 * 啟用DWT計時器。
 * 紅外線解碼要靠它量測微秒時間。
 */


 
DWT_Delay_Init();
Monitor_Print("RECEIVER READY\r\n");

/* 初始化紅外線候選狀態 */
output_payload = 0xFFU;
Candidate_Reset();

/* 尚未收到任何有效事件，先顯示藍燈 */
Panel_ShowUnknown();

/* 等待DFPlayer與microSD完成啟動 */
HAL_Delay(3000);

/* 選擇microSD */
DFPlayer_SelectSD();

HAL_Delay(500);

/* 設定音量 */
DFPlayer_SetVolume(15);

HAL_Delay(500);
/* 初始化完成後，再通知電腦監控視窗 */
Monitor_Print("RECEIVER READY\r\n");

/*
 * 不再放Process_IR_Payload(0x33)之類固定測試碼。
 * 之後由紅外線真正收到的資料決定。
 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
{
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

    /*
     * 檢查候選封包是否等太久。
     */
    Candidate_CheckTimeout();

    /*
     * 嘗試接收一個完整紅外線封包。
     */
    rx_status =
        IR_DecodeFrame(&received_payload);

    if (rx_status == IR_RX_OK)
    {
        /*
         * 已經成功解出8-bit，
         * 再進行檢查碼、連續次數與語音處理。
         */
        Process_ValidPayload(received_payload);
    }
    else if (rx_status == IR_RX_INVALID)
    {
        /*
         * 收到不符合協定的波形。
         * 不改變目前正式狀態，但清除候選次數。
         */
        Candidate_Reset();
    }
    else
    {
        /*
         * IR_RX_NO_FRAME：
         * 目前沒有收到封包，保持原本狀態。
         */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_96K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_I2C_SPI_Pin */
  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 BOOT1_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
