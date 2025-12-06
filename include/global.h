#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Sensor data structure for atomic read
typedef struct {
    float temperature;
    float humidity;
    float soil;
} SensorData_t;

extern float glob_temperature;
extern float glob_humidity;
extern float glob_anomaly_score;
extern String glob_anomaly_message;

// Sensor variables (used by adafruit.cpp and mainserver.cpp)
extern float temperature;
extern float humidity;
extern float soil_moisture_value;
extern float anomaly_score;
extern String anomaly_message;

extern bool web_led1_control_enabled; 
extern bool web_led2_control_enabled;

extern String ssid;
extern String password;
extern String wifi_ssid;
extern String wifi_password;
extern String location_label;
extern boolean isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;

// Soil sensor and pump control variables
extern float glob_soil;
extern bool pump_manual_control;
extern bool pump_state;
extern String pump_mode;  // "AUTO" or "MANUAL"
extern SemaphoreHandle_t xMutexSensorData;
extern SemaphoreHandle_t xMutexPumpControl;

// Sensor data mutex (used by adafruit.cpp)
extern SemaphoreHandle_t dataMutex;

// Serial output mutex (prevent task interleaving)
extern SemaphoreHandle_t xMutexSerial;

// Pin definitions
#define PIN_PUMP 5  // Define pump pin

// Pump threshold settings (for AUTO mode)
extern float pump_threshold_min;
extern float pump_threshold_max;

// LCD and sensor refresh rate (in seconds)
extern int lcd_refresh_rate;

// Adafruit IO credentials
extern String adafruit_username;
extern String adafruit_key;
extern bool adafruit_reconnect_needed;  // Flag to trigger reconnect when credentials change

// Helper function to get all sensor data atomically (thread-safe)
void getSensorData(SensorData_t* data);

// Config management functions
void loadAdafruitCredentials();
void saveAdafruitCredentials(const String& username, const String& key);

// Pump threshold management functions
void loadPumpThresholds();
void savePumpThresholds(float min, float max);

// LCD refresh rate management functions
void loadLCDRefreshRate();
void saveLCDRefreshRate(int rate);
#endif