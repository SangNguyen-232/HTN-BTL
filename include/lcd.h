#ifndef __LCD__
#define __LCD__
#include <Arduino.h>
#include "LiquidCrystal_I2C.h"

extern LiquidCrystal_I2C lcd;
void lcd_init();

#endif