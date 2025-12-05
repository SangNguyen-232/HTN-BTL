# QUY TRÌNH HOẠT ĐỘNG: CHUYỂN TỪ AP MODE SANG STA MODE

## 📋 TỔNG QUAN

Tài liệu này mô tả chi tiết quy trình hoạt động khi ESP32 chuyển từ **AP Mode** (Access Point Mode) sang **STA Mode** (Station Mode), bao gồm tất cả các hoạt động bên trong từng Mode.

---

## 🔄 QUY TRÌNH CHUYỂN ĐỔI

### **BƯỚC 1: KHỞI TẠO - AP MODE (Mặc định)**

#### 1.1. Khởi động hệ thống (`main_server_task`)
```
📍 File: mainserver.cpp - main_server_task()
```

**Hoạt động:**
1. Load cấu hình:
   - `loadPumpThresholds()` - Load ngưỡng pump từ Preferences
   - `loadLCDRefreshRate()` - Load refresh rate LCD từ Preferences

2. Thử kết nối STA Mode trước (8 giây):
   - `isAPMode = false`
   - `connecting = true`
   - `connectToWiFi()` - Thử kết nối với WiFi đã lưu
   - Đợi 8 giây để kiểm tra kết nối

3. Nếu STA thất bại → Chuyển sang AP Mode:
   - Gọi `startAP()`
   - `isAPMode = true`
   - `isWifiConnected = false`
   - Reset CoreIOT data: `coreiot_data.is_valid = false`, `use_coreiot_data = false`

#### 1.2. Khởi động AP Mode (`startAP()`)
```
📍 File: mainserver.cpp - startAP()
```

**Hoạt động:**
1. **WiFi Configuration:**
   - `WiFi.mode(WIFI_AP)` - Chuyển sang chế độ Access Point
   - `WiFi.softAP(ssid, password)` - Tạo AP với SSID và password từ `global.cpp`
   - IP mặc định: `192.168.4.1`

2. **mDNS Setup:**
   - `MDNS.begin("esp32")` - Khởi tạo mDNS với tên "esp32"
   - Access qua: `http://esp32.local` hoặc `http://192.168.4.1`

3. **State Variables:**
   - `isAPMode = true`
   - `connecting = false`
   - `coreiot_data.is_valid = false`
   - `use_coreiot_data = false`

4. **Web Server:**
   - `setupServer()` - Khởi tạo web server với các routes
   - Server chạy trên port 80

#### 1.3. Hoạt động trong AP Mode

**A. Web Interface (`mainPage()`)**
```
📍 File: mainserver.cpp - mainPage()
```

**Data Source:**
- **Sensors:** Lấy trực tiếp từ local sensors (thread-safe)
  ```cpp
  SensorData_t sensor_data;
  getSensorData(&sensor_data);
  temperature = String(sensor_data.temperature);
  humidity = String(sensor_data.humidity);
  soil = String(sensor_data.soil);
  ```
- **Anomaly:** Từ global variables
  ```cpp
  score = String(glob_anomaly_score, 4);
  message = glob_anomaly_message;
  ```
- **LED/Pump State:** Từ local state variables
  ```cpp
  led1 = led1_state ? "ON" : "OFF";
  pumpState = pump_state ? "ON" : "OFF";
  ```

**API Endpoints:**
- `/api/sensor-data` - Trả về dữ liệu sensor trực tiếp từ local
- `/sensors` - Trả về dữ liệu với source = "Local"

**B. CoreIOT Task (`coreiot_task`)**
```
📍 File: coreiot.cpp - coreiot_task()
```

**Hoạt động:**
- **Kiểm tra Mode:** `if (isAPMode)`
- **Disconnect MQTT:** Nếu đang kết nối, disconnect ngay
- **Invalidate Data:**
  ```cpp
  coreiot_data.is_valid = false;
  use_coreiot_data = false;
  ```
- **Delay:** `vTaskDelay(5000)` - Check lại mỗi 5 giây
- **Không publish telemetry** lên CoreIOT

**C. Sensor Data Flow:**
```
Sensors (DHT20, Soil) 
  → global.cpp (glob_temperature, glob_humidity, glob_soil)
  → mainserver.cpp (getSensorData - thread-safe)
  → Web Interface (hiển thị trực tiếp)
  → LCD Display (hiển thị trực tiếp)
```

---

### **BƯỚC 2: YÊU CẦU KẾT NỐI WiFi (Từ Web Interface)**

