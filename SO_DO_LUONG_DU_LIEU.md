# SƠ ĐỒ LUỒNG DỮ LIỆU - HỆ THỐNG IoT

## 1. LUỒNG DỮ LIỆU TỔNG QUAN

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32 SYSTEM                            │
│                                                                 │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐    │
│  │  SENSORS    │      │  PROCESSING  │      │  ACTUATORS  │    │
│  │             │      │             │      │             │    │
│  │ DHT20       │─────>│  TinyML     │      │  Pump       │    │
│  │ (I2C)       │      │  (Anomaly)  │      │  (GPIO 6)   │    │
│  │             │      │             │      │             │    │
│  │ Soil Sensor │      │             │      │  LED1       │    │
│  │ (ADC)       │      │             │      │  (GPIO 48)  │    │
│  └─────────────┘      └─────────────┘      │             │    │
│         │                    │             │  LED2       │    │
│         │                    │             │  (GPIO 45)  │    │
│         └────────┬───────────┘             └─────────────┘    │
│                  │                                           │
│         ┌────────▼──────────┐                                │
│         │  GLOBAL DATA      │                                │
│         │  (Mutex Protected)│                                │
│         └────────┬──────────┘                                │
│                  │                                           │
│  ┌───────────────┴───────────────┐                           │
│  │                               │                           │
│  ▼                               ▼                           │
│  ┌─────────────┐      ┌──────────────────┐                  │
│  │   DISPLAY   │      │  COMMUNICATION   │                  │
│  │             │      │                  │                  │
│  │ LCD I2C     │      │  Web Server      │                  │
│  │             │      │  (HTTP Port 80) │                  │
│  └─────────────┘      │                  │                  │
│                       │  CoreIOT         │                  │
│                       │  (MQTT Port 1883)│                  │
│                       └──────────────────┘                  │
└─────────────────────────────────────────────────────────────┘
```

## 2. LUỒNG ĐỌC CẢM BIẾN

```
[Task: Temp/Humi Monitor]
    │
    ├─> [Đọc DHT20 qua I2C]
    │       │
    │       ├─> Temperature
    │       └─> Humidity
    │
    ├─> [Kiểm tra hợp lệ]
    │       │
    │       ├─> [Hợp lệ] ──> Lưu vào glob_temperature, glob_humidity (mutex)
    │       └─> [Không hợp lệ] ──> Gán -1
    │
    ├─> [Lấy dữ liệu từ CoreIOT]
    │       │
    │       ├─> [Có dữ liệu] ──> Hiển thị từ CoreIOT
    │       └─> [Không có] ──> Hiển thị "--"
    │
    └─> [Cập nhật LCD]
            │
            └─> [Delay 3s] ──> Lặp lại


[Task: Soil Sensor]
    │
    ├─> [Đọc ADC Pin 1]
    │       │
    │       └─> Raw value (0-4095)
    │
    ├─> [Map: 0-4095 → 0-100%]
    │
    ├─> [Lưu vào glob_soil (mutex)]
    │
    └─> [Delay 3s] ──> Lặp lại
```

## 3. LUỒNG XỬ LÝ TINYML

```
[Task: TinyML]
    │
    ├─> [Khởi tạo TensorFlow Lite]
    │       │
    │       ├─> Load model
    │       ├─> Allocate tensor arena
    │       └─> Setup interpreter
    │
    ├─> [Vòng lặp]
    │       │
    │       ├─> [Lấy dữ liệu cảm biến]
    │       │       │
    │       │       ├─> input[0] = glob_temperature
    │       │       └─> input[1] = glob_humidity
    │       │
    │       ├─> [Chạy inference]
    │       │       │
    │       │       └─> interpreter->Invoke()
    │       │
    │       ├─> [Lấy kết quả]
    │       │       │
    │       │       └─> result = output->data.f[0]
    │       │
    │       ├─> [Phân loại]
    │       │       │
    │       │       ├─> [0.49 ≤ result ≤ 0.52] ──> "Normal"
    │       │       └─> [Khác] ──> "Warning!"
    │       │
    │       ├─> [Lưu kết quả]
    │       │       │
    │       │       ├─> glob_anomaly_score = result
    │       │       └─> glob_anomaly_message = "Normal"/"Warning!"
    │       │
    │       └─> [Delay 3s] ──> Lặp lại
