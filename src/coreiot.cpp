#include "coreiot.h"
#include "global.h"
#include "mainserver.h"

const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// Forward declarations
void handleAttributesUpdate(const char* jsonData);
void handleRPCRequest(String topicStr, const char* jsonData);
bool calculateAutoPumpState();  // Helper to calculate AUTO pump state based on soil moisture

// Send shared attributes to CoreIOT to define available RPC methods
void sendSharedAttributes() {
  // Define available RPC methods and their parameters
  // This helps CoreIOT dashboard understand what controls are available

  //In ra Serial
  char attributes[512];
  snprintf(attributes, sizeof(attributes),
    "{"
    "\"rpcMethods\":{"
      "\"setPumpState\":{"
        "\"description\":\"Set pump state ON/OFF\","
        "\"params\":{"
          "\"type\":\"boolean\","
          "\"description\":\"true=ON, false=OFF\""
        "}"
      "},"
      "\"setPumpMode\":{"
        "\"description\":\"Set pump mode AUTO/MANUAL\","
        "\"params\":{"
          "\"type\":\"string\","
          "\"enum\":[\"AUTO\",\"MANUAL\"],"
          "\"description\":\"AUTO or MANUAL mode\""
        "}"
      "}"
    "},"
    "\"deviceCapabilities\":{"
      "\"pump\":{"
        "\"state\":\"boolean\","
        "\"mode\":\"string\""
      "},"
      "\"sensors\":{"
        "\"temperature\":\"float\","
        "\"humidity\":\"float\","
        "\"soil_moisture\":\"float\""
      "}"
    "}"
    "}"
  );
  
  if (client.publish("v1/devices/me/attributes", attributes)) {
    Serial.println("Shared attributes published successfully");
    Serial.print("Attributes: ");
    Serial.println(attributes);
  } else {
    Serial.println("Failed to publish shared attributes");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(coreiot_server);
    Serial.print("...");
    
    bool connected = false;
    
    if (coreiot_use_token) {
      // Use token authentication (username=token, password=empty)
      if (coreiot_token.length() == 0) {
        Serial.println(" ERROR: Token not configured!");
        Serial.println("Please configure CoreIOT credentials in Settings page.");
        delay(5000);
        continue;
      }
      // For token auth, use token as username and default client ID
      String clientId = coreiot_client_id.length() > 0 ? coreiot_client_id : "ESP32Client";
      connected = client.connect(clientId.c_str(), coreiot_token.c_str(), NULL);
    } else {
      // Use username/password authentication with client ID
      if (coreiot_username.length() == 0 || coreiot_password.length() == 0) {
        Serial.println(" ERROR: Username/Password not configured!");
        Serial.println("Please configure CoreIOT credentials in Settings page.");
        delay(5000);
        continue;
      }
      // Use custom client ID if provided, otherwise default
      String clientId = coreiot_client_id.length() > 0 ? coreiot_client_id : "ESP32Client";
      Serial.print("Connecting with ClientID: ");
      Serial.println(clientId);
      connected = client.connect(clientId.c_str(), coreiot_username.c_str(), coreiot_password.c_str());
    }
    
    if (connected) {
      Serial.println(" connected to CoreIOT Server!");
      client.subscribe("v1/devices/me/rpc/request/+");
      Serial.println("Subscribed to v1/devices/me/rpc/request/+");
      
      // Subscribe to attributes to receive data from CoreIOT
      client.subscribe("v1/devices/me/attributes");
      Serial.println("Subscribed to v1/devices/me/attributes");
      
      // Request latest attributes from server
      client.publish("v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"temperature,humidity,soil_moisture_value,soil_moisture,pump_state,pump_mode\"}");
      Serial.println("Requested attributes from CoreIOT server");
      
      // Send shared attributes to define RPC methods available
      sendSharedAttributes();
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  // Allocate a temporary buffer for the message
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Payload: ");
  Serial.println(message);

  String topicStr = String(topic);
  
  // Check if this is an attributes update (from server)
  if (topicStr == "v1/devices/me/attributes" || topicStr.startsWith("v1/devices/me/attributes/")) {
    // This is attributes data from CoreIOT server
    handleAttributesUpdate(message);
    return;
  }
  
  // Check if this is an RPC request
  if (topicStr.startsWith("v1/devices/me/rpc/request/")) {
    handleRPCRequest(topicStr, message);
    return;
  }
  
  Serial.println("Unknown topic: " + topicStr);
}

// Helper function to calculate AUTO pump state based on soil moisture
// Logic: bật khi độ ẩm < min, tắt khi độ ẩm >= max
bool calculateAutoPumpState() {
  float current_soil;
  bool current_pump_state;
  
  if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
    current_soil = glob_soil;
    xSemaphoreGive(xMutexSensorData);
  } else {
    // Fallback nếu không lấy được mutex
    current_soil = glob_soil;
  }
  
  if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
    current_pump_state = pump_state;
    xSemaphoreGive(xMutexPumpControl);
  } else {
    current_pump_state = pump_state;
  }
  
  // AUTO mode: bật khi độ ẩm < min, tắt khi độ ẩm >= max
  bool auto_state;
  if (current_pump_state) {
    // Nếu pump đang bật, chỉ tắt khi đạt ngưỡng max
    auto_state = (current_soil < pump_threshold_max);
  } else {
    // Nếu pump đang tắt, chỉ bật khi dưới ngưỡng min
    auto_state = (current_soil < pump_threshold_min);
  }
  
  Serial.println("[AUTO] Calculated pump state from soil moisture: " + String(current_soil) + "% (min:" + String(pump_threshold_min) + "%, max:" + String(pump_threshold_max) + "%) -> " + String(auto_state ? "ON" : "OFF"));
  return auto_state;
}

