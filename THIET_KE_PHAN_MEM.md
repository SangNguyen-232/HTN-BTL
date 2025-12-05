# THIẾT KẾ PHẦN MỀM - HỆ THỐNG GIÁM SÁT VÀ ĐIỀU KHIỂN IoT

## 1. TỔNG QUAN HỆ THỐNG

Hệ thống IoT giám sát và điều khiển tự động dựa trên ESP32, bao gồm:
- **Cảm biến**: Nhiệt độ, độ ẩm không khí (DHT20), độ ẩm đất (analog)
- **Thiết bị điều khiển**: Máy bơm nước, LED1, LED2 (NeoPixel)
- **Hiển thị**: LCD I2C 16x2
- **Xử lý**: TinyML (TensorFlow Lite) phát hiện bất thường
- **Giao tiếp**: WiFi, HTTP Web Server, MQTT (CoreIOT), UART

---

## 2. LUỒNG CHƯƠNG TRÌNH (FLOWCHART)

### 2.1. Luồng Khởi Động Hệ Thống

```
[START]
    |
    v
[Khởi tạo Serial (115200 baud)]
    |
    v
[Tạo các FreeRTOS Tasks]
    ├─> Task LED Blink
    ├─> Task NEO Blink  
    ├─> Task Temp/Humi Monitor
    ├─> Task Soil Sensor
    ├─> Task May Bom (Pump Control)
    ├─> Task Main Server (Web Server)
    ├─> Task TinyML (Anomaly Detection)
    └─> Task CoreIOT (MQTT Client)
    |
    v
[Chờ tất cả tasks khởi động]
    |
    v
[Vòng lặp chính - loop() rỗng]
    |
    v
[END - Hệ thống chạy đa nhiệm]
```

### 2.2. Luồng Đọc Cảm Biến và Xử Lý

```
[Task: Temp/Humi Monitor]
    |
    v
[Khởi tạo I2C (SDA=11, SCL=12)]
    |
    v
[Khởi tạo DHT20 sensor]
    |
    v
[Khởi tạo LCD I2C]
    |
    v
[Vòng lặp vô hạn]
    |
    ├─> [Đọc DHT20]
    |       |
    |       v
    |   [Kiểm tra dữ liệu hợp lệ?]
    |       |
    |       ├─> [Có] -> Lưu vào glob_temperature, glob_humidity (mutex)
    |       |
    |       └─> [Không] -> Gán -1
    |
    ├─> [Lấy dữ liệu từ CoreIOT (nếu có)]
    |       |
    |       v
    |   [coreiot_data.is_valid?]
    |       |
    |       ├─> [Có] -> Hiển thị từ CoreIOT
    |       |
    |       └─> [Không] -> Hiển thị "--"
    |
    ├─> [Cập nhật LCD]
    |       |
    |       v
    |   [Hiển thị: Temp, Hum, Soil]
    |
    └─> [Delay 3 giây]
            |
            v
        [Quay lại vòng lặp]
```

```
[Task: Soil Sensor]
    |
    v
[Khởi tạo I2C]
    |
    v
[Vòng lặp vô hạn]
    |
    ├─> [Đọc analog pin 1]
    |       |
    |       v
    |   [Map giá trị: 0-4095 -> 0-100%]
    |
    ├─> [Lưu vào glob_soil (mutex)]
    |
    └─> [Delay 3 giây]
            |
            v
        [Quay lại vòng lặp]
```

### 2.3. Luồng Điều Khiển Máy Bơm

