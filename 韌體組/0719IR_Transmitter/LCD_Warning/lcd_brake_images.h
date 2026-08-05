#ifndef LCD_BRAKE_IMAGES_H
#define LCD_BRAKE_IMAGES_H

#include <stdint.h>

#define LCD_BRAKE_IMAGE_WIDTH          320U
#define LCD_BRAKE_IMAGE_HEIGHT         240U
#define LCD_BRAKE_IMAGE_PIXEL_COUNT    \
    (LCD_BRAKE_IMAGE_WIDTH * LCD_BRAKE_IMAGE_HEIGHT)

/* RGB565：每個像素使用一個 uint16_t */
extern const uint16_t brake_level1_image[LCD_BRAKE_IMAGE_PIXEL_COUNT];

#endif /* LCD_BRAKE_IMAGES_H */