// Helper function to parse attributes JSON object
bool parseAttributesObject(JsonObject obj, void* coreiotDataPtr) {
  // CoreIOTData is defined in mainserver.h
  CoreIOTData* data = (CoreIOTData*)coreiotDataPtr;
  bool updated = false;
  
  // Update temperature
  if (obj.containsKey("temperature")) {
    data->temperature = obj["temperature"].as<float>();
    updated = true;
  }
  
  // Update humidity
  if (obj.containsKey("humidity")) {
    data->humidity = obj["humidity"].as<float>();
    updated = true;
  }
  
  // Update soil moisture (can be from different keys)
  if (obj.containsKey("soil_moisture_value")) {
    data->soil = obj["soil_moisture_value"].as<float>();
    updated = true;
  } else if (obj.containsKey("soil")) {
    data->soil = obj["soil"].as<float>();
    updated = true;
  } else if (obj.containsKey("soil_moisture")) {
    // Can be string like "45" or number
    if (obj["soil_moisture"].is<float>()) {
      data->soil = obj["soil_moisture"].as<float>();
    } else if (obj["soil_moisture"].is<const char*>()) {
      data->soil = atof(obj["soil_moisture"].as<const char*>());
    }
    updated = true;
  }
  
  // Update pump state
  if (obj.containsKey("pump_state")) {
    if (obj["pump_state"].is<bool>()) {
      data->pump_state = obj["pump_state"].as<bool>();
    } else if (obj["pump_state"].is<const char*>()) {
      String stateStr = obj["pump_state"].as<const char*>();
      data->pump_state = (stateStr == "true" || stateStr == "ON" || stateStr == "1");
    }
    updated = true;
  }
  
  // Update pump mode (can be string "MANUAL"/"AUTO" or boolean true/false)
  if (obj.containsKey("pump_mode")) {
    if (obj["pump_mode"].is<bool>()) {
      // Boolean: true = MANUAL, false = AUTO
      data->pump_mode = obj["pump_mode"].as<bool>() ? "MANUAL" : "AUTO";
    } else if (obj["pump_mode"].is<int>()) {
      // Integer: non-zero = MANUAL, zero = AUTO
      data->pump_mode = (obj["pump_mode"].as<int>() != 0) ? "MANUAL" : "AUTO";
    } else {
      // String: "MANUAL" or "AUTO"
      data->pump_mode = obj["pump_mode"].as<String>();
    }
    updated = true;
  }
  
  // Update anomaly score if available
  if (obj.containsKey("anomaly_score")) {
    data->anomaly_score = obj["anomaly_score"].as<float>();
    updated = true;
  }
  
  // Update anomaly message if available
  if (obj.containsKey("anomaly_message")) {
    data->anomaly_message = obj["anomaly_message"].as<String>();
    updated = true;
  }
  
  return updated;
}