```
[Task: May Bom]
    |
    v
[Khởi tạo GPIO 6 (OUTPUT)]
    |
    v
[Vòng lặp vô hạn]
    |
    ├─> [Đọc glob_soil (mutex)]
    |
    ├─> [Đọc pump_manual_control, pump_state (mutex)]
    |
    ├─> [Kiểm tra chế độ]
    |       |
    |       ├─> [MANUAL Mode]
    |       |       |
    |       |       v
    |       |   [Điều khiển theo pump_state]
    |       |       |
    |       |       v
    |       |   [digitalWrite(MAYBOM_PIN, pump_state)]
    |       |
    |       └─> [AUTO Mode]
    |               |
    |               v
    |           [Tính toán trạng thái tự động]
    |               |
    |               ├─> [Pump đang BẬT]
    |               |       |
    |               |       v
    |               |   [soil < pump_threshold_max?]
    |               |       |
    |               |       ├─> [Có] -> Giữ BẬT
    |               |       |
    |               |       └─> [Không] -> TẮT
    |               |
    |               └─> [Pump đang TẮT]
    |                       |
    |                       v
    |                   [soil < pump_threshold_min?]
    |                       |
    |                       ├─> [Có] -> BẬT
    |                       |
    |                       └─> [Không] -> Giữ TẮT
    |
    ├─> [Cập nhật pump_state (mutex)]
    |
    └─> [Delay 100ms]
            |
            v
        [Quay lại vòng lặp]
```

### 2.4. Luồng Phát Hiện Bất Thường (TinyML)

```
[Task: TinyML]
    |
    v
[Khởi tạo TensorFlow Lite]
    |
    ├─> [Load model từ dht_anomaly_model.h]
    |
    ├─> [Allocate tensor arena (8KB)]
    |
    └─> [Setup interpreter]
    |
    v
[Vòng lặp vô hạn]
    |
    ├─> [Lấy dữ liệu cảm biến (thread-safe)]
    |       |
    |       v
    |   [input[0] = glob_temperature]
    |   [input[1] = glob_humidity]
    |
    ├─> [Chạy inference]
    |       |
    |       v
    |   [interpreter->Invoke()]
    |
    ├─> [Lấy kết quả]
    |       |
    |       v
    |   [result = output->data.f[0]]
    |
    ├─> [Phân loại]
    |       |
    |       ├─> [0.49 <= result <= 0.52]
    |       |       |
    |       |       v
    |       |   [glob_anomaly_message = "Normal"]
    |       |
    |       └─> [Khác]
    |               |
    |               v
    |           [glob_anomaly_message = "Warning!"]
    |
    ├─> [Lưu glob_anomaly_score = result]
    |
    └─> [Delay 3 giây]
            |
            v
        [Quay lại vòng lặp]
```

### 2.5. Luồng Web Server

```
[Task: Main Server]
    |
    v
[Kiểm tra chế độ WiFi]
    |
    ├─> [AP Mode]
    |       |
    |       v
    |   [Tạo Access Point]
    |       |
    |       v
    |   [IP: 192.168.4.1]
    |
    └─> [STA Mode]
            |
            v
        [Kết nối WiFi]
            |
            v
        [Lấy IP từ DHCP]
    |
    v
[Khởi tạo Web Server (port 80)]
    |
    ├─> [Route: /]
    |       |
    |       v
    |   [Trả về mainPage() HTML]
    |
    ├─> [Route: /settings]
    |       |
    |       v
    |   [Trả về settingsPage() HTML]
    |
    ├─> [Route: /api/data]
    |       |
    |       v
    |   [Trả về JSON dữ liệu cảm biến]
    |
    ├─> [Route: /api/pump]
    |       |
    |       v
    |   [POST: Điều khiển máy bơm]
    |
    └─> [Route: /api/wifi]
            |
            v
        [POST: Cấu hình WiFi]
    |
    v
[Vòng lặp vô hạn]
    |
    ├─> [server.handleClient()]
    |
    └─> [Delay 10ms]
            |
            v
        [Quay lại vòng lặp]
```

### 2.6. Luồng CoreIOT (MQTT)

