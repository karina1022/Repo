#include "lcd.h"
#include "lcd_images.h"
#include "lcd_brake_images.h"
#include "ili9341.h"

#include <stdint.h>


typedef enum
{
    LCD_SCREEN_UNKNOWN = 0U,
    LCD_SCREEN_SAFE,
    LCD_SCREEN_VRU_LEVEL1,
    LCD_SCREEN_VRU_LEVEL2,
    LCD_SCREEN_VRU_LEVEL3,
    LCD_SCREEN_BRAKE,
    LCD_SCREEN_ERROR
} LCD_ScreenId;


static LCD_ScreenId lcd_current_screen = LCD_SCREEN_UNKNOWN;


/*
 * 新的 Level 1 / 2 / 3 圖片除了顏色不同，
 * 下方行動指示及部分圖形位置也不同。
 * 為避免局部更新後殘留上一張圖片的像素，
 * 等級切換時改為完整更新 320 x 240 畫面。
 */
static void LCD_ShowVRUScreen(
    LCD_ScreenId target_screen,
    const uint16_t *target_image)
{
    if ((target_image == 0) || (lcd_current_screen == target_screen))
    {
        return;
    }

    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        target_image
    );

    lcd_current_screen = target_screen;
}


void LCD_ResetScreenState(void)
{
    lcd_current_screen = LCD_SCREEN_UNKNOWN;
}


void LCD_ShowSafeScreen(void)
{
    if (lcd_current_screen == LCD_SCREEN_SAFE)
    {
        return;
    }

    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_SAFE_IMAGE
    );

    lcd_current_screen = LCD_SCREEN_SAFE;
}


void LCD_ShowLevel1Screen(void)
{
    LCD_ShowVRUScreen(
        LCD_SCREEN_VRU_LEVEL1,
        LCD_LEVEL1_IMAGE
    );
}


void LCD_ShowLevel2Screen(void)
{
    LCD_ShowVRUScreen(
        LCD_SCREEN_VRU_LEVEL2,
        LCD_LEVEL2_IMAGE
    );
}


void LCD_ShowLevel3Screen(void)
{
    LCD_ShowVRUScreen(
        LCD_SCREEN_VRU_LEVEL3,
        LCD_LEVEL3_IMAGE
    );
}


void LCD_ShowSystemErrorScreen(void)
{
    if (lcd_current_screen == LCD_SCREEN_ERROR)
    {
        return;
    }

    ILI9341_DrawImage(
        0U,
        0U,
        LCD_IMAGE_WIDTH,
        LCD_IMAGE_HEIGHT,
        LCD_SYSTEM_ERROR_IMAGE
    );

    lcd_current_screen = LCD_SCREEN_ERROR;
}


void LCD_ShowBrakeLevel1Screen(void)
{
    if (lcd_current_screen == LCD_SCREEN_BRAKE)
    {
        return;
    }

    /*
     * 目前的 lcd_brake_images.c 已經是 RGB565 uint16_t 圖片，
     * 因此必須使用 ILI9341_DrawImage()，不能再呼叫 DrawIndexed4()。
     */
    ILI9341_DrawImage(
        0U,
        0U,
        LCD_BRAKE_IMAGE_WIDTH,
        LCD_BRAKE_IMAGE_HEIGHT,
        brake_level1_image
    );

    lcd_current_screen = LCD_SCREEN_BRAKE;
}


/*
 * 現在前前車緊急煞車只有單一等級。
 * 保留舊函式名稱，避免 main.c 尚未清除舊呼叫時發生連結錯誤。
 */
void LCD_ShowBrakeLevel2Screen(void)
{
    LCD_ShowBrakeLevel1Screen();
}


void LCD_ShowBrakeLevel3Screen(void)
{
    LCD_ShowBrakeLevel1Screen();
}