// Handle attributes update from CoreIOT server
void handleAttributesUpdate(const char* jsonData) {
  Serial.println("[CoreIOT] Received attributes update");
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    Serial.print("[CoreIOT] JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }
  
  bool dataUpdated = false;
  
  // CoreIOTData is defined in mainserver.h, extern variables in mainserver.cpp
  
  // Check for shared attributes (from server)
  if (doc.containsKey("shared")) {
    JsonObject shared = doc["shared"];
    dataUpdated = parseAttributesObject(shared, &coreiot_data);
  }
  
  // Check for client attributes (from server) 
  if (doc.containsKey("client")) {
    JsonObject client = doc["client"];
    if (parseAttributesObject(client, &coreiot_data)) {
      dataUpdated = true;
    }
  }
  
  // Check for direct attributes (if sent as flat JSON)
  if (doc.containsKey("temperature") || doc.containsKey("humidity") || 
      doc.containsKey("soil_moisture_value") || doc.containsKey("pump_state") ||
      doc.containsKey("pump_mode")) {
    dataUpdated = parseAttributesObject(doc.as<JsonObject>(), &coreiot_data);
  }
  
  if (dataUpdated) {

    coreiot_data.last_update = millis();
    coreiot_data.is_valid = true;
    use_coreiot_data = true;
    
    // ĐỒNG BỘ: Khi nhận attributes từ CoreIOT, cập nhật local pump state/mode
    // Kiểm tra xem có pump_state hoặc pump_mode trong attributes không
    bool pumpUpdated = false;
    if ((doc.containsKey("shared") && (doc["shared"].containsKey("pump_state") || doc["shared"].containsKey("pump_mode"))) ||
        (doc.containsKey("client") && (doc["client"].containsKey("pump_state") || doc["client"].containsKey("pump_mode"))) ||
        doc.containsKey("pump_state") || doc.containsKey("pump_mode")) {
      pumpUpdated = true;
    }
    
    if (pumpUpdated) {
      // Cập nhật local pump state và mode từ coreiot_data
      bool autoStateCalculated = false;
      bool calculated_pump_state = false;
      
      if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
        bool wasManual = pump_manual_control;
        bool newMode = (coreiot_data.pump_mode == "MANUAL");
        pump_manual_control = newMode;
        
        // Nếu chuyển sang AUTO mode, tính toán lại pump_state dựa trên soil moisture
        if (!newMode && wasManual) {
          // Chuyển từ MANUAL sang AUTO: tính toán lại state
          calculated_pump_state = calculateAutoPumpState();
          pump_state = calculated_pump_state;
          coreiot_data.pump_state = calculated_pump_state;  // Cập nhật lại coreiot_data với state mới
          autoStateCalculated = true;
          Serial.println("[AUTO] Switched to AUTO mode from attributes, calculated new pump state: " + String(calculated_pump_state ? "ON" : "OFF"));
        } else {
          // Giữ nguyên state nếu đang ở MANUAL hoặc đã ở AUTO
          pump_state = coreiot_data.pump_state;
        }
        
        xSemaphoreGive(xMutexPumpControl);
        Serial.println("[SYNC] Local pump state/mode updated from CoreIOT attributes");
        Serial.println("  Pump State: " + String(pump_state ? "ON" : "OFF"));
        Serial.println("  Pump Mode: " + String(pump_manual_control ? "MANUAL" : "AUTO"));
      }
      
      // Nếu vừa tính toán AUTO state, publish lại lên CoreIOT để đồng bộ
      if (autoStateCalculated && client.connected()) {
        char attr[128];
        snprintf(attr, sizeof(attr),
            "{\"pump_state\": %s, \"pump_mode\": %s}",
            calculated_pump_state ? "true" : "false",
            "false"  // AUTO mode = false
        );
        if (client.publish("v1/devices/me/attributes", attr)) {
          Serial.println("[SYNC] Auto-calculated pump state pushed to CoreIOT:");
          Serial.println(attr);
        } else {
          Serial.println("[SYNC] Failed to publish auto-calculated pump state to CoreIOT");
        }
      }
    }
    
    Serial.println("[CoreIOT] Attributes updated successfully");
    Serial.println("  Temperature: " + String(coreiot_data.temperature));
    Serial.println("  Humidity: " + String(coreiot_data.humidity));
    Serial.println("  Soil: " + String(coreiot_data.soil));
    Serial.println("  Pump State: " + String(coreiot_data.pump_state ? "ON" : "OFF"));
    Serial.println("  Pump Mode: " + coreiot_data.pump_mode);
  }
}