```
[Task: CoreIOT]
    |
    v
[Chờ WiFi kết nối (semaphore)]
    |
    v
[Load credentials từ EEPROM]
    |
    ├─> [coreiot_server]
    ├─> [coreiot_token hoặc username/password]
    └─> [coreiot_client_id]
    |
    v
[Khởi tạo MQTT Client]
    |
    ├─> [client.setServer(server, 1883)]
    └─> [client.setCallback(callback)]
    |
    v
[Vòng lặp vô hạn]
    |
    ├─> [Kiểm tra kết nối]
    |       |
    |       ├─> [Chưa kết nối]
    |       |       |
    |       |       v
    |       |   [reconnect()]
    |       |       |
    |       |       ├─> [Xác thực (token hoặc username/password)]
    |       |       |
    |       |       ├─> [Subscribe topics]
    |       |       |       |
    |       |       |       ├─> v1/devices/me/rpc/request/+
    |       |       |       └─> v1/devices/me/attributes
    |       |       |
    |       |       └─> [Gửi shared attributes]
    |
    ├─> [client.loop() - Xử lý messages]
    |
    ├─> [Thu thập dữ liệu cảm biến (thread-safe)]
    |
    ├─> [Tạo JSON payload]
    |       |
    |       v
    |   {
    |     "temperature": float,
    |     "humidity": float,
    |     "soil_moisture": "00-99",
    |     "soil_moisture_value": float,
    |     "pump_state": bool,
    |     "pump_mode": "AUTO"/"MANUAL",
    |     "anomaly_score": float,
    |     "anomaly_message": string
    |   }
    |
    ├─> [Publish telemetry]
    |       |
    |       v
    |   [client.publish("v1/devices/me/telemetry", payload)]
    |
    ├─> [Cập nhật coreiot_data]
    |       |
    |       v
    |   [Đánh dấu is_valid = true]
    |
    ├─> [Kiểm tra thay đổi pump_state (AUTO mode)]
    |       |
    |       v
    |   [Nếu thay đổi -> Publish attributes]
    |
    └─> [Delay 3 giây]
            |
            v
        [Quay lại vòng lặp]
```

### 2.7. State Diagram - Trạng Thái Máy Bơm

```
                    [MANUAL Mode]
                         |
                         v
              ┌──────────────────────┐
              │  pump_manual_control │
              │       = true         │
              └──────────────────────┘
                         |
                         v
        ┌────────────────┴────────────────┐
        |                                  |
        v                                  v
  [pump_state = ON]              [pump_state = OFF]
        |                                  |
        └────────────────┬─────────────────┘
                         |
                         v
              [Điều khiển trực tiếp]
                         |
                         v
              [digitalWrite(ON/OFF)]


                    [AUTO Mode]
                         |
                         v
              ┌──────────────────────┐
              │  pump_manual_control │
              │       = false        │
              └──────────────────────┘
                         |
                         v
        ┌────────────────┴────────────────┐
        |                                  |
        v                                  v
  [pump_state = ON]              [pump_state = OFF]
        |                                  |
        |                                  |
        v                                  v
  [soil < max?]                    [soil < min?]
        |                                  |
        ├─> [Có] -> Giữ ON                 ├─> [Có] -> Chuyển ON
        |                                  |
        └─> [Không] -> Chuyển OFF          └─> [Không] -> Giữ OFF
```

---

## 3. MÔ TẢ CHỨC NĂNG TỪNG MODULE

### 3.1. Module Đọc Cảm Biến

#### 3.1.1. Module Temp/Humi Monitor (`temp_humi_monitor.cpp`)

**Chức năng:**
- Đọc dữ liệu nhiệt độ và độ ẩm từ cảm biến DHT20 qua giao tiếp I2C
- Hiển thị dữ liệu lên màn hình LCD I2C
- Lưu trữ dữ liệu vào biến global với cơ chế thread-safe (mutex)

**Thông số kỹ thuật:**
- **Cảm biến**: DHT20 (I2C)
- **Giao tiếp**: I2C (SDA=GPIO 11, SCL=GPIO 12)
- **Tần suất đọc**: 3 giây/lần
- **Dữ liệu đầu ra**: `glob_temperature`, `glob_humidity`

**Luồng xử lý:**
1. Khởi tạo I2C và DHT20
2. Khởi tạo LCD
3. Vòng lặp:
   - Đọc DHT20
   - Kiểm tra tính hợp lệ
   - Lưu vào global variables (mutex)
   - Lấy dữ liệu từ CoreIOT (nếu có)
   - Hiển thị lên LCD
   - Delay 3 giây

**Xử lý lỗi:**
- Nếu đọc lỗi: Gán giá trị -1
- Hiển thị "--.--" trên LCD khi không có dữ liệu

