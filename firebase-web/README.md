# Firebase Frontend - ESP32 IoT Dashboard

Frontend web application được deploy trên Firebase Hosting để hiển thị dữ liệu từ ESP32 qua CoreIOT.

## Cấu trúc dự án

```
firebase-web/
├── public/
│   ├── index.html          # Trang Dashboard chính
│   ├── settings.html       # Trang cấu hình CoreIOT
│   ├── css/
│   │   └── styles.css      # CSS styles
│   └── js/
│       ├── config.js       # Cấu hình CoreIOT API
│       ├── chart.js        # Logic vẽ biểu đồ
│       └── app.js          # Logic ứng dụng chính
├── firebase.json           # Cấu hình Firebase Hosting
├── .firebaserc            # Firebase project config
└── README.md              # File này

```

## Cài đặt và Deploy

### 1. Cài đặt Firebase CLI

```bash
npm install -g firebase-tools
```

### 2. Đăng nhập Firebase

```bash
firebase login
```

### 3. Khởi tạo Firebase project

```bash
cd firebase-web
firebase init hosting
```

Chọn:
- Use an existing project hoặc Create a new project
- Public directory: `public`
- Configure as a single-page app: `Yes`
- Set up automatic builds: `No`

### 4. Cập nhật .firebaserc

Sửa file `.firebaserc` và thay `your-project-id` bằng Firebase project ID của bạn.

### 5. Deploy lên Firebase

```bash
firebase deploy --only hosting
```

## Cấu hình CoreIOT

1. Truy cập trang Settings: `https://your-project.web.app/settings.html`
2. Nhập CoreIOT Server (ví dụ: `app.coreiot.io`)
3. Nhập CoreIOT Access Token từ ESP32
4. Click "Save Configuration"
5. Frontend sẽ tự động lấy dữ liệu từ CoreIOT

## Cách hoạt động

1. **ESP32** gửi dữ liệu sensor lên **CoreIOT** (đã cấu hình WiFi và CoreIOT token trên ESP32)
2. **Firebase Frontend** lấy dữ liệu từ **CoreIOT API**:
   - Telemetry: `/api/v1/{token}/telemetry/latest`
   - Attributes: `/api/v1/{token}/attributes`
   - RPC Commands: `/api/v1/{token}/rpc`
3. Hiển thị dữ liệu real-time trên Dashboard

## Tính năng

- ✅ Hiển thị dữ liệu sensor (Temperature, Humidity, Soil Moisture)
- ✅ Biểu đồ real-time với Canvas
- ✅ Điều khiển Pump (ON/OFF, AUTO/MANUAL)
- ✅ Điều khiển LED (LED1, LED2 với color picker)
- ✅ Cấu hình CoreIOT Server và Token
- ✅ Responsive design cho mobile

## Lưu ý

- CoreIOT API endpoints có thể khác tùy theo version. Kiểm tra CoreIOT documentation để cập nhật đúng endpoints.
- Cần cấu hình CORS trên CoreIOT server nếu cần thiết.
- Access Token được lưu trong localStorage của browser.

