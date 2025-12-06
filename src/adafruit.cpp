#include "adafruit.h"
#include "global.h"
#include "mainserver.h"

const char* ADAFRUIT_SERVER = "io.adafruit.com";
const int ADAFRUIT_PORT = 1883;

// Extern variables from global
extern float temperature;
extern float humidity;
extern float soil_moisture_value;
extern float anomaly_score;
extern String anomaly_message;
extern bool pump_state;
extern String pump_mode;
extern SemaphoreHandle_t xMutexSensorData;
extern float glob_temperature;
extern float glob_humidity;
extern float glob_soil;
extern float glob_anomaly_score;
extern String glob_anomaly_message;

// Pin definitions
#define PIN_PUMP 47

// Forward declarations
void publishPumpState();
void publishPumpMode();

WiFiClient adafruitClient;
PubSubClient mqttClient(adafruitClient);

// Helper function to build Adafruit IO topic
String buildTopic(const char* feedName) {
  return String(adafruit_username) + "/feeds/" + String(feedName);
}

// Forward declarations
void handleAdafruitMessage(char* topic, byte* payload, unsigned int length);
bool calculateAutoPumpState();

void reconnectAdafruit() {
  if (mqttClient.connected()) {
    return;  // Already connected, no need to reconnect
  }
  
  Serial.print("[Adafruit] Attempting MQTT connection to Adafruit IO...");
  
  if (adafruit_username.length() == 0 || adafruit_key.length() == 0) {
    Serial.println(" ERROR: Credentials not configured!");
    Serial.print("[Adafruit] Username: '");
    Serial.print(adafruit_username);
    Serial.print("' | Key: '");
    Serial.print(adafruit_key);
    Serial.println("'");
    Serial.println("Please configure Adafruit IO username and key in Settings page.");
    return;  // Exit, don't block
  }
  
  // Create client ID
  String clientId = "ESP32_" + String(ESP.getEfuseMac(), HEX);
  
  Serial.print(" (User: ");
  Serial.print(adafruit_username);
  Serial.println(")");
  
  // Connect with username and key (key is used as password)
  if (mqttClient.connect(clientId.c_str(), adafruit_username.c_str(), adafruit_key.c_str())) {
    Serial.println("[Adafruit] ✓ Connected to Adafruit IO!");
    
    // Subscribe to control feeds (pump state and mode)
    String pumpStateTopic = buildTopic(ADAFRUIT_FEED_PUMP_STATE);
    String pumpModeTopic = buildTopic(ADAFRUIT_FEED_PUMP_MODE);
    
    mqttClient.subscribe(pumpStateTopic.c_str());
    mqttClient.subscribe(pumpModeTopic.c_str());
    
    Serial.println("[Adafruit] Subscribed to:");
    Serial.println("  - " + pumpStateTopic);
    Serial.println("  - " + pumpModeTopic);
    
    // Publish initial states
    publishPumpState();
    publishPumpMode();
    
  } else {
    int state = mqttClient.state();
    Serial.print("[Adafruit] ✗ Failed, rc=");
    Serial.print(state);
    Serial.println(" (will retry in 5s)");
    
    // Print error description
    const char* stateStr = "";
    if (state == -4) stateStr = "CONNECT_TIMEOUT";
    else if (state == -3) stateStr = "CONNECT_FAILED";
    else if (state == -2) stateStr = "CONNECT_LOST";
    else if (state == -1) stateStr = "DISCONNECTED";
    else if (state == 0) stateStr = "CONNECTED";
    else if (state == 1) stateStr = "CONNECT_BAD_PROTOCOL";
    else if (state == 2) stateStr = "CONNECT_BAD_CLIENT_ID";
    else if (state == 3) stateStr = "CONNECT_UNAVAILABLE";
    else if (state == 4) stateStr = "CONNECT_BAD_CREDENTIALS";
    else if (state == 5) stateStr = "CONNECT_UNAUTHORIZED";
    
    Serial.print("[Adafruit] Error: ");
    Serial.println(stateStr);
  }
}

