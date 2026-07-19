#ifndef LCD_IMAGES_H_
#define LCD_IMAGES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_IMAGE_WIDTH        320U
#define LCD_IMAGE_HEIGHT       240U
#define LCD_IMAGE_PIXEL_COUNT  (LCD_IMAGE_WIDTH * LCD_IMAGE_HEIGHT)

/*
 * RGB565 image data, row-major order:
 * index = y * LCD_IMAGE_WIDTH + x
 *
 * Each image contains 320 x 240 = 76,800 pixels.
 */
extern const uint16_t LCD_SAFE_IMAGE[LCD_IMAGE_PIXEL_COUNT];
extern const uint16_t LCD_LEVEL1_IMAGE[LCD_IMAGE_PIXEL_COUNT];
extern const uint16_t LCD_LEVEL2_IMAGE[LCD_IMAGE_PIXEL_COUNT];
extern const uint16_t LCD_LEVEL3_IMAGE[LCD_IMAGE_PIXEL_COUNT];
extern const uint16_t LCD_SYSTEM_ERROR_IMAGE[LCD_IMAGE_PIXEL_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* LCD_IMAGES_H_ */
