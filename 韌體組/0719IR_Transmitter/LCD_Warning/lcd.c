#include "lcd.h"
#include "lcd_images.h"
#include "lcd_brake_images.h"
#include "ili9341.h"


void LCD_ShowSafeScreen(void)
{
    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_SAFE_IMAGE
    );
}


void LCD_ShowLevel1Screen(void)
{
    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_LEVEL1_IMAGE
    );
}

void LCD_ShowLevel2Screen(void)
{
    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_LEVEL2_IMAGE
    );
}


void LCD_ShowLevel3Screen(void)
{
    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_LEVEL3_IMAGE
    );
}


void LCD_ShowSystemErrorScreen(void)
{
    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_SYSTEM_ERROR_IMAGE
    );
}
void LCD_ShowBrakeLevel1Screen(void)
{
    ILI9341_DrawIndexed4(
        0U,
        0U,
        LCD_BRAKE_IMAGE_WIDTH,
        LCD_BRAKE_IMAGE_HEIGHT,
        brake_level1_image.palette,
        brake_level1_image.pixels
    );
}

void LCD_ShowBrakeLevel2Screen(void)
{
    ILI9341_DrawIndexed4(
        0U,
        0U,
        LCD_BRAKE_IMAGE_WIDTH,
        LCD_BRAKE_IMAGE_HEIGHT,
       brake_level2_image.palette,
       brake_level2_image.pixels
    );
}

void LCD_ShowBrakeLevel3Screen(void)
{
    ILI9341_DrawIndexed4(
        0U,
        0U,
        LCD_BRAKE_IMAGE_WIDTH,
        LCD_BRAKE_IMAGE_HEIGHT,
        brake_level3_image.palette,
        brake_level3_image.pixels
    );
}