#include "ili9341.h"

#include "spi.h"

/*
 * STM32F407G-DISC1 -> ILI9341 wiring
 * PB13 -> SCK
 * PB15 -> SDI/MOSI
 * PB12 -> CS
 * PC4  -> DC/RS
 * PC5  -> RST
 *
 * SPI2 must be configured as:
 * CPOL = High, CPHA = 2 Edge (SPI Mode 3)
 * 8-bit, MSB first, software NSS.
 */

#define LCD_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)

#define LCD_DC_CMD()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET)
#define LCD_DC_DATA()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET)

#define LCD_RST_LOW()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET)
#define LCD_RST_HIGH() HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET)

//#define LCD_SDI_LOW()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET)
//#define LCD_SDI_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET)


static void LCD_Transmit(const uint8_t *data, uint16_t size)
{
    if ((data == NULL) || (size == 0U))
    {
        return;
    }

    if (HAL_SPI_Transmit(&hspi2, (uint8_t *)data, size, 1000U) != HAL_OK)
    {
        Error_Handler();
    }

    while (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY) != RESET)
    {
        /* Wait until the last bit has physically left the SPI peripheral. */
    }
}


/*
 * Match the referenced STM32 example:
 * every command byte has its own CS-low / CS-high transaction.
 */
static void LCD_WriteIndex(uint8_t command)
{
    LCD_CS_LOW();
    LCD_DC_CMD();

    LCD_Transmit(&command, 1U);

    LCD_CS_HIGH();
}


/*
 * Match the referenced STM32 example:
 * every parameter byte has its own CS-low / CS-high transaction.
 */
static void LCD_WriteData8(uint8_t data)
{
    LCD_CS_LOW();
    LCD_DC_DATA();

    LCD_Transmit(&data, 1U);

    LCD_CS_HIGH();
}


static void LCD_WriteData16(uint16_t data)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)(data >> 8);
    bytes[1] = (uint8_t)(data & 0xFFU);

    LCD_CS_LOW();
    LCD_DC_DATA();

    LCD_Transmit(bytes, 2U);

    LCD_CS_HIGH();
}


static void LCD_Reset(void)
{
    LCD_CS_HIGH();
    LCD_DC_DATA();

    LCD_RST_LOW();
    HAL_Delay(100U);

    LCD_RST_HIGH();
    HAL_Delay(300U);
}


static void LCD_SetRegion(
    uint16_t xStart,
    uint16_t yStart,
    uint16_t xEnd,
    uint16_t yEnd)
{
    LCD_WriteIndex(0x2AU);
    LCD_WriteData16(xStart);
    LCD_WriteData16(xEnd);

    LCD_WriteIndex(0x2BU);
    LCD_WriteData16(yStart);
    LCD_WriteData16(yEnd);

    LCD_WriteIndex(0x2CU);
}


