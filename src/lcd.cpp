#include "lcd.h"

LiquidCrystal_I2C lcd(0x21, 16, 2);

void lcd_init() {
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    
    lcd.setCursor(1, 0);
    lcd.print("EMBEDDED SYSTEM");
    
    lcd.setCursor(3, 1);
    lcd.print("ASSIGNMENT");
    
    delay(5000);
    lcd.clear();
}