void handleAdafruitMessage(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  
  // Convert payload to string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.print("Message received on topic: ");
  Serial.println(topicStr);
  Serial.print("Payload: ");
  Serial.println(message);
  
  // Extract feed name from topic (format: username/feeds/feedname)
  int lastSlash = topicStr.lastIndexOf('/');
  if (lastSlash != -1) {
    String feedName = topicStr.substring(lastSlash + 1);
    
    if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
      if (feedName == ADAFRUIT_FEED_PUMP_STATE) {
        // Handle pump state change
        String valueStr = String(message);
        valueStr.trim();
        
        if (valueStr == "1" || valueStr.equalsIgnoreCase("ON") || valueStr.equalsIgnoreCase("true")) {
          pump_state = true;
          Serial.println("Pump state set to ON via Adafruit IO");
        } else if (valueStr == "0" || valueStr.equalsIgnoreCase("OFF") || valueStr.equalsIgnoreCase("false")) {
          pump_state = false;
          Serial.println("Pump state set to OFF via Adafruit IO");
        }
        
        // Update actual pump hardware
        digitalWrite(PIN_PUMP, pump_state ? HIGH : LOW);
        
      } else if (feedName == ADAFRUIT_FEED_PUMP_MODE) {
        // Handle pump mode change
        String valueStr = String(message);
        valueStr.trim();
        valueStr.toUpperCase();
        
        if (valueStr == "AUTO" || valueStr == "AUTOMATIC") {
          pump_mode = "AUTO";
          Serial.println("Pump mode set to AUTO via Adafruit IO");
          
          // In AUTO mode, immediately calculate and apply pump state
          bool autoState = calculateAutoPumpState();
          pump_state = autoState;
          digitalWrite(PIN_PUMP, pump_state ? HIGH : LOW);
          
          // Publish updated state
          publishPumpState();
          
        } else if (valueStr == "MANUAL") {
          pump_mode = "MANUAL";
          Serial.println("Pump mode set to MANUAL via Adafruit IO");
        }
      }
      
      xSemaphoreGive(xMutexSensorData);
    }
  }
}

bool calculateAutoPumpState() {
  // In AUTO mode: Turn pump ON if soil moisture < 30%, OFF if > 70%
  float currentSoil = glob_soil;
  
  if (currentSoil < 30.0) {
    return true;  // Soil too dry, turn pump ON
  } else if (currentSoil > 70.0) {
    return false; // Soil wet enough, turn pump OFF
  } else {
    // Hysteresis: Keep current state if in middle range
    return pump_state;
  }
}

void publishSensorData() {
  if (!mqttClient.connected()) {
    Serial.println("[Adafruit] Cannot publish - MQTT not connected!");
    return;
  }
  
  if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
    Serial.print("[Adafruit] Publishing data - T:");
    Serial.print(glob_temperature);
    Serial.print(" H:");
    Serial.print(glob_humidity);
    Serial.print(" S:");
    Serial.print(glob_soil);
    Serial.print(" AS:");
    Serial.print(glob_anomaly_score);
    Serial.print(" AM:");
    Serial.println(glob_anomaly_message);
    
    // Publish temperature
    String tempTopic = buildTopic(ADAFRUIT_FEED_TEMPERATURE);
    mqttClient.publish(tempTopic.c_str(), String(glob_temperature, 2).c_str());
    
    // Publish humidity
    String humTopic = buildTopic(ADAFRUIT_FEED_HUMIDITY);
    mqttClient.publish(humTopic.c_str(), String(glob_humidity, 2).c_str());
    
    // Publish soil moisture
    String soilTopic = buildTopic(ADAFRUIT_FEED_SOIL);
    mqttClient.publish(soilTopic.c_str(), String(glob_soil, 1).c_str());
    
    // Publish anomaly score
    String anomalyScoreTopic = buildTopic(ADAFRUIT_FEED_ANOMALY_SCORE);
    mqttClient.publish(anomalyScoreTopic.c_str(), String(glob_anomaly_score, 4).c_str());
    
    // Publish anomaly message
    String anomalyMsgTopic = buildTopic(ADAFRUIT_FEED_ANOMALY_MESSAGE);
    mqttClient.publish(anomalyMsgTopic.c_str(), glob_anomaly_message.c_str());
    
    xSemaphoreGive(xMutexSensorData);
    Serial.println("[Adafruit] ✓ All data published successfully");
  }
}

void publishPumpState() {
  if (!mqttClient.connected()) {
    return;
  }
  
  if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
    String topic = buildTopic(ADAFRUIT_FEED_PUMP_STATE);
    mqttClient.publish(topic.c_str(), pump_state ? "1" : "0");
    xSemaphoreGive(xMutexSensorData);
  }
  
  Serial.print("Pump state published: ");
  Serial.println(pump_state ? "ON" : "OFF");
}

