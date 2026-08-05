#ifndef LCD_H_
#define LCD_H_

void LCD_ShowSafeScreen(void);
void LCD_ShowLevel1Screen(void);
void LCD_ShowLevel2Screen(void);
void LCD_ShowLevel3Screen(void);

void LCD_ShowBrakeLevel1Screen(void);
void LCD_ShowBrakeLevel2Screen(void);
void LCD_ShowBrakeLevel3Screen(void);

void LCD_ShowSystemErrorScreen(void);

/*
 * LCD 重新初始化後可呼叫一次，讓下一次顯示強制完整刷新。
 */
void LCD_ResetScreenState(void);

#endif /* LCD_H_ */