```

## 4. LUỒNG ĐIỀU KHIỂN MÁY BƠM

```
[Task: May Bom]
    │
    ├─> [Khởi tạo GPIO 6 (OUTPUT)]
    │
    ├─> [Vòng lặp]
    │       │
    │       ├─> [Đọc dữ liệu]
    │       │       │
    │       │       ├─> glob_soil (mutex)
    │       │       ├─> pump_manual_control (mutex)
    │       │       └─> pump_state (mutex)
    │       │
    │       ├─> [Kiểm tra chế độ]
    │       │       │
    │       │       ├─> [MANUAL]
    │       │       │       │
    │       │       │       └─> [Điều khiển trực tiếp]
    │       │       │               │
    │       │       │               └─> digitalWrite(MAYBOM_PIN, pump_state)
    │       │       │
    │       │       └─> [AUTO]
    │       │               │
    │       │               ├─> [Pump đang BẬT]
    │       │               │       │
    │       │               │       └─> [soil < max?] ──> Giữ BẬT / TẮT
    │       │               │
    │       │               └─> [Pump đang TẮT]
    │       │                       │
    │       │                       └─> [soil < min?] ──> BẬT / Giữ TẮT
    │       │
    │       ├─> [Cập nhật GPIO]
    │       │
    │       ├─> [Cập nhật pump_state (mutex)]
    │       │
    │       └─> [Delay 100ms] ──> Lặp lại
```

## 5. LUỒNG WEB SERVER

```
[Task: Main Server]
    │
    ├─> [Kiểm tra WiFi Mode]
    │       │
    │       ├─> [AP Mode]
    │       │       │
    │       │       └─> [Tạo AP: 192.168.4.1]
    │       │
    │       └─> [STA Mode]
    │               │
    │               └─> [Kết nối WiFi → Lấy IP từ DHCP]
    │
    ├─> [Khởi tạo Web Server (Port 80)]
    │
    ├─> [Đăng ký Routes]
    │       │
    │       ├─> GET / ──> Dashboard
    │       ├─> GET /settings ──> Settings page
    │       ├─> GET /api/data ──> JSON data
    │       ├─> POST /api/pump ──> Control pump
    │       ├─> POST /api/wifi ──> Configure WiFi
    │       └─> POST /api/coreiot ──> Configure CoreIOT
    │
    ├─> [Vòng lặp]
    │       │
    │       ├─> [Xử lý client requests]
    │       │       │
    │       │       ├─> [Lấy dữ liệu từ CoreIOT hoặc local]
    │       │       │
    │       │       ├─> [Render HTML/JSON]
    │       │       │
    │       │       └─> [Gửi response]
    │       │
    │       └─> [Delay 10ms] ──> Lặp lại
```

## 6. LUỒNG COREIOT (MQTT)

```
[Task: CoreIOT]
    │
    ├─> [Chờ WiFi kết nối (semaphore)]
    │
    ├─> [Load credentials từ EEPROM]
    │       │
    │       ├─> coreiot_server
    │       ├─> coreiot_token / username+password
    │       └─> coreiot_client_id
    │
    ├─> [Khởi tạo MQTT Client]
    │       │
    │       ├─> client.setServer(server, 1883)
    │       └─> client.setCallback(callback)
    │
    ├─> [Vòng lặp]
    │       │
    │       ├─> [Kiểm tra kết nối]
    │       │       │
    │       │       ├─> [Chưa kết nối] ──> reconnect()
    │       │       │       │
    │       │       │       ├─> [Xác thực]
    │       │       │       ├─> [Subscribe topics]
    │       │       │       └─> [Gửi shared attributes]
    │       │       │
    │       │       └─> [Đã kết nối] ──> Tiếp tục
    │       │
    │       ├─> [Xử lý messages]
    │       │       │
    │       │       └─> client.loop()
    │       │               │
    │       │               ├─> [RPC Request] ──> Xử lý command
    │       │               └─> [Attributes] ──> Cập nhật coreiot_data
    │       │
    │       ├─> [Thu thập dữ liệu]
    │       │       │
    │       │       ├─> Lấy sensor data (mutex)
    │       │       ├─> Lấy pump state (mutex)
    │       │       └─> Lấy anomaly score
    │       │
    │       ├─> [Tạo JSON payload]
    │       │       │
    │       │       └─> {temperature, humidity, soil, pump_state, ...}
    │       │
    │       ├─> [Publish telemetry]
    │       │       │
    │       │       └─> client.publish("v1/devices/me/telemetry", payload)
    │       │
    │       ├─> [Cập nhật coreiot_data]
    │       │       │
    │       │       └─> is_valid = true
    │       │
    │       ├─> [Kiểm tra thay đổi pump_state (AUTO mode)]
    │       │       │
    │       │       └─> [Nếu thay đổi] ──> Publish attributes
    │       │
    │       └─> [Delay 3s] ──> Lặp lại