#### 2.1. User nhập WiFi credentials
```
📍 File: mainserver.cpp - handleConnect()
```

**Input từ Web Form:**
- SSID: `server.arg("ssid")`
- Password: `server.arg("pass")`

**Validation:**
- Kiểm tra SSID không rỗng
- Log thông tin kết nối

**Response:**
- Gửi response ngay: `server.send(200, "text/plain", "OK")`
- Delay 200ms để đảm bảo response được gửi

#### 2.2. Bắt đầu quá trình kết nối
```cpp
isAPMode = false;
connecting = true;
connect_start_ms = millis();
connectToWiFi();
```

---

### **BƯỚC 3: CHUYỂN ĐỔI WiFi MODE (`connectToWiFi()`)**

```
📍 File: mainserver.cpp - connectToWiFi()
```

#### 3.1. Tắt AP Mode
```cpp
if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
  WiFi.softAPdisconnect(true);  // Tắt AP
  delay(200);
}
```

#### 3.2. Chuyển sang STA Mode
```cpp
WiFi.mode(WIFI_STA);  // Chuyển sang Station Mode
delay(200);
```

#### 3.3. Disconnect kết nối cũ
```cpp
WiFi.disconnect();  // Ngắt kết nối cũ nếu có
delay(100);
```

#### 3.4. Bắt đầu kết nối mới
```cpp
WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
```

**Lưu ý:** Quá trình này mất thời gian (có thể 5-20 giây tùy router)

---

### **BƯỚC 4: MONITORING KẾT NỐI (Trong Main Loop)**

```
📍 File: mainserver.cpp - main_server_task() - while(1) loop
```

#### 4.1. Kiểm tra trạng thái kết nối
```cpp
if (connecting) {
  unsigned long elapsed = millis() - connect_start_ms;
  
  // Log progress mỗi 3 giây
  if (elapsed - lastLog > 3000) {
    Serial.print("[WiFi] Connecting...");
    Serial.println(WiFi.status());
  }
```

#### 4.2. Kết nối thành công
```cpp
if (WiFi.status() == WL_CONNECTED) {
  // Setup mDNS
  MDNS.begin("esp32");
  
  // Update state
  isWifiConnected = true;
  xSemaphoreGive(xBinarySemaphoreInternet);  // ⚠️ QUAN TRỌNG: Signal cho CoreIOT task
  isAPMode = false;
  connecting = false;
}
```

**Semaphore `xBinarySemaphoreInternet`:**
- Được signal khi WiFi kết nối thành công
- CoreIOT task đang chờ semaphore này để bắt đầu kết nối MQTT

#### 4.3. Timeout (20 giây)
```cpp
else if (elapsed > 20000) {
  // Quay lại AP Mode
  startAP();
  setupServer();
  connecting = false;
  isWifiConnected = false;
  coreiot_data.is_valid = false;
  use_coreiot_data = false;
}
```

---

### **BƯỚC 5: STA MODE ĐÃ KẾT NỐI**

#### 5.1. CoreIOT Task được kích hoạt
```
📍 File: coreiot.cpp - coreiot_task()
```

**A. Setup CoreIOT (`setup_coreiot()`)**
```cpp
loadCoreIOTCredentials();  // Load từ Preferences

// Đợi semaphore từ mainserver
while(1) {
  if (xSemaphoreTake(xBinarySemaphoreInternet, portMAX_DELAY)) {
    break;  // WiFi đã kết nối!
  }
  delay(500);
}
```

**B. Cấu hình MQTT Client**
```cpp
client.setServer(coreiot_server.c_str(), mqttPort);  // Port 1883
client.setCallback(callback);  // Set callback cho MQTT messages
```

**C. Kết nối MQTT (`reconnect()`)**
```cpp
// Authentication:
if (coreiot_use_token) {
  // Token auth: username = token, password = NULL
  client.connect(clientId, coreiot_token, NULL);
} else {
  // Username/Password auth
  client.connect(clientId, coreiot_username, coreiot_password);
}

// Subscribe topics:
client.subscribe("v1/devices/me/rpc/request/+");
client.subscribe("v1/devices/me/attributes");

// Request attributes từ server
client.publish("v1/devices/me/attributes/request/1", "...");

// Send shared attributes (RPC methods definition)
sendSharedAttributes();
```

#### 5.2. Main Loop của CoreIOT Task