void ILI9341_Init(void)
{
    LCD_Reset();

    LCD_WriteIndex(0x01U);
    HAL_Delay(150U);

    /*
     * Initialization sequence from the vendor STM32 example in this package.
     */
    LCD_WriteIndex(0xCBU);
    LCD_WriteData8(0x39U);
    LCD_WriteData8(0x2CU);
    LCD_WriteData8(0x00U);
    LCD_WriteData8(0x34U);
    LCD_WriteData8(0x02U);

    LCD_WriteIndex(0xCFU);
    LCD_WriteData8(0x00U);
    LCD_WriteData8(0xC1U);
    LCD_WriteData8(0x30U);

    LCD_WriteIndex(0xE8U);
    LCD_WriteData8(0x85U);
    LCD_WriteData8(0x00U);
    LCD_WriteData8(0x78U);

    LCD_WriteIndex(0xEAU);
    LCD_WriteData8(0x00U);
    LCD_WriteData8(0x00U);

    LCD_WriteIndex(0xEDU);
    LCD_WriteData8(0x64U);
    LCD_WriteData8(0x03U);
    LCD_WriteData8(0x12U);
    LCD_WriteData8(0x81U);

    LCD_WriteIndex(0xF7U);
    LCD_WriteData8(0x20U);

    LCD_WriteIndex(0xC0U);
    LCD_WriteData8(0x23U);

    LCD_WriteIndex(0xC1U);
    LCD_WriteData8(0x10U);

    LCD_WriteIndex(0xC5U);
    LCD_WriteData8(0x3EU);
    LCD_WriteData8(0x28U);

    LCD_WriteIndex(0xC7U);
    LCD_WriteData8(0x86U);

    /* Portrait orientation, BGR order. */
    LCD_WriteIndex(0x36U);
    LCD_WriteData8(0x28U);

    /* RGB565, 16-bit pixel data. */
    LCD_WriteIndex(0x3AU);
    LCD_WriteData8(0x55U);

    LCD_WriteIndex(0xB1U);
    LCD_WriteData8(0x00U);
    LCD_WriteData8(0x18U);

    LCD_WriteIndex(0xB6U);
    LCD_WriteData8(0x08U);
    LCD_WriteData8(0x82U);
    LCD_WriteData8(0x27U);

    LCD_WriteIndex(0xF2U);
    LCD_WriteData8(0x00U);

    LCD_WriteIndex(0x26U);
    LCD_WriteData8(0x01U);

    LCD_WriteIndex(0xE0U);
    LCD_WriteData8(0x0FU);
    LCD_WriteData8(0x31U);
    LCD_WriteData8(0x2BU);
    LCD_WriteData8(0x0CU);
    LCD_WriteData8(0x0EU);
    LCD_WriteData8(0x08U);
    LCD_WriteData8(0x4EU);
    LCD_WriteData8(0xF1U);
    LCD_WriteData8(0x37U);
    LCD_WriteData8(0x07U);
    LCD_WriteData8(0x10U);
    LCD_WriteData8(0x03U);
    LCD_WriteData8(0x0EU);
    LCD_WriteData8(0x09U);
    LCD_WriteData8(0x00U);

    LCD_WriteIndex(0xE1U);
    LCD_WriteData8(0x00U);
    LCD_WriteData8(0x0EU);
    LCD_WriteData8(0x14U);
    LCD_WriteData8(0x03U);
    LCD_WriteData8(0x11U);
    LCD_WriteData8(0x07U);
    LCD_WriteData8(0x31U);
    LCD_WriteData8(0xC1U);
    LCD_WriteData8(0x48U);
    LCD_WriteData8(0x08U);
    LCD_WriteData8(0x0FU);
    LCD_WriteData8(0x0CU);
    LCD_WriteData8(0x31U);
    LCD_WriteData8(0x36U);
    LCD_WriteData8(0x0FU);

    LCD_WriteIndex(0x11U);
    HAL_Delay(120U);

    LCD_WriteIndex(0x29U);
    HAL_Delay(20U);

    LCD_WriteIndex(0x2CU);
}


void ILI9341_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    uint8_t buffer[128];
    uint32_t remainingPixels;
    uint32_t pixelsThisTransfer;

    if ((width == 0U) || (height == 0U))
    {
        return;
    }

    if ((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT))
    {
        return;
    }

    if (((uint32_t)x + width) > ILI9341_WIDTH)
    {
        width = (uint16_t)(ILI9341_WIDTH - x);
    }

    if (((uint32_t)y + height) > ILI9341_HEIGHT)
    {
        height = (uint16_t)(ILI9341_HEIGHT - y);
    }

    for (uint16_t i = 0U; i < sizeof(buffer); i += 2U)
    {
        buffer[i] = (uint8_t)(color >> 8);
        buffer[i + 1U] = (uint8_t)(color & 0xFFU);
    }

    LCD_SetRegion(
        x,
        y,
        (uint16_t)(x + width - 1U),
        (uint16_t)(y + height - 1U)
    );

    remainingPixels = (uint32_t)width * (uint32_t)height;

    /*
     * The article's clear-screen function keeps CS low for the entire
     * pixel stream after the address window has been selected.
     */
    LCD_CS_LOW();
    LCD_DC_DATA();

    while (remainingPixels > 0U)
    {
        pixelsThisTransfer = remainingPixels;

        if (pixelsThisTransfer > (sizeof(buffer) / 2U))
        {
            pixelsThisTransfer = sizeof(buffer) / 2U;
        }

        LCD_Transmit(
            buffer,
            (uint16_t)(pixelsThisTransfer * 2U)
        );

        remainingPixels -= pixelsThisTransfer;
    }

    LCD_CS_HIGH();
}