#### 3.1.2. Module Soil Sensor (`soil_sensor.cpp`)

**Chức năng:**
- Đọc giá trị analog từ cảm biến độ ẩm đất
- Chuyển đổi giá trị raw (0-4095) sang phần trăm (0-100%)
- Lưu trữ dữ liệu vào biến global với cơ chế thread-safe

**Thông số kỹ thuật:**
- **Cảm biến**: Analog soil moisture sensor
- **Pin**: GPIO 1 (ADC)
- **Độ phân giải**: 12-bit (0-4095)
- **Tần suất đọc**: 3 giây/lần
- **Dữ liệu đầu ra**: `glob_soil` (0-100%)

**Luồng xử lý:**
1. Khởi tạo I2C (nếu cần)
2. Vòng lặp:
   - Đọc analog pin 1
   - Map giá trị: `map(rawValue, 0, 4095, 0, 100)`
   - Lưu vào `glob_soil` (mutex)
   - Delay 3 giây

### 3.2. Module Xử Lý Dữ Liệu

#### 3.2.1. Module TinyML (`tinyml.cpp`)

**Chức năng:**
- Phát hiện bất thường trong dữ liệu nhiệt độ và độ ẩm
- Sử dụng mô hình TensorFlow Lite được train sẵn
- Phân loại trạng thái: "Normal" hoặc "Warning!"

**Thông số kỹ thuật:**
- **Framework**: TensorFlow Lite Micro
- **Model**: `dht_anomaly_model.tflite`
- **Input**: 2 features (temperature, humidity)
- **Output**: 1 giá trị (anomaly score)
- **Memory**: 8KB tensor arena
- **Tần suất inference**: 3 giây/lần

**Luồng xử lý:**
1. Khởi tạo TensorFlow Lite:
   - Load model từ header file
   - Allocate tensor arena
   - Setup interpreter
2. Vòng lặp:
   - Lấy dữ liệu cảm biến (thread-safe)
   - Gán vào input tensor
   - Chạy inference
   - Lấy kết quả từ output tensor
   - Phân loại:
     - `0.49 <= score <= 0.52` → "Normal"
     - Khác → "Warning!"
   - Lưu vào `glob_anomaly_score`, `glob_anomaly_message`
   - Delay 3 giây

**Ngưỡng phân loại:**
- **Normal**: 0.49 - 0.52
- **Warning**: < 0.49 hoặc > 0.52

### 3.3. Module Điều Khiển

#### 3.3.1. Module Máy Bơm (`maybom.cpp`)

**Chức năng:**
- Điều khiển máy bơm nước tự động hoặc thủ công
- Hỗ trợ 2 chế độ: AUTO và MANUAL
- Trong chế độ AUTO, tự động bật/tắt dựa trên độ ẩm đất

**Thông số kỹ thuật:**
- **Pin điều khiển**: GPIO 6
- **Chế độ**: OUTPUT
- **Tần suất kiểm tra**: 100ms/lần
- **Ngưỡng AUTO**: `pump_threshold_min`, `pump_threshold_max`

**Chế độ MANUAL:**
- Điều khiển trực tiếp theo `pump_state`
- `pump_state = true` → BẬT
- `pump_state = false` → TẮT

**Chế độ AUTO:**
- Logic điều khiển:
  - **Khi pump đang TẮT**: Bật khi `soil < pump_threshold_min`
  - **Khi pump đang BẬT**: Tắt khi `soil >= pump_threshold_max`
- Tránh hiện tượng bật/tắt liên tục (hysteresis)

**Luồng xử lý:**
1. Khởi tạo GPIO 6 (OUTPUT, LOW)
2. Vòng lặp:
   - Đọc `glob_soil` (mutex)
   - Đọc `pump_manual_control`, `pump_state` (mutex)
   - Kiểm tra chế độ:
     - **MANUAL**: Điều khiển trực tiếp
     - **AUTO**: Tính toán trạng thái dựa trên ngưỡng
   - Cập nhật GPIO
   - Cập nhật `pump_state` (mutex)
   - Delay 100ms