void publishPumpMode() {
  if (!mqttClient.connected()) {
    return;
  }
  
  if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
    String topic = buildTopic(ADAFRUIT_FEED_PUMP_MODE);
    mqttClient.publish(topic.c_str(), pump_mode.c_str());
    xSemaphoreGive(xMutexSensorData);
  }
  
  Serial.print("Pump mode published: ");
  Serial.println(pump_mode);
}

void adafruit_task(void *pvParameters) {
  Serial.println("\n[Adafruit] Task started");
  
  // Initialize MQTT client
  mqttClient.setServer(ADAFRUIT_SERVER, ADAFRUIT_PORT);
  mqttClient.setCallback(handleAdafruitMessage);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  
  unsigned long lastDataPublish = millis();  // Initialize to current time
  unsigned long lastWiFiWarnLog = 0;
  unsigned long lastReconnectAttempt = 0;
  unsigned long lastDebugLog = millis();
  unsigned long loopCounter = 0;
  const unsigned long DATA_PUBLISH_INTERVAL = 5000;  // 5 seconds - publish all data
  const unsigned long RECONNECT_INTERVAL = 5000;     // Retry MQTT connection every 5 seconds
  
  Serial.println("[Adafruit] Entering main loop...");
  Serial.print("[Adafruit] Initial lastDataPublish: ");
  Serial.println(lastDataPublish);
  
  while (true) {
    loopCounter++;
    
    // Ensure WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
      // Reduce log spam - only log every 30 seconds
      if (millis() - lastWiFiWarnLog > 30000) {
        Serial.println("[Adafruit] WiFi not connected, waiting...");
        lastWiFiWarnLog = millis();
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    
    // Try to reconnect if disconnected (but not too frequently)
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
        Serial.println("[Adafruit] MQTT disconnected, attempting reconnect...");
        reconnectAdafruit();
        lastReconnectAttempt = now;
      }
    }
    
    // Process MQTT messages (called frequently to keep connection alive)
    if (mqttClient.connected()) {
      if (!mqttClient.loop()) {
        Serial.println("[Adafruit] MQTT loop failed!");
      }
    }
    
    unsigned long currentMillis = millis();
    unsigned long timeSinceLastPublish = currentMillis - lastDataPublish;
    
    // Publish all sensor data + pump state/mode every 5 seconds
    if (timeSinceLastPublish >= DATA_PUBLISH_INTERVAL) {
      if (mqttClient.connected()) {
        Serial.print("[Adafruit] Publishing (loop ");
        Serial.print(loopCounter);
        Serial.print(", time: ");
        Serial.print(timeSinceLastPublish);
        Serial.println("ms)");
        publishSensorData();
        publishPumpState();
        publishPumpMode();
        lastDataPublish = currentMillis;
        Serial.print("[Adafruit] Next publish in 5s (at ");
        Serial.print(lastDataPublish + DATA_PUBLISH_INTERVAL);
        Serial.println("ms)");
      } else {
        Serial.println("[Adafruit] MQTT not connected, skipping publish");
      }
    }
    
    // Debug log every 10 seconds
    if (currentMillis - lastDebugLog > 10000) {
      Serial.print("[Adafruit] Debug - Loop: ");
      Serial.print(loopCounter);
      Serial.print(" | Time since publish: ");
      Serial.print(currentMillis - lastDataPublish);
      Serial.print("ms | MQTT: ");
      Serial.print(mqttClient.connected() ? "CONNECTED" : "DISCONNECTED");
      Serial.print(" | WiFi: ");
      Serial.println(WiFi.isConnected() ? "CONNECTED" : "DISCONNECTED");
      loopCounter = 0;
      lastDebugLog = currentMillis;
    }
    
    // Handle AUTO mode pump control
    // First, check mode and calculate state (with mutex held briefly)
    bool shouldUpdatePump = false;
    bool newPumpState = false;
    
    if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
      if (pump_mode == "AUTO") {
        bool autoState = calculateAutoPumpState();
        if (autoState != pump_state) {
          pump_state = autoState;
          newPumpState = autoState;
          shouldUpdatePump = true;
          digitalWrite(PIN_PUMP, pump_state ? HIGH : LOW);
        }
      }
      xSemaphoreGive(xMutexSensorData);
    }
    
    // Publish after releasing mutex to avoid deadlock
    if (shouldUpdatePump && mqttClient.connected()) {
      publishPumpState();  // This function will acquire mutex internally
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