```

## 7. LUỒNG ĐỒNG BỘ DỮ LIỆU

```
[Sensors] ──> [Global Variables (Mutex)]
                    │
                    ├─> [CoreIOT Task]
                    │       │
                    │       └─> [Publish Telemetry]
                    │               │
                    │               └─> [CoreIOT Platform]
                    │                       │
                    │                       ├─> [Store Data]
                    │                       │
                    │                       └─> [Send Attributes]
                    │                               │
                    │                               └─> [ESP32]
                    │                                       │
                    │                                       └─> [Update coreiot_data]
                    │                                               │
                    │                                               ├─> [LCD Display]
                    │                                               │
                    │                                               └─> [Web Server]
```

## 8. LUỒNG RPC COMMAND

```
[CoreIOT Platform]
    │
    └─> [Gửi RPC Request]
            │
            └─> Topic: v1/devices/me/rpc/request/{id}
                    │
                    └─> Payload: {"method": "setPumpState", "params": true}
                            │
                            └─> [ESP32 MQTT Callback]
                                    │
                                    ├─> [Parse JSON]
                                    │
                                    ├─> [Xử lý command]
                                    │       │
                                    │       ├─> setPumpState ──> Cập nhật pump_state
                                    │       ├─> setPumpMode ──> Cập nhật pump_mode
                                    │       └─> getPumpMode ──> Trả về mode hiện tại
                                    │
                                    ├─> [Cập nhật local state (mutex)]
                                    │
                                    ├─> [Cập nhật coreiot_data]
                                    │
                                    ├─> [Publish attributes để đồng bộ]
                                    │
                                    └─> [Gửi RPC Response]
                                            │
                                            └─> Topic: v1/devices/me/rpc/response/{id}
                                                    │
                                                    └─> Payload: {"success": true, "message": "..."}
```

## 9. BẢNG TÓM TẮT GIAO THỨC

| Giao thức | Hướng | Tần suất | Dữ liệu | Mục đích |
|-----------|-------|----------|---------|----------|
| **I2C** | Bidirectional | 3s | Temperature, Humidity | Đọc cảm biến DHT20, LCD |
| **ADC** | Input | 3s | Soil moisture (0-100%) | Đọc cảm biến đất |
| **UART** | Bidirectional | Continuous | Log messages | Debug, monitoring |
| **WiFi** | Bidirectional | Continuous | Network packets | Kết nối Internet |
| **HTTP** | Request/Response | On-demand | HTML, JSON | Web interface, API |
| **MQTT** | Publish/Subscribe | 3s (telemetry) | JSON payload | CoreIOT platform |

## 10. TIMING DIAGRAM

```
Time ───────────────────────────────────────────────────────────>

Task: Temp/Humi
  │  [Read]───[Process]───[Display]───────────────────[Read]───
  │     │         │           │                           │
  │     └─────────┴───────────┴───────────────────────────┘
  │     0s        1s         2s         3s                 6s

Task: Soil Sensor
  │  [Read]───[Process]────────────────────────────────[Read]───
  │     │         │                                          │
  │     └─────────┴──────────────────────────────────────────┘
  │     0s        1s         3s                             6s

Task: TinyML
  │  [Inference]───[Process]──────────────────────────[Inference]
  │       │            │                                    │
  │       └────────────┴────────────────────────────────────┘
  │        0s          1s         3s                       6s

Task: Pump Control
  │  [Check]─[Check]─[Check]─[Check]─[Check]─[Check]─[Check]───
  │     │      │       │       │       │       │       │
  │     └──────┴───────┴───────┴───────┴───────┴───────┘
  │     0s    0.1s   0.2s   0.3s   0.4s   0.5s   0.6s

Task: CoreIOT
  │  [Publish]───────────────────────────────────────[Publish]───
  │      │                                                │
  │      └────────────────────────────────────────────────┘
  │       0s                   3s                        6s

Task: Web Server
  │  [Handle]─[Handle]─[Handle]─[Handle]─[Handle]─[Handle]──────
  │      │        │        │        │        │        │
  │      └────────┴────────┴────────┴────────┴────────┘
  │       0s     0.01s   0.02s   0.03s   0.04s   0.05s
```






