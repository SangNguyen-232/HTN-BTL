#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <PubSubClient.h>

extern PubSubClient client;

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

extern bool web_led1_control_enabled; 
extern bool web_led2_control_enabled;

extern float glob_anomaly_score;
extern String glob_anomaly_message;

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
extern SemaphoreHandle_t xMutexSensorData;
extern SemaphoreHandle_t xMutexPumpControl;

// Pump threshold settings (for AUTO mode)
extern float pump_threshold_min;
extern float pump_threshold_max;

// CoreIOT credentials
extern String coreiot_server;
extern String coreiot_token;
extern String coreiot_client_id;
extern String coreiot_username;
extern String coreiot_password;
extern bool coreiot_use_token;  // true = use token, false = use username/password
extern bool coreiot_reconnect_needed;  // Flag to trigger reconnect when credentials change

// Helper function to get all sensor data atomically (thread-safe)
void getSensorData(SensorData_t* data);

// Config management functions
void loadCoreIOTCredentials();
void saveCoreIOTCredentials(const String& server, const String& token, const String& clientId, const String& username, const String& password, bool useToken);

// Pump threshold management functions
void loadPumpThresholds();
void savePumpThresholds(float min, float max);
#endif