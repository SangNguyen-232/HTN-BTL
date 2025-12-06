#include "global.h"
#include <Preferences.h>

float glob_temperature = 0;
float glob_humidity = 0;
float glob_anomaly_score = 0;
String glob_anomaly_message = "Normal";

// Sensor variables (used by adafruit.cpp and mainserver.cpp)
float temperature = 0;
float humidity = 0;
float soil_moisture_value = 0;
float anomaly_score = 0;
String anomaly_message = "Normal";

bool web_led1_control_enabled = false;
bool web_led2_control_enabled = false;

String ssid = "L06 - NHÓM 7!!!";
String password = "12345678";
String wifi_ssid = "";
String wifi_password = "";
String location_label = "Thành phố Hồ Chí Minh";
boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();

float glob_soil = 0;
bool pump_manual_control = false;
bool pump_state = false;
String pump_mode = "AUTO";  // Default to AUTO mode
SemaphoreHandle_t xMutexSensorData = xSemaphoreCreateMutex();
SemaphoreHandle_t xMutexPumpControl = xSemaphoreCreateMutex();
SemaphoreHandle_t dataMutex = xSemaphoreCreateMutex();  // Used by adafruit.cpp

// Pump threshold settings (default values)
float pump_threshold_min = 20.0f;
float pump_threshold_max = 80.0f;

// LCD and sensor refresh rate (default 3 seconds)
int lcd_refresh_rate = 3;

// Adafruit IO credentials - defaults
String adafruit_username = "";
String adafruit_key = "";
bool adafruit_reconnect_needed = false;  // Flag to trigger reconnect

Preferences preferences;

void loadAdafruitCredentials() {
  preferences.begin("adafruit", true);  // Read-only mode
  adafruit_username = preferences.getString("username", "");
  adafruit_key = preferences.getString("key", "");
  preferences.end();
  
  Serial.println("Loaded Adafruit IO config:");
  Serial.print("  Username: ");
  Serial.println(adafruit_username.length() > 0 ? adafruit_username : String("Not set"));
  Serial.print("  Key: ");
  if (adafruit_key.length() > 0) {
    Serial.println("***" + adafruit_key.substring(adafruit_key.length()-4));
  } else {
    Serial.println("Not set");
  }
}

void saveAdafruitCredentials(const String& username, const String& key) {
  // Check if credentials actually changed
  bool changed = (adafruit_username != username || adafruit_key != key);
  
  preferences.begin("adafruit", false);  // Read-write mode
  preferences.putString("username", username);
  preferences.putString("key", key);
  preferences.end();
  
  // Update global variables
  adafruit_username = username;
  adafruit_key = key;
  
  // Set flag to trigger reconnect if credentials changed
  if (changed) {
    adafruit_reconnect_needed = true;
  }
  
  Serial.println("Saved Adafruit IO credentials");
  if (changed) {
    Serial.println("Credentials changed - reconnect will be triggered");
  }
}

// Helper function to get all sensor data atomically (thread-safe)
void getSensorData(SensorData_t* data) {
    if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
        data->temperature = glob_temperature;
        data->humidity = glob_humidity;
        data->soil = glob_soil;
        xSemaphoreGive(xMutexSensorData);
    } else {
        // Fallback (should never happen with portMAX_DELAY, but for safety)
        data->temperature = glob_temperature;
        data->humidity = glob_humidity;
        data->soil = glob_soil;
    }
}

// Load pump thresholds from preferences
void loadPumpThresholds() {
  preferences.begin("pump", true);  // Read-only mode
  pump_threshold_min = preferences.getFloat("threshold_min", 20.0f);
  pump_threshold_max = preferences.getFloat("threshold_max", 80.0f);
  preferences.end();
  
}

// Save pump thresholds to preferences
void savePumpThresholds(float min, float max) {
  preferences.begin("pump", false);  // Read-write mode
  preferences.putFloat("threshold_min", min);
  preferences.putFloat("threshold_max", max);
  preferences.end();
  
  // Update global variables
  pump_threshold_min = min;
  pump_threshold_max = max;
  
  Serial.println("Saved pump thresholds:");
  Serial.println("  Min: " + String(pump_threshold_min) + "%");
  Serial.println("  Max: " + String(pump_threshold_max) + "%");
}

// Load LCD refresh rate from preferences
void loadLCDRefreshRate() {
  preferences.begin("display", true);  // Read-only mode
  lcd_refresh_rate = preferences.getInt("lcd_refresh", 3);  // Default 3 seconds
  preferences.end();
  
  Serial.println("Loaded LCD refresh rate: " + String(lcd_refresh_rate) + "s");
}

// Save LCD refresh rate to preferences
void saveLCDRefreshRate(int rate) {
  preferences.begin("display", false);  // Read-write mode
  preferences.putInt("lcd_refresh", rate);
  preferences.end();
  
  // Update global variable
  lcd_refresh_rate = rate;
  
  Serial.println("Saved LCD refresh rate: " + String(lcd_refresh_rate) + "s");
}