#### 3.3.2. Module LED (`led_blinky.cpp`, `neo_blinky.cpp`)

**Chức năng:**
- Điều khiển LED1 (GPIO 48) và LED2/NeoPixel (GPIO 45)
- Hiển thị trạng thái hệ thống
- Có thể điều khiển qua web interface

**Thông số kỹ thuật:**
- **LED1**: GPIO 48 (digital)
- **LED2**: GPIO 45 (NeoPixel RGB)
- **Tần suất**: Tùy chỉnh

### 3.4. Module Hiển Thị

#### 3.4.1. Module LCD (`lcd.cpp`)

**Chức năng:**
- Hiển thị dữ liệu cảm biến lên màn hình LCD I2C 16x2
- Format: "Tem:XX.XXC|Soil:XX%" và "Hum:XX.XX%|XX%"

**Thông số kỹ thuật:**
- **Loại**: LCD I2C 16x2
- **Giao tiếp**: I2C
- **Độ phân giải**: 16x2 ký tự

**Luồng xử lý:**
1. Khởi tạo LCD qua I2C
2. Hiển thị format cố định
3. Cập nhật giá trị từ `coreiot_data` hoặc local sensors

### 3.5. Module Truyền Dữ Liệu

#### 3.5.1. Module Web Server (`mainserver.cpp`)

**Chức năng:**
- Cung cấp web interface để xem dữ liệu và điều khiển
- Hỗ trợ 2 chế độ: AP Mode và STA Mode
- RESTful API endpoints

**Thông số kỹ thuật:**
- **Port**: 80
- **Framework**: ESP32 WebServer
- **Giao thức**: HTTP/1.1

**Endpoints:**
- `GET /`: Trang chủ (dashboard)
- `GET /settings`: Trang cấu hình
- `GET /api/data`: API lấy dữ liệu JSON
- `POST /api/pump`: API điều khiển máy bơm
- `POST /api/wifi`: API cấu hình WiFi
- `POST /api/coreiot`: API cấu hình CoreIOT

**Luồng xử lý:**
1. Kiểm tra chế độ WiFi:
   - **AP Mode**: Tạo Access Point (192.168.4.1)
   - **STA Mode**: Kết nối WiFi
2. Khởi tạo Web Server
3. Đăng ký routes
4. Vòng lặp:
   - `server.handleClient()`
   - Delay 10ms

**Tính năng:**
- Dashboard với biểu đồ real-time (Chart.js)
- Điều khiển máy bơm (AUTO/MANUAL)
- Cấu hình WiFi
- Cấu hình CoreIOT credentials
- Cấu hình ngưỡng máy bơm

#### 3.5.2. Module CoreIOT (`coreiot.cpp`)

**Chức năng:**
- Kết nối và giao tiếp với CoreIOT platform qua MQTT
- Gửi telemetry (dữ liệu cảm biến)
- Nhận RPC commands (điều khiển từ xa)
- Đồng bộ attributes (trạng thái thiết bị)

**Thông số kỹ thuật:**
- **Giao thức**: MQTT (port 1883)
- **Library**: PubSubClient
- **Tần suất publish**: 3 giây/lần
- **Xác thực**: Token hoặc Username/Password

**Topics:**
- **Telemetry**: `v1/devices/me/telemetry`
- **Attributes**: `v1/devices/me/attributes`
- **RPC Request**: `v1/devices/me/rpc/request/+`
- **RPC Response**: `v1/devices/me/rpc/response/{requestId}`
- **Attributes Request**: `v1/devices/me/attributes/request/1`

**Luồng xử lý:**
1. Chờ WiFi kết nối (semaphore)
2. Load credentials từ EEPROM
3. Khởi tạo MQTT client
4. Vòng lặp:
   - Kiểm tra kết nối → Reconnect nếu cần
   - `client.loop()` → Xử lý messages
   - Thu thập dữ liệu cảm biến (thread-safe)
   - Tạo JSON payload
   - Publish telemetry
   - Cập nhật `coreiot_data`
   - Kiểm tra thay đổi pump_state (AUTO mode)
   - Delay 3 giây