**A. Kiểm tra Mode:**
```cpp
if (isAPMode) {
  // Disconnect và invalidate data
  client.disconnect();
  coreiot_data.is_valid = false;
  use_coreiot_data = false;
  continue;
}
```

**B. Reconnect nếu cần:**
```cpp
if (!client.connected()) {
  reconnect();
}
client.loop();  // Xử lý MQTT messages
```

**C. Publish Telemetry (mỗi 3 giây):**
```cpp
// Get sensor data (thread-safe)
SensorData_t sensor_data;
getSensorData(&sensor_data);

// Create JSON payload
char payload[384];
snprintf(payload, sizeof(payload),
  "{\"temperature\":%.2f,\"humidity\":%.2f,\"soil_moisture\":\"%s\",...}",
  temp, hum, soil_str, ...
);

// Publish
client.publish("v1/devices/me/telemetry", payload);

// ⚠️ QUAN TRỌNG: Cập nhật coreiot_data sau khi publish
coreiot_data.temperature = temp;
coreiot_data.humidity = hum;
coreiot_data.soil = soil;
coreiot_data.is_valid = true;
use_coreiot_data = true;
```

**D. Monitor AUTO Mode Pump Changes:**
```cpp
// Phát hiện pump_state thay đổi trong AUTO mode
if (!current_pump_mode && !last_pump_mode && 
    (current_pump_state != last_pump_state)) {
  // Publish attributes ngay lập tức
  client.publish("v1/devices/me/attributes", attr);
  // Cập nhật coreiot_data
}
```

#### 5.3. Web Interface trong STA Mode

**A. Data Source (`mainPage()`):**
```cpp
if (coreiot_data.is_valid) {
  // Sử dụng dữ liệu từ CoreIOT
  temperature = String(coreiot_data.temperature);
  humidity = String(coreiot_data.humidity);
  soil = String(coreiot_data.soil);
  pumpState = coreiot_data.pump_state ? "ON" : "OFF";
  pumpMode = coreiot_data.pump_mode;
} else {
  // Fallback: dùng local data nếu chưa có CoreIOT data
  // (xảy ra khi vừa chuyển sang STA mode)
}
```

**B. API Endpoints:**

**`/sensors` (`handleSensors()`):**
```cpp
if (coreiot_data.is_valid) {
  source = "CoreIOT";
  // Trả về dữ liệu từ coreiot_data
} else {
  source = "Loading...";
  // Trả về dữ liệu local (tạm thời)
}
```

**`/api/sensor-data`:**
- Chỉ dùng trong AP Mode
- Trả về dữ liệu local trực tiếp

**C. Refresh Rate:**
- AP Mode: 3 giây
- STA Mode: 5 giây

#### 5.4. MQTT Callbacks

**A. Attributes Update (`handleAttributesUpdate()`):**
```cpp
// Nhận attributes từ CoreIOT server
// Parse JSON và cập nhật coreiot_data
if (doc.containsKey("shared")) {
  parseAttributesObject(doc["shared"], &coreiot_data);
}
// Đồng bộ pump state/mode với local
```

**B. RPC Requests (`handleRPCRequest()`):**
```cpp
// Xử lý RPC methods từ CoreIOT:
// - setPumpState: Bật/tắt pump
// - setPumpMode: Chuyển AUTO/MANUAL
// - getPumpMode: Lấy trạng thái hiện tại

// Cập nhật local state (thread-safe với mutex)
// Publish response về CoreIOT
// Đồng bộ coreiot_data
```

#### 5.5. HTTP Fetch CoreIOT Data (`fetchCoreIOTData()`)
```
📍 File: mainserver.cpp - fetchCoreIOTData()
```

**Hoạt động (mỗi 10 giây):**
```cpp
// Thử các endpoints:
// 1. /api/v1/{token}/attributes
// 2. /api/v1/{token}/telemetry/latest
// 3. /api/v1/{token}/rpc

// Chỉ lấy pump controls, không có sensor data
// (CoreIOT không lưu sensor history)
```

---

## 📊 SO SÁNH AP MODE vs STA MODE