void ILI9341_FillScreen(uint16_t color)
{
    ILI9341_FillRect(
        0U,
        0U,
        ILI9341_WIDTH,
        ILI9341_HEIGHT,
        color
    );
}


void ILI9341_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color)
{
    ILI9341_FillRect(x, y, 1U, 1U, color);
}

void ILI9341_DrawImage(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const uint16_t *image)
{
    uint8_t buffer[128];

    uint32_t totalPixels;
    uint32_t pixelIndex = 0U;
    uint32_t remainingPixels;

    if ((image == NULL) || (width == 0U) || (height == 0U))
    {
        return;
    }

    /* 確認圖片沒有超出螢幕範圍 */
    if (((uint32_t)x + (uint32_t)width) > ILI9341_WIDTH)
    {
        return;
    }

    if (((uint32_t)y + (uint32_t)height) > ILI9341_HEIGHT)
    {
        return;
    }

    /* 設定要寫入圖片的顯示範圍 */
    LCD_SetRegion(
        x,
        y,
        (uint16_t)(x + width - 1U),
        (uint16_t)(y + height - 1U)
    );

    totalPixels = (uint32_t)width * (uint32_t)height;

    LCD_CS_LOW();
    LCD_DC_DATA();

    while (pixelIndex < totalPixels)
    {
        uint16_t pixelsThisTransfer;
        uint16_t bufferIndex = 0U;

        remainingPixels = totalPixels - pixelIndex;

        /*
         * buffer 為 128 bytes。
         * 一個 RGB565 像素是 2 bytes，
         * 所以每次最多傳送 64 個像素。
         */
        if (remainingPixels > 64U)
        {
            pixelsThisTransfer = 64U;
        }
        else
        {
            pixelsThisTransfer = (uint16_t)remainingPixels;
        }

        for (uint16_t i = 0U; i < pixelsThisTransfer; i++)
        {
            uint16_t color = image[pixelIndex + i];

            /* ILI9341 需要高位元組先送 */
            buffer[bufferIndex] =
                (uint8_t)(color >> 8);

            buffer[bufferIndex + 1U] =
                (uint8_t)(color & 0xFFU);

            bufferIndex += 2U;
        }

        LCD_Transmit(
            buffer,
            (uint16_t)(pixelsThisTransfer * 2U)
        );

        pixelIndex += pixelsThisTransfer;
    }

    LCD_CS_HIGH();
}

void ILI9341_DrawImageRegion(
    uint16_t destination_x,
    uint16_t destination_y,
    uint16_t region_width,
    uint16_t region_height,
    const uint16_t *source_image,
    uint16_t source_width,
    uint16_t source_height,
    uint16_t source_x,
    uint16_t source_y)
{
    uint8_t buffer[128];
    uint16_t row;

    if ((source_image == NULL) ||
        (region_width == 0U) ||
        (region_height == 0U) ||
        (source_width == 0U) ||
        (source_height == 0U))
    {
        return;
    }

    /* 確認來源矩形沒有超出原始圖片。 */
    if (((uint32_t)source_x + region_width) > source_width)
    {
        return;
    }

    if (((uint32_t)source_y + region_height) > source_height)
    {
        return;
    }

    /* 確認目的矩形沒有超出 LCD。 */
    if (((uint32_t)destination_x + region_width) > ILI9341_WIDTH)
    {
        return;
    }

    if (((uint32_t)destination_y + region_height) > ILI9341_HEIGHT)
    {
        return;
    }

    LCD_SetRegion(
        destination_x,
        destination_y,
        (uint16_t)(destination_x + region_width - 1U),
        (uint16_t)(destination_y + region_height - 1U)
    );

    LCD_CS_LOW();
    LCD_DC_DATA();

    /*
     * LCD 的目的區域是一個連續矩形，但來源圖片每列的跨度是
     * source_width，因此逐列取出需要的區段，再連續送往 LCD。
     */
    for (row = 0U; row < region_height; row++)
    {
        const uint16_t *source_row;
        uint16_t remaining_pixels = region_width;
        uint16_t column = 0U;

        source_row =
            source_image +
            ((uint32_t)(source_y + row) * source_width) +
            source_x;

        while (remaining_pixels > 0U)
        {
            uint16_t pixels_this_transfer;
            uint16_t buffer_index = 0U;
            uint16_t i;

            pixels_this_transfer = remaining_pixels;

            if (pixels_this_transfer > 64U)
            {
                pixels_this_transfer = 64U;
            }

            for (i = 0U; i < pixels_this_transfer; i++)
            {
                uint16_t color = source_row[column + i];

                buffer[buffer_index] =
                    (uint8_t)(color >> 8U);

                buffer[buffer_index + 1U] =
                    (uint8_t)(color & 0xFFU);

                buffer_index += 2U;
            }

            LCD_Transmit(
                buffer,
                (uint16_t)(pixels_this_transfer * 2U)
            );

            column =
                (uint16_t)(column + pixels_this_transfer);

            remaining_pixels =
                (uint16_t)(remaining_pixels - pixels_this_transfer);
        }
    }

    LCD_CS_HIGH();
}