**RPC Methods:**
- `setPumpState(params: boolean)`: Bật/tắt máy bơm
- `setPumpMode(params: "AUTO"/"MANUAL")`: Chuyển chế độ
- `getPumpMode()`: Lấy chế độ hiện tại

**Xử lý Attributes:**
- Nhận attributes từ server (shared/client)
- Cập nhật `coreiot_data`
- Đồng bộ trạng thái máy bơm

### 3.6. Module Quản Lý Dữ Liệu

#### 3.6.1. Module Global (`global.cpp`)

**Chức năng:**
- Quản lý biến global và cơ chế thread-safe
- Cung cấp hàm helper để đọc dữ liệu nguyên tử
- Quản lý credentials và cấu hình

**Cấu trúc dữ liệu:**
```cpp
typedef struct {
    float temperature;
    float humidity;
    float soil;
} SensorData_t;
```

**Biến global:**
- `glob_temperature`: Nhiệt độ
- `glob_humidity`: Độ ẩm không khí
- `glob_soil`: Độ ẩm đất
- `glob_anomaly_score`: Điểm bất thường
- `glob_anomaly_message`: Thông báo bất thường
- `pump_state`: Trạng thái máy bơm
- `pump_manual_control`: Chế độ điều khiển

**Mutex/Semaphore:**
- `xMutexSensorData`: Bảo vệ dữ liệu cảm biến
- `xMutexPumpControl`: Bảo vệ điều khiển máy bơm
- `xBinarySemaphoreInternet`: Báo hiệu WiFi kết nối

**Hàm helper:**
- `getSensorData(SensorData_t* data)`: Đọc dữ liệu nguyên tử
- `loadCoreIOTCredentials()`: Load credentials từ EEPROM
- `saveCoreIOTCredentials(...)`: Lưu credentials
- `loadPumpThresholds()`: Load ngưỡng máy bơm
- `savePumpThresholds(...)`: Lưu ngưỡng máy bơm

---

## 4. GIAO THỨC TRUYỀN DỮ LIỆU

### 4.1. UART (Serial)

**Mục đích:**
- Debug và monitoring
- Hiển thị log messages

**Thông số:**
- **Baud rate**: 115200
- **Data bits**: 8
- **Stop bits**: 1
- **Parity**: None
- **Flow control**: None

**Sử dụng:**
- In log messages
- Debug sensor readings
- Debug MQTT connection
- Debug web server

**Ví dụ:**
```cpp
Serial.begin(115200);
Serial.println("Temperature: " + String(temperature));
```

### 4.2. I2C

**Mục đích:**
- Giao tiếp với DHT20 (nhiệt độ, độ ẩm)
- Giao tiếp với LCD I2C

**Thông số:**
- **SDA**: GPIO 11
- **SCL**: GPIO 12
- **Tần số**: 100kHz (default)
- **Mode**: Master

**Thiết bị:**
- **DHT20**: Address 0x38
- **LCD I2C**: Address 0x27 (default)

**Ví dụ:**
```cpp
Wire.begin(11, 12);
dht20.begin();
```

### 4.3. WiFi

**Mục đích:**
- Kết nối Internet
- Tạo Access Point
- Giao tiếp với Web Server và MQTT

**Thông số:**
- **Standard**: IEEE 802.11 b/g/n
- **Frequency**: 2.4 GHz
- **Mode**: AP (Access Point) hoặc STA (Station)

**AP Mode:**
- **SSID**: "ESP32-AP" (mặc định)
- **IP**: 192.168.4.1
- **Gateway**: 192.168.4.1
- **Subnet**: 255.255.255.0

**STA Mode:**
- Kết nối với WiFi router
- Nhận IP từ DHCP
- Có thể cấu hình qua web interface

**Ví dụ:**
```cpp
WiFi.mode(WIFI_AP);
WiFi.softAP("ESP32-AP", "password");
```

### 4.4. HTTP

**Mục đích:**
- Web interface
- RESTful API

**Thông số:**
- **Protocol**: HTTP/1.1
- **Port**: 80
- **Method**: GET, POST
- **Content-Type**: text/html, application/json

**Endpoints:**