| Đặc điểm | AP Mode | STA Mode |
|----------|---------|----------|
| **WiFi Mode** | `WIFI_AP` | `WIFI_STA` |
| **IP Address** | `192.168.4.1` (fixed) | DHCP từ router |
| **mDNS** | `esp32.local` | `esp32.local` |
| **Data Source** | Local sensors trực tiếp | CoreIOT (MQTT/HTTP) |
| **CoreIOT** | ❌ Disconnected | ✅ Connected (MQTT) |
| **Web Refresh** | 3 giây | 5 giây |
| **API Endpoint** | `/api/sensor-data` | `/sensors` |
| **Telemetry** | ❌ Không gửi | ✅ Gửi mỗi 3 giây |
| **RPC Control** | ❌ Không có | ✅ Có (từ CoreIOT) |
| **Attributes** | ❌ Không sync | ✅ Sync 2 chiều |

---

## 🔄 DATA FLOW TRONG TỪNG MODE

### **AP MODE:**
```
┌─────────────┐
│   Sensors   │ (DHT20, Soil Sensor)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  global.cpp │ (glob_temperature, glob_humidity, glob_soil)
└──────┬──────┘
       │
       ├─────────────────┐
       ▼                 ▼
┌─────────────┐   ┌─────────────┐
│ Web Server  │   │  LCD Task   │
│ (mainPage)  │   │  (Display)  │
└─────────────┘   └─────────────┘
```

### **STA MODE:**
```
┌─────────────┐
│   Sensors   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  global.cpp │
└──────┬──────┘
       │
       ├─────────────────┐
       ▼                 ▼
┌─────────────┐   ┌─────────────┐
│CoreIOT Task │   │  LCD Task   │
│  (MQTT)     │   │  (Display)  │
└──────┬──────┘   └──────┬──────┘
       │                 │
       │ Publish         │ Read
       ▼                 ▼
┌─────────────┐   ┌─────────────┐
│  CoreIOT    │   │ coreiot_data│
│  Platform   │   │  (struct)   │
└──────┬──────┘   └──────┬──────┘
       │                 │
       │ Attributes/RPC  │
       │      ▼          │
       └─────────────────┘
                 │
                 ▼
         ┌─────────────┐
         │ Web Server  │
         │ (mainPage)  │
         └─────────────┘
```

---

## ⚠️ CÁC ĐIỂM QUAN TRỌNG

### 1. **Semaphore `xBinarySemaphoreInternet`**
- Được signal khi WiFi kết nối thành công
- CoreIOT task chờ semaphore này trước khi kết nối MQTT
- Đảm bảo CoreIOT chỉ chạy khi có internet

### 2. **Thread Safety**
- Tất cả access đến sensor data dùng `xMutexSensorData`
- Tất cả access đến pump control dùng `xMutexPumpControl`
- Đảm bảo không có race condition giữa các tasks

### 3. **Data Synchronization**
- `coreiot_data` struct được cập nhật sau mỗi lần publish telemetry
- Web interface và LCD đọc từ `coreiot_data` khi `is_valid = true`
- Pump state/mode được sync 2 chiều: Local ↔ CoreIOT

### 4. **Fallback Mechanism**
- Nếu CoreIOT không có data → dùng local data
- Nếu MQTT disconnect → vẫn có thể dùng HTTP fetch
- Nếu STA mode fail → tự động quay về AP mode

### 5. **State Variables**
```cpp
bool isAPMode;              // Current WiFi mode
bool connecting;            // Đang trong quá trình kết nối
bool isWifiConnected;      // WiFi đã kết nối thành công
bool use_coreiot_data;      // Có sử dụng CoreIOT data không
bool coreiot_data.is_valid; // CoreIOT data có hợp lệ không
```

---

## 🎯 TÓM TẮT QUY TRÌNH

1. **Khởi động:** Thử STA → Fail → AP Mode
2. **User nhập WiFi:** Web form → `handleConnect()`
3. **Chuyển Mode:** Tắt AP → STA → `WiFi.begin()`
4. **Monitoring:** Loop kiểm tra `WiFi.status()`
5. **Kết nối thành công:** Signal semaphore → CoreIOT kết nối MQTT
6. **STA Mode hoạt động:**
   - Publish telemetry mỗi 3 giây
   - Subscribe RPC/Attributes
   - Web interface hiển thị từ CoreIOT
   - Sync 2 chiều pump controls

---

## 📝 GHI CHÚ

- Timeout kết nối WiFi: **20 giây**
- CoreIOT telemetry interval: **3 giây**
- CoreIOT HTTP fetch interval: **10 giây**
- CoreIOT task check AP mode: **5 giây**
- Web refresh rate AP: **3 giây**
- Web refresh rate STA: **5 giây**