// Handle RPC request from CoreIOT
void handleRPCRequest(String topicStr, const char* jsonData) {
  // Extract request ID from topic: v1/devices/me/rpc/request/{requestId}
  int requestIdStart = topicStr.lastIndexOf('/') + 1;
  String requestId = topicStr.substring(requestIdStart);
  Serial.print("Request ID: ");
  Serial.println(requestId);

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    // Send error response
    char errorResponse[128];
    snprintf(errorResponse, sizeof(errorResponse), "{\"error\":\"Failed to parse JSON\"}");
    String responseTopic = "v1/devices/me/rpc/response/" + requestId;
    client.publish(responseTopic.c_str(), errorResponse);
    return;
  }

  const char* method = doc["method"];
  bool success = false;
  String resultMsg = "";

  // Handle setPumpState method
  if (strcmp(method, "setPumpState") == 0) {
    // Params can be: true/false or "ON"/"OFF"
    bool newState = false;
    
    if (doc["params"].is<bool>()) {
      newState = doc["params"].as<bool>();
    } else if (doc["params"].is<const char*>()) {
      const char* params = doc["params"].as<const char*>();
      newState = (strcmp(params, "ON") == 0 || strcmp(params, "true") == 0 || strcmp(params, "1") == 0);
    } else if (doc["params"].is<int>()) {
      newState = (doc["params"].as<int>() != 0);
    }

    // Update pump state (thread-safe)
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      // If switching to MANUAL control, enable manual mode
      bool wasAuto = !pump_manual_control;
      if (wasAuto) {
        pump_state = newState;
        pump_manual_control = true;
        Serial.println("Switched to MANUAL mode via CoreIOT");
      }
      pump_state = newState;
      bool current_pump_mode = pump_manual_control;
      xSemaphoreGive(xMutexPumpControl);
      
      // ĐỒNG BỘ: Cập nhật coreiot_data để mainserver/LCD hiển thị đúng
      coreiot_data.pump_state = newState;
      coreiot_data.pump_mode = current_pump_mode ? "MANUAL" : "AUTO";
      coreiot_data.last_update = millis();
      coreiot_data.is_valid = true;
      use_coreiot_data = true;
      
      success = true;
      resultMsg = "Pump state set to " + String(newState ? "ON" : "OFF");
      Serial.println("CoreIOT RPC: " + resultMsg);
      Serial.println("[SYNC] Updated coreiot_data with pump state from CoreIOT");
      
      // ĐỒNG BỘ: Publish attributes lên CoreIOT để đồng bộ với website
      char attr[128];
      snprintf(attr, sizeof(attr),
          "{\"pump_state\": %s, \"pump_mode\": %s}",
          newState ? "true" : "false",
          current_pump_mode ? "true" : "false"
      );
      if (client.publish("v1/devices/me/attributes", attr)) {
        Serial.println("[SYNC] Pump state pushed to CoreIOT after RPC update:");
        Serial.println(attr);
      } else {
        Serial.println("[SYNC] Failed to publish pump state to CoreIOT");
      }
    } else {
      resultMsg = "Failed to acquire mutex";
    }

  }
  // Handle setPumpMode method
  else if (strcmp(method, "setPumpMode") == 0) {
    // Params: true = MANUAL, false = AUTO
    bool isManual = false;
    
    if (doc["params"].is<bool>()) {
      isManual = doc["params"].as<bool>();
    } else if (doc["params"].is<const char*>()) {
      // Support legacy string format for backward compatibility
      const char* params = doc["params"].as<const char*>();
      isManual = (strcmp(params, "MANUAL") == 0 || strcmp(params, "manual") == 0);
    } else if (doc["params"].is<int>()) {
      isManual = (doc["params"].as<int>() != 0);
    }

    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      bool wasManual = pump_manual_control;
      pump_manual_control = isManual;
      bool current_pump_state;
      
      // Nếu chuyển sang AUTO mode, tính toán lại pump_state dựa trên soil moisture
      if (!isManual && wasManual) {
        // Chuyển từ MANUAL sang AUTO: tính toán lại state
        current_pump_state = calculateAutoPumpState();
        pump_state = current_pump_state;
        Serial.println("[AUTO] Switched to AUTO mode, calculated new pump state: " + String(current_pump_state ? "ON" : "OFF"));
      } else {
        // Giữ nguyên state nếu đang ở MANUAL hoặc đã ở AUTO
        current_pump_state = pump_state;
      }
      
      xSemaphoreGive(xMutexPumpControl);
      
      // ĐỒNG BỘ: Cập nhật coreiot_data để mainserver/LCD hiển thị đúng
      coreiot_data.pump_state = current_pump_state;
      coreiot_data.pump_mode = isManual ? "MANUAL" : "AUTO";
      coreiot_data.last_update = millis();
      coreiot_data.is_valid = true;
      use_coreiot_data = true;
      
      success = true;
      resultMsg = "Pump mode set to " + String(isManual ? "MANUAL" : "AUTO");
      Serial.println("CoreIOT RPC: " + resultMsg);
      Serial.println("[SYNC] Updated coreiot_data with pump mode from CoreIOT");
      
      // ĐỒNG BỘ: Publish attributes lên CoreIOT để đồng bộ với website
      char attr[128];
      snprintf(attr, sizeof(attr),
          "{\"pump_state\": %s, \"pump_mode\": %s}",
          current_pump_state ? "true" : "false",
          isManual ? "true" : "false"
      );
      if (client.publish("v1/devices/me/attributes", attr)) {
        Serial.println("[SYNC] Pump mode pushed to CoreIOT after RPC update:");
        Serial.println(attr);
      } else {
        Serial.println("[SYNC] Failed to publish pump mode to CoreIOT");
      }
    } else {
      resultMsg = "Failed to acquire mutex";
    }

  }

  // Handle getPumpMode method (alias for pump_manual_control)
  // This is used by Core IOT app to retrieve current pump mode value
  else if (strcmp(method, "pump_manual_control") == 0 || strcmp(method, "getPumpMode") == 0) {
    // Return current pump mode: true = MANUAL, false = AUTO
    bool current_mode;
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      current_mode = pump_manual_control;
      xSemaphoreGive(xMutexPumpControl);
    } else {
      current_mode = pump_manual_control;
    }
    success = true;
    // For RPC get value method, return the value directly (boolean)
    resultMsg = current_mode ? "true" : "false";
    Serial.println("CoreIOT RPC: getPumpMode returned " + String(current_mode ? "MANUAL (true)" : "AUTO (false)"));
  }
  
  // Handle legacy LED control (if needed)
  else if (strcmp(method, "setStateLED") == 0) {
    const char* params = doc["params"].as<const char*>();
    // TODO: Implement LED control if needed
    success = true;
    resultMsg = "LED command received";
  }
  else {
    Serial.print("Unknown method: ");
    Serial.println(method);
    resultMsg = "Unknown method: " + String(method);
  }



  // Send response back to CoreIOT
  char response[256];
  if (success) {
    snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"%s\"}", resultMsg.c_str());
  } else {
    snprintf(response, sizeof(response), "{\"success\":false,\"error\":\"%s\"}", resultMsg.c_str());
  }
  
  String responseTopic = "v1/devices/me/rpc/response/" + requestId;
  if (client.publish(responseTopic.c_str(), response)) {
    Serial.print("Sent response to: ");
    Serial.println(responseTopic);
  } else {
    Serial.println("Failed to send RPC response");
  }
}


