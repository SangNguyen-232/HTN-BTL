#include "global.h"
#include <Preferences.h>

float glob_temperature = 0;
float glob_humidity = 0;
float glob_anomaly_score = 0;
String glob_anomaly_message = "Normal";

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
SemaphoreHandle_t xMutexSensorData = xSemaphoreCreateMutex();
SemaphoreHandle_t xMutexPumpControl = xSemaphoreCreateMutex();

// Pump threshold settings (default values)
float pump_threshold_min = 20.0f;
float pump_threshold_max = 80.0f;

// LCD and sensor refresh rate (default 3 seconds)
int lcd_refresh_rate = 3;

// CoreIOT credentials - defaults
String coreiot_server = "app.coreiot.io";
String coreiot_token = "";
String coreiot_client_id = "ESP32Client";  // Default client ID
String coreiot_username = "";
String coreiot_password = "";
bool coreiot_use_token = true;  // Default to token authentication
bool coreiot_reconnect_needed = false;  // Flag to trigger reconnect

Preferences preferences;

void loadCoreIOTCredentials() {
  preferences.begin("coreiot", true);  // Read-only mode
  coreiot_server = preferences.getString("server", "app.coreiot.io");
  coreiot_token = preferences.getString("token", "");
  coreiot_client_id = preferences.getString("client_id", "ESP32Client");
  coreiot_username = preferences.getString("username", "");
  coreiot_password = preferences.getString("password", "");
  coreiot_use_token = preferences.getBool("use_token", true);
  preferences.end();
  
  Serial.println("Loaded CoreIOT config:");
  Serial.println("  Server: " + coreiot_server);
  Serial.println("  Use Token: " + String(coreiot_use_token ? "Yes" : "No"));
  if (coreiot_use_token) {
    if (coreiot_token.length() > 0) {
      Serial.println("  Token: ***" + coreiot_token.substring(coreiot_token.length()-4));
    } else {
      Serial.println("  Token: Not set");
    }
  } else {
    Serial.print("  Client ID: ");
    Serial.println(coreiot_client_id.length() > 0 ? coreiot_client_id : String("ESP32Client"));
    Serial.print("  Username: ");
    Serial.println(coreiot_username.length() > 0 ? coreiot_username : String("Not set"));
    Serial.print("  Password: ");
    Serial.println(coreiot_password.length() > 0 ? String("***") : String("Not set"));
  }
}

void saveCoreIOTCredentials(const String& server, const String& token, const String& clientId, const String& username, const String& password, bool useToken) {
  // Check if credentials actually changed
  bool changed = (coreiot_server != server || 
                  coreiot_token != token || 
                  coreiot_client_id != clientId ||
                  coreiot_username != username || 
                  coreiot_password != password || 
                  coreiot_use_token != useToken);
  
  preferences.begin("coreiot", false);  // Read-write mode
  preferences.putString("server", server);
  preferences.putString("token", token);
  preferences.putString("client_id", clientId);
  preferences.putString("username", username);
  preferences.putString("password", password);
  preferences.putBool("use_token", useToken);
  preferences.end();
  
  // Update global variables
  coreiot_server = server;
  coreiot_token = token;
  coreiot_client_id = clientId;
  coreiot_username = username;
  coreiot_password = password;
  coreiot_use_token = useToken;
  
  // Set flag to trigger reconnect if credentials changed
  if (changed) {
    coreiot_reconnect_needed = true;
  }
  
  Serial.println("Saved CoreIOT credentials");
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