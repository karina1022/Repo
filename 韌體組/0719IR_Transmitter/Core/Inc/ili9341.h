#ifndef INC_ILI9341_H_
#define INC_ILI9341_H_

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9341_WIDTH   320U
#define ILI9341_HEIGHT  240U

/* RGB565 colors */
#define ILI9341_BLACK       0x0000U
#define ILI9341_NAVY        0x000FU
#define ILI9341_DARKGREEN   0x03E0U
#define ILI9341_DARKCYAN    0x03EFU
#define ILI9341_MAROON      0x7800U
#define ILI9341_PURPLE      0x780FU
#define ILI9341_OLIVE       0x7BE0U
#define ILI9341_LIGHTGREY   0xC618U
#define ILI9341_DARKGREY    0x7BEFU
#define ILI9341_BLUE        0x001FU
#define ILI9341_GREEN       0x07E0U
#define ILI9341_CYAN        0x07FFU
#define ILI9341_RED         0xF800U
#define ILI9341_MAGENTA     0xF81FU
#define ILI9341_YELLOW      0xFFE0U
#define ILI9341_WHITE       0xFFFFU
#define ILI9341_ORANGE      0xFD20U
#define ILI9341_GREENYELLOW 0xAFE5U
#define ILI9341_PINK        0xFC18U

void ILI9341_Init(void);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_TestPattern(void);

void ILI9341_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
);

void ILI9341_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color
);

void ILI9341_DrawImage(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const uint16_t *image
);

/*
 * 從一張較大的 RGB565 圖片中，只取出指定矩形並畫到面板。
 *
 * destination_x / destination_y：
 *     要顯示在 LCD 上的位置。
 *
 * region_width / region_height：
 *     要更新的局部區域尺寸。
 *
 * source_image：
 *     完整 RGB565 圖片資料。
 *
 * source_width / source_height：
 *     完整來源圖片尺寸。
 *
 * source_x / source_y：
 *     要從完整圖片的哪個座標開始取資料。
 */
void ILI9341_DrawImageRegion(
    uint16_t destination_x,
    uint16_t destination_y,
    uint16_t region_width,
    uint16_t region_height,
    const uint16_t *source_image,
    uint16_t source_width,
    uint16_t source_height,
    uint16_t source_x,
    uint16_t source_y
);

void ILI9341_DrawIndexed4(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const uint16_t *palette,
    const uint8_t *pixels
);

#ifdef __cplusplus
}
#endif

#endif /* INC_ILI9341_H_ */