void setup_coreiot(){
  // Load credentials from storage
  loadCoreIOTCredentials();

  while(1){
    if (xSemaphoreTake(xBinarySemaphoreInternet, portMAX_DELAY)) {
      break;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println(" Connected!");

  client.setServer(coreiot_server.c_str(), mqttPort);
  client.setCallback(callback);

}

void coreiot_task(void *pvParameters){

    setup_coreiot();
    
    // Variables to track pump_state changes in AUTO mode
    static bool last_pump_state = false;
    static bool last_pump_mode = false;
    static bool initialized = false;

    while(1){
        // Skip CoreIOT operations in AP mode
        if (isAPMode) {
            // In AP mode, disconnect from CoreIOT and invalidate data
            if (client.connected()) {
                Serial.println("[CoreIOT] AP Mode detected - disconnecting from CoreIOT");
                client.disconnect();
                coreiot_data.is_valid = false;
                use_coreiot_data = false;
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS);  // Check every 5 seconds
            continue;
        }
        
        // Check if reconnect is needed (credentials changed)
        if (coreiot_reconnect_needed) {
            Serial.println("CoreIOT credentials changed - reconnecting...");
            if (client.connected()) {
                client.disconnect();
            }
            loadCoreIOTCredentials();  // Reload credentials
            client.setServer(coreiot_server.c_str(), mqttPort);  // Update server
            coreiot_reconnect_needed = false;
        }

        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        // Get sensor data atomically to ensure consistency
        SensorData_t sensor_data;
        getSensorData(&sensor_data);
        float temp = sensor_data.temperature;
        float hum = sensor_data.humidity;
        float soil = sensor_data.soil;

        // Get pump state atomically (thread-safe)
        bool current_pump_state;
        bool current_pump_mode;
        if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
            current_pump_state = pump_state;
            current_pump_mode = pump_manual_control;
            xSemaphoreGive(xMutexPumpControl);
        } else {
            // Fallback if mutex fails
            current_pump_state = pump_state;
            current_pump_mode = pump_manual_control;
        }
        
        // MONITOR: Phát hiện pump_state thay đổi trong AUTO mode và publish attributes ngay lập tức
        if (initialized) {
            // Chỉ publish khi: đang ở AUTO mode và pump_state thay đổi (không phải khi chuyển mode)
            if (!current_pump_mode && !last_pump_mode && (current_pump_state != last_pump_state)) {
                // Đang ở AUTO mode và pump_state đã thay đổi (maybom.cpp đã tự động thay đổi dựa trên soil)
                if (client.connected()) {
                    char attr[128];
                    snprintf(attr, sizeof(attr),
                        "{\"pump_state\": %s, \"pump_mode\": %s}",
                        current_pump_state ? "true" : "false",
                        "false"  // AUTO mode = false (boolean)
                    );
                    if (client.publish("v1/devices/me/attributes", attr)) {
                        Serial.println("[AUTO SYNC] Pump state changed in AUTO mode, published attributes to CoreIOT:");
                        Serial.println("  Soil: " + String(soil) + " -> Pump: " + String(current_pump_state ? "ON (true)" : "OFF (false)"));
                        Serial.println(attr);
                        
                        // Cập nhật coreiot_data với state mới để mainserver hiển thị đúng
                        coreiot_data.pump_state = current_pump_state;
                        coreiot_data.pump_mode = "AUTO";
                        coreiot_data.last_update = millis();
                        coreiot_data.is_valid = true;
                        use_coreiot_data = true;
                    } else {
                        Serial.println("[AUTO SYNC] Failed to publish pump state attributes to CoreIOT");
                    }
                }
            }
        } else {
            // Initialize tracking variables lần đầu
            initialized = true;
        }
        
        // Update last known values for next iteration
        last_pump_state = current_pump_state;
        last_pump_mode = current_pump_mode;

        // Get anomaly score and message from global variables
        float current_anomaly_score = glob_anomaly_score;
        String current_anomaly_message = glob_anomaly_message;

        // Format soil moisture to 2 digits (00, 01, 02, ... 09, 10, 11, ...)
        int soil_int = int(soil);
        String soil_str = (soil_int < 10 ? "0" : "") + String(soil_int);

        // Create JSON payload with all data including anomaly_score and anomaly_message
        // Using snprintf for better memory efficiency
        char payload[384];  // Increased buffer size to accommodate anomaly fields
        // Escape quotes in message string for JSON
        String escaped_message = current_anomaly_message;
        escaped_message.replace("\"", "\\\"");
        escaped_message.replace("\\", "\\\\");
        
        snprintf(payload, sizeof(payload),
            "{\"temperature\":%.2f,\"humidity\":%.2f,\"soil_moisture\":\"%s\",\"soil_moisture_value\":%.1f,\"pump_state\":%s,\"pump_mode\":\"%s\",\"anomaly_score\":%.4f,\"anomaly_message\":\"%s\"}",
            temp,
            hum,
            soil_str.c_str(),
            soil,
            current_pump_state ? "true" : "false",
            current_pump_mode ? "MANUAL" : "AUTO",
            current_anomaly_score,
            escaped_message.c_str()
        );
        
        if (client.publish("v1/devices/me/telemetry", payload)) {
            Serial.print("Published payload: ");
            Serial.println(payload);
            
            // THEO CHU TRÌNH MỚI: Sau khi publish lên CoreIOT, tự cập nhật coreiot_data
            // để Mainserver và LCD có thể lấy dữ liệu từ CoreIOT để hiển thị
            coreiot_data.temperature = temp;
            coreiot_data.humidity = hum;
            coreiot_data.soil = soil;
            coreiot_data.pump_state = current_pump_state;
            coreiot_data.pump_mode = current_pump_mode ? "MANUAL" : "AUTO";
            coreiot_data.anomaly_score = current_anomaly_score;
            coreiot_data.anomaly_message = current_anomaly_message;
            coreiot_data.last_update = millis();
            coreiot_data.is_valid = true;
            use_coreiot_data = true;
            
            Serial.println("[CoreIOT] Updated local data structure with published values");
            
            // Request attributes từ CoreIOT để đồng bộ (nếu CoreIOT có lưu telemetry vào attributes)
            // Điều này đảm bảo dữ liệu được sync từ CoreIOT về
            static unsigned long lastAttributeRequest = 0;
            if (millis() - lastAttributeRequest > 10000) {  // Request mỗi 10 giây
                client.publish("v1/devices/me/attributes/request/1", 
                              "{\"sharedKeys\":\"temperature,humidity,soil_moisture_value,soil_moisture,pump_state,pump_mode,anomaly_score,anomaly_message\"}");
                lastAttributeRequest = millis();
            }
        } else {
            Serial.println("Failed to publish telemetry");
        }
        
        vTaskDelay(3000 / portTICK_PERIOD_MS);  
    }
}