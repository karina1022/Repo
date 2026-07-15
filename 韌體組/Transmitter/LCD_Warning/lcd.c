#include "lcd.h"
#include "lcd_images.h"
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