void ILI9341_DrawIndexed4(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const uint16_t *palette,
    const uint8_t *pixels)
{
    uint8_t buffer[128];

    uint32_t totalPixels;
    uint32_t pixelIndex = 0U;
    uint32_t remainingPixels;

    /*
     * palette：16個RGB565顏色
     * pixels ：4-bit索引資料，每個byte存兩個像素
     */
    if ((palette == NULL) ||
        (pixels == NULL) ||
        (width == 0U) ||
        (height == 0U))
    {
        return;
    }

    /*
     * 確認圖片沒有超出面板範圍。
     */
    if (((uint32_t)x + (uint32_t)width) > ILI9341_WIDTH)
    {
        return;
    }

    if (((uint32_t)y + (uint32_t)height) > ILI9341_HEIGHT)
    {
        return;
    }

    /*
     * 設定要寫入圖片的顯示範圍。
     */
    LCD_SetRegion(
        x,
        y,
        (uint16_t)(x + width - 1U),
        (uint16_t)(y + height - 1U)
    );

    totalPixels = (uint32_t)width * (uint32_t)height;

    LCD_CS_LOW();
    LCD_DC_DATA();

    while (pixelIndex < totalPixels)
    {
        uint16_t pixelsThisTransfer;
        uint16_t bufferIndex = 0U;

        remainingPixels = totalPixels - pixelIndex;

        /*
         * buffer為128 bytes。
         * 顯示時仍傳送RGB565，所以一個像素是2 bytes。
         * 每次最多處理64個像素。
         */
        if (remainingPixels > 64U)
        {
            pixelsThisTransfer = 64U;
        }
        else
        {
            pixelsThisTransfer =
                (uint16_t)remainingPixels;
        }

        for (uint16_t i = 0U;
             i < pixelsThisTransfer;
             i++)
        {
            uint32_t currentPixel;
            uint8_t packedByte;
            uint8_t colorIndex;
            uint16_t color;

            currentPixel = pixelIndex + i;

            /*
             * 一個byte存兩個像素：
             *
             * 高4 bit：偶數像素
             * 低4 bit：奇數像素
             */
            packedByte = pixels[currentPixel >> 1U];

            if ((currentPixel & 0x01U) == 0U)
            {
                colorIndex =
                    (packedByte >> 4U) & 0x0FU;
            }
            else
            {
                colorIndex =
                    packedByte & 0x0FU;
            }

            /*
             * 使用索引到調色盤取出真正的RGB565顏色。
             */
            color = palette[colorIndex];

            /*
             * ILI9341要求RGB565高位元組先送。
             */
            buffer[bufferIndex] =
                (uint8_t)(color >> 8U);

            buffer[bufferIndex + 1U] =
                (uint8_t)(color & 0xFFU);

            bufferIndex += 2U;
        }

        LCD_Transmit(
            buffer,
            (uint16_t)(pixelsThisTransfer * 2U)
        );

        pixelIndex += pixelsThisTransfer;
    }

    LCD_CS_HIGH();
}
void ILI9341_TestPattern(void)
{
    ILI9341_FillRect(0U,   0U, 240U, 64U, ILI9341_RED);
    ILI9341_FillRect(0U,  64U, 240U, 64U, ILI9341_GREEN);
    ILI9341_FillRect(0U, 128U, 240U, 64U, ILI9341_BLUE);
    ILI9341_FillRect(0U, 192U, 240U, 64U, ILI9341_WHITE);
    ILI9341_FillRect(0U, 256U, 240U, 64U, ILI9341_YELLOW);
}
