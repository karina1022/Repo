#ifndef LCD_BRAKE_IMAGES_H
#define LCD_BRAKE_IMAGES_H

#include <stdint.h>

#define LCD_BRAKE_IMAGE_WIDTH          320U
#define LCD_BRAKE_IMAGE_HEIGHT         240U
#define LCD_BRAKE_IMAGE_PIXEL_COUNT    (LCD_BRAKE_IMAGE_WIDTH * LCD_BRAKE_IMAGE_HEIGHT)
#define LCD_BRAKE_IMAGE_PACKED_BYTES   ((LCD_BRAKE_IMAGE_PIXEL_COUNT + 1U) / 2U)
#define LCD_INDEXED4_PALETTE_SIZE      16U

typedef struct
{
    uint16_t width;
    uint16_t height;
    const uint16_t *palette;
    const uint8_t *pixels;
} LCD_Indexed4Image;

extern const uint16_t brake_level1_palette[LCD_INDEXED4_PALETTE_SIZE];
extern const uint8_t brake_level1_pixels[LCD_BRAKE_IMAGE_PACKED_BYTES];
extern const LCD_Indexed4Image brake_level1_image;

extern const uint16_t brake_level2_palette[LCD_INDEXED4_PALETTE_SIZE];
extern const uint8_t brake_level2_pixels[LCD_BRAKE_IMAGE_PACKED_BYTES];
extern const LCD_Indexed4Image brake_level2_image;

extern const uint16_t brake_level3_palette[LCD_INDEXED4_PALETTE_SIZE];
extern const uint8_t brake_level3_pixels[LCD_BRAKE_IMAGE_PACKED_BYTES];
extern const LCD_Indexed4Image brake_level3_image;

#endif /* LCD_BRAKE_IMAGES_4BIT_H */