1. **GET /**
   - Trả về HTML dashboard
   - Content-Type: text/html

2. **GET /settings**
   - Trả về trang cấu hình
   - Content-Type: text/html

3. **GET /api/data**
   - Trả về dữ liệu JSON
   - Response:
   ```json
   {
     "temperature": 25.5,
     "humidity": 60.0,
     "soil": 45.0,
     "pump_state": true,
     "pump_mode": "AUTO",
     "anomaly_score": 0.51,
     "anomaly_message": "Normal"
   }
   ```

4. **POST /api/pump**
   - Điều khiển máy bơm
   - Body:
   ```json
   {
     "state": true,
     "mode": "MANUAL"
   }
   ```

5. **POST /api/wifi**
   - Cấu hình WiFi
   - Body:
   ```json
   {
     "ssid": "WiFi-Name",
     "password": "password"
   }
   ```

6. **POST /api/coreiot**
   - Cấu hình CoreIOT
   - Body:
   ```json
   {
     "server": "demo.thingsboard.io",
     "token": "your-token",
     "client_id": "ESP32Client"
   }
   ```

**Ví dụ:**
```cpp
server.on("/api/data", HTTP_GET, []() {
  String json = "{...}";
  server.send(200, "application/json", json);
});
```

### 4.5. MQTT

**Mục đích:**
- Giao tiếp với CoreIOT platform
- Gửi telemetry
- Nhận RPC commands
- Đồng bộ attributes

**Thông số:**
- **Protocol**: MQTT 3.1.1
- **Port**: 1883 (default)
- **Library**: PubSubClient
- **QoS**: 0 (default)
- **Keep Alive**: 60 giây

**Xác thực:**
- **Token**: Username = token, Password = ""
- **Username/Password**: Username và password riêng

**Topics:**

1. **Telemetry** (Publish)
   - Topic: `v1/devices/me/telemetry`
   - Payload:
   ```json
   {
     "temperature": 25.5,
     "humidity": 60.0,
     "soil_moisture": "45",
     "soil_moisture_value": 45.0,
     "pump_state": true,
     "pump_mode": "AUTO",
     "anomaly_score": 0.51,
     "anomaly_message": "Normal"
   }
   ```

2. **Attributes** (Publish/Subscribe)
   - Topic: `v1/devices/me/attributes`
   - Payload:
   ```json
   {
     "pump_state": true,
     "pump_mode": "AUTO",
     "temperature": 25.5,
     "humidity": 60.0,
     "soil_moisture_value": 45.0
   }
   ```

3. **RPC Request** (Subscribe)
   - Topic: `v1/devices/me/rpc/request/+`
   - Payload:
   ```json
   {
     "method": "setPumpState",
     "params": true
   }
   ```

4. **RPC Response** (Publish)
   - Topic: `v1/devices/me/rpc/response/{requestId}`
   - Payload:
   ```json
   {
     "success": true,
     "message": "Pump state set to ON"
   }
   ```

5. **Attributes Request** (Publish)
   - Topic: `v1/devices/me/attributes/request/1`
   - Payload:
   ```json
   {
     "sharedKeys": "temperature,humidity,soil_moisture_value,pump_state,pump_mode"
   }
   ```

**Ví dụ:**
```cpp
// Publish telemetry
client.publish("v1/devices/me/telemetry", payload);

// Subscribe RPC
client.subscribe("v1/devices/me/rpc/request/+");

// Callback
void callback(char* topic, byte* payload, unsigned int length) {
  // Xử lý message
}
```

### 4.6. So Sánh Các Giao Thức

| Giao thức | Tốc độ | Khoảng cách | Độ phức tạp | Ứng dụng |
|-----------|--------|-------------|-------------|----------|
| **UART** | Trung bình | Ngắn (< 1m) | Thấp | Debug, logging |
| **I2C** | Trung bình | Ngắn (< 1m) | Trung bình | Cảm biến, LCD |
| **WiFi** | Cao | Trung bình (10-100m) | Cao | Internet, web |
| **HTTP** | Cao | Toàn cầu (qua Internet) | Trung bình | Web interface, API |
| **MQTT** | Cao | Toàn cầu (qua Internet) | Trung bình | IoT platform, real-time |

---

## 5. KIẾN TRÚC TỔNG THỂ

### 5.1. Sơ Đồ Kiến Trúc

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Microcontroller                 │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │   Sensors    │  │  Actuators   │  │   Display    │ │
│  │              │  │              │  │              │ │
│  │ - DHT20      │  │ - Pump (GPIO6)│  │ - LCD I2C    │ │
│  │ - Soil (ADC) │  │ - LED1 (GPIO48)│ │              │ │
│  │              │  │ - LED2 (GPIO45)│ │              │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘ │
│         │                  │                  │          │
│         └──────────────────┼──────────────────┘          │
│                            │                            │
│  ┌──────────────────────────────────────────────────┐   │
│  │         FreeRTOS Multi-Tasking System            │   │
│  │                                                   │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ │   │
│  │  │ Temp/Humi   │  │ Soil Sensor │  │ TinyML   │ │   │
│  │  │   Task      │  │    Task     │  │   Task   │ │   │
│  │  └──────┬──────┘  └──────┬──────┘  └─────┬─────┘ │   │
│  │         │                │                │        │   │
│  │  ┌──────┴────────────────┴────────────────┴────┐ │   │
│  │  │         Global Data (Mutex Protected)        │ │   │
│  │  └──────┬───────────────────────────────────────┘ │   │
│  │         │                                          │   │
│  │  ┌──────┴──────┐  ┌─────────────┐  ┌──────────┐ │   │
│  │  │ Pump Control│  │ Web Server  │  │ CoreIOT  │ │   │
│  │  │    Task     │  │    Task     │  │   Task   │ │   │
│  │  └─────────────┘  └──────┬───────┘  └─────┬─────┘ │   │
│  └──────────────────────────┼────────────────┼───────┘   │
└──────────────────────────────┼────────────────┼───────────┘
                               │                │
                    ┌──────────┴────┐  ┌────────┴────────┐
                    │               │  │                 │
                    v               v  v                 v
              ┌─────────┐    ┌──────────┐        ┌─────────────┐
              │  WiFi   │    │  HTTP    │        │    MQTT     │
              │  AP/STA │    │  Server  │        │  (CoreIOT)  │
              └─────────┘    └──────────┘        └─────────────┘
                    │               │                 │
                    └───────────────┴─────────────────┘
                                    │
                                    v
                            ┌───────────────┐
                            │   Internet    │
                            │  (Cloud/Web)  │
                            └───────────────┘
```

### 5.2. Luồng Dữ Liệu

```
[Sensors]
    │
    ├─> [DHT20] ──> glob_temperature, glob_humidity
    │
    └─> [Soil Sensor] ──> glob_soil
            │
            v
    [Global Data (Mutex)]
            │
            ├─> [TinyML Task] ──> glob_anomaly_score, glob_anomaly_message
            │
            ├─> [Pump Control Task] ──> pump_state
            │
            ├─> [LCD Display Task] ──> Hiển thị
            │
            ├─> [Web Server Task] ──> HTTP API
            │
            └─> [CoreIOT Task] ──> MQTT Telemetry
                    │
                    v
            [CoreIOT Platform]
                    │
                    ├─> [Attributes] ──> Đồng bộ về ESP32
                    │
                    └─> [RPC Commands] ──> Điều khiển từ xa
```

---

## 6. TÓM TẮT

Hệ thống sử dụng kiến trúc đa nhiệm (FreeRTOS) với các module độc lập:

1. **Đọc cảm biến**: DHT20 (I2C), Soil sensor (ADC)
2. **Xử lý**: TinyML phát hiện bất thường
3. **Điều khiển**: Máy bơm tự động/thủ công
4. **Hiển thị**: LCD I2C
5. **Truyền dữ liệu**: 
   - UART (debug)
   - I2C (cảm biến, LCD)
   - WiFi (kết nối)
   - HTTP (web interface)
   - MQTT (CoreIOT platform)

Tất cả các module giao tiếp thông qua biến global được bảo vệ bởi mutex/semaphore để đảm bảo thread-safety.






