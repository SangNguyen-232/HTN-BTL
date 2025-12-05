#include "mainserver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include "global.h"

bool led1_state = false;
bool led2_state = false;
bool isAPMode = true;



WebServer server(80);
Adafruit_NeoPixel neoPixel(1, LED2_PIN, NEO_GRB + NEO_KHZ800);

unsigned long connect_start_ms = 0;
bool connecting = false;

// Thêm biến để tracking connection status
String connectionStatus = "idle";
String newIPAddress = "";

// Biến để lưu dữ liệu từ CoreIOT (struct được định nghĩa trong mainserver.h)
CoreIOTData coreiot_data;
bool use_coreiot_data = false;
unsigned long last_coreiot_fetch = 0;
const unsigned long COREIOT_FETCH_INTERVAL = 10000; // 10 giây

String mainPage() {
  String temperature, humidity, soil, score;
  String message, led1, led2, pumpState, pumpMode;
  
  // AP MODE: Sử dụng dữ liệu local trực tiếp, không cần CoreIOT
  if (isAPMode) {
    // Debug: Print AP mode status
    static unsigned long lastAPDebug = 0;
    if (millis() - lastAPDebug > 5000) {  // Every 5 seconds
      Serial.println("[WEB] AP Mode - using local sensor data");
      lastAPDebug = millis();
    }
    
    // Lấy dữ liệu từ local sensors (thread-safe)
    SensorData_t sensor_data;
    getSensorData(&sensor_data);
    temperature = String(sensor_data.temperature);
    humidity = String(sensor_data.humidity);
    soil = String(sensor_data.soil);
    score = String(glob_anomaly_score, 4);
    message = glob_anomaly_message;
    
    // Lấy trạng thái LED và Pump từ local state
    led1 = led1_state ? "ON" : "OFF";
    led2 = led2_state ? "ON" : "OFF";
    
    // Lấy pump state và mode từ local (thread-safe)
    bool current_pump_state;
    bool current_pump_mode;
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      current_pump_state = pump_state;
      current_pump_mode = pump_manual_control;
      xSemaphoreGive(xMutexPumpControl);
    } else {
      current_pump_state = pump_state;
      current_pump_mode = pump_manual_control;
    }
    pumpState = current_pump_state ? "ON" : "OFF";
    pumpMode = current_pump_mode ? "MANUAL" : "AUTO";
  }
  // STA MODE: Lấy dữ liệu từ CoreIOT
  // Flow: Sensors → CoreIOT → Mainserver/LCD hiển thị từ CoreIOT
  // Khi đợi dữ liệu mới từ CoreIOT, vẫn hiển thị giá trị hiện tại (last known values)
  else if (coreiot_data.is_valid) {
    // Luôn hiển thị giá trị từ CoreIOT (kể cả khi đợi cập nhật mới)
    temperature = String(coreiot_data.temperature);
    humidity = String(coreiot_data.humidity);
    soil = String(coreiot_data.soil);
    score = String(coreiot_data.anomaly_score, 4);
    message = coreiot_data.anomaly_message;
    led1 = coreiot_data.led1_state ? "ON" : "OFF";
    led2 = coreiot_data.led2_state ? "ON" : "OFF";
    pumpState = coreiot_data.pump_state ? "ON" : "OFF";
    pumpMode = coreiot_data.pump_mode;
  } else {
    // Chưa có dữ liệu từ CoreIOT lần đầu, lấy từ local để hiển thị
    SensorData_t sensor_data;
    getSensorData(&sensor_data);
    temperature = String(sensor_data.temperature);
    humidity = String(sensor_data.humidity);
    soil = String(sensor_data.soil);
    score = String(glob_anomaly_score, 4);
    message = glob_anomaly_message;
    
    // Lấy trạng thái LED và Pump từ local state
    led1 = led1_state ? "ON" : "OFF";
    led2 = led2_state ? "ON" : "OFF";
    
    // Lấy pump state và mode từ local (thread-safe)
    bool current_pump_state;
    bool current_pump_mode;
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      current_pump_state = pump_state;
      current_pump_mode = pump_manual_control;
      xSemaphoreGive(xMutexPumpControl);
    } else {
      current_pump_state = pump_state;
      current_pump_mode = pump_manual_control;
    }
    pumpState = current_pump_state ? "ON" : "OFF";
    pumpMode = current_pump_mode ? "MANUAL" : "AUTO";
  }
  
  // Load pump thresholds (chỉ cần load một lần khi khởi động)
  static bool thresholds_loaded = false;
  if (!thresholds_loaded) {
    loadPumpThresholds();
    thresholds_loaded = true;
  }
  
  String thresholdMin = String(pump_threshold_min);
  String thresholdMax = String(pump_threshold_max);

  return R"rawliteral(
    <!DOCTYPE html><html><head>
      <meta charset='utf-8'>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>L06 - NHÓM 7</title>
      <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
      <style>
        :root{
          --bg:#0f172a;
          --card:#111827;
          --muted:#9ca3af;
          --text:#e5e7eb;
          --primary:#3b82f6;
          --accent:#22d3ee;
          --success:#22c55e;
          --warning:#f59e0b;
          --shadow:0 10px 30px rgba(0,0,0,0.35);
          --radius:16px;
        }
        *{box-sizing:border-box}
        body {
          margin:0;
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, "Noto Sans", "Apple Color Emoji", "Segoe UI Emoji";
          color:var(--text);
          background:
            radial-gradient(1200px 600px at -10% -10%, rgba(59,130,246,.15), transparent 40%),
            radial-gradient(800px 400px at 120% 10%, rgba(34,211,238,.12), transparent 40%),
            linear-gradient(180deg, #0b1220, #0f172a 30%, #0b1220 100%);
          min-height:100vh;
          display:flex;
          padding:0;
        }
        .app-layout {
          display:flex;
          width:100%;
          min-height:100vh;
        }
        .sidebar {
          width:280px;
          background: linear-gradient(180deg, rgba(255,255,255,0.06), rgba(255,255,255,0.03)), var(--card);
          border-right:1px solid rgba(255,255,255,0.08);
          backdrop-filter: blur(12px);
          display:flex;
          flex-direction:column;
          position:fixed;
          height:100vh;
          z-index:100;
          overflow-y:auto;
        }
        .sidebar-header {
          padding:24px 20px 16px;
          border-bottom:1px solid rgba(255,255,255,0.06);
        }
        .sidebar-brand {
          display:flex;
          align-items:center;
          gap:12px;
          font-weight:800;
          font-size:18px;
          letter-spacing:.3px;
        }
        .sidebar-brand .logo {
          width:36px;height:36px;border-radius:10px;
          background: conic-gradient(from 180deg at 50% 50%, var(--accent), var(--primary), var(--accent));
          filter: drop-shadow(0 6px 14px rgba(59,130,246,.45));
          display:flex;
          align-items:center;
          justify-content:center;
        }
        .sidebar-brand .logo svg {
          width:20px;
          height:20px;
          color:white;
        }
        .sidebar-nav {
          flex:1;
          padding:20px 0;
        }
        .nav-item {
          margin:0 16px 8px;
        }
        .nav-link {
          display:flex;
          align-items:center;
          gap:14px;
          padding:14px 16px;
          border-radius:12px;
          color:var(--muted);
          text-decoration:none;
          font-weight:600;
          font-size:15px;
          transition: all .2s ease;
          border:1px solid transparent;
        }
        .nav-link:hover {
          background: rgba(255,255,255,0.05);
          color:var(--text);
          border-color: rgba(255,255,255,0.1);
        }
        .nav-link.active {
          background: linear-gradient(135deg, rgba(59,130,246,0.2), rgba(59,130,246,0.1));
          color:#93c5fd;
          border-color: rgba(59,130,246,0.3);
        }
        .nav-icon {
          width:20px;
          height:20px;
          flex-shrink:0;
        }
        .nav-icon svg { width:100%; height:100%; stroke-width:1.8; stroke-linecap:round; stroke-linejoin:round; stroke:currentColor; fill:none; }
        .main-content {
          flex:1;
          margin-left:280px;
          padding:24px;
          overflow-y:auto;
        }
        .container {
          width:100%;
          max-width:900px;
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02)) , var(--card);
          border:1px solid rgba(255,255,255,0.08);
          border-radius: var(--radius);
          box-shadow: var(--shadow);
          padding: 22px 22px 18px;
          backdrop-filter: blur(8px);
        }
        .mobile-nav-toggle {
          display:none;
          position:fixed;
          top:20px;
          left:20px;
          z-index:200;
          background: var(--card);
          border:1px solid rgba(255,255,255,0.1);
          border-radius:10px;
          padding:12px;
          cursor:pointer;
          backdrop-filter: blur(8px);
        }
        .mobile-nav-toggle svg {
          width:24px;
          height:24px;
          color:var(--text);
        }
        @media (max-width: 768px) {
          .sidebar {
            transform: translateX(-100%);
            transition: transform .3s ease;
          }
          .sidebar.open {
            transform: translateX(0);
          }
          .main-content {
            margin-left:0;
            padding:80px 16px 16px;
          }
          .mobile-nav-toggle {
            display:block;
          }
        }
        .header{
          display:flex;
          align-items:center;
          justify-content:space-between;
          margin-bottom:16px;
        }
        .title{
          display:flex;
          align-items:center;
          gap:10px;
          font-weight:700;
          letter-spacing:.3px;
          font-size:20px;
        }
        .data-source {
          display:flex;
          align-items:center;
        }
        .source-indicator {
          display:flex;
          align-items:center;
          gap:8px;
          padding:6px 12px;
          background: rgba(255,255,255,0.05);
          border:1px solid rgba(255,255,255,0.1);
          border-radius:20px;
          font-size:12px;
          font-weight:600;
        }
        .source-dot {
          width:8px;
          height:8px;
          border-radius:50%;
          background:#22c55e;
          animation:pulse 2s infinite;
        }
        .source-dot.coreiot {
          background:#3b82f6;
        }
        .source-dot.local {
          background:#f59e0b;
        }
        .source-text {
          color:var(--muted);
          text-transform:uppercase;
          letter-spacing:.5px;
        }
        @keyframes pulse {
          0%, 100% { opacity:1; }
          50% { opacity:0.5; }
        }
        .grid{
          display:grid;
          grid-template-columns: repeat(5, 1fr);
          grid-auto-rows: 1fr;
          align-items: stretch;
          gap:12px;
          margin: 10px 0 6px;
        }
        .metric{
          position:relative;
          overflow:hidden;
          background: linear-gradient(180deg, rgba(255,255,255,0.06), rgba(255,255,255,0.02));
          border:1px solid rgba(255,255,255,0.08);
          border-radius:14px;
          padding:22px;
          box-shadow: inset 0 1px 0 rgba(255,255,255,0.04);
          transition: transform 0.15s ease, box-shadow 0.15s ease;
          min-width: 0;
          display: flex;
          flex-direction: column;
        }
        .metric::after{
          content:"";
          position:absolute;
          inset:0;
          opacity:0;
          transition:opacity 0.2s ease;
          border-radius:inherit;
        }
        .metric:hover{
          transform: translateY(-2px);
          box-shadow: rgba(15,23,42,0.35) 0 10px 25px;
        }
        .metric:hover::after{ opacity:0.12; }
        .metric-temp{
          background: linear-gradient(145deg, rgba(249,115,22,0.22), rgba(217,70,239,0.18), rgba(15,23,42,0.9));
        }
        .metric-temp::after{
          background: radial-gradient(circle at 100% 0%, rgba(255,161,90,0.6), transparent 55%);
        }
        .metric-humid{
          background: linear-gradient(145deg, rgba(56,189,248,0.22), rgba(14,165,233,0.18), rgba(15,23,42,0.9));
        }
        .metric-humid::after{
          background: radial-gradient(circle at 100% 0%, rgba(125,211,252,0.6), transparent 55%);
        }
        .metric-time{
          background: linear-gradient(145deg, rgba(168,85,247,0.22), rgba(139,92,246,0.18), rgba(15,23,42,0.9));
        }
        .metric-time::after{
          background: radial-gradient(circle at 100% 0%, rgba(196,181,253,0.55), transparent 55%);
        }
        .metric-soil{
          background: linear-gradient(145deg, rgba(34,197,94,0.22), rgba(22,163,74,0.18), rgba(15,23,42,0.9));
        }
        .metric-soil::after{
          background: radial-gradient(circle at 100% 0%, rgba(74,222,128,0.6), transparent 55%);
        }
        .metric-score-message{
          background: linear-gradient(145deg, rgba(147,51,234,0.22), rgba(126,34,206,0.18), rgba(15,23,42,0.9));
        }
        .metric-score-message::after{
          background: radial-gradient(circle at 100% 0%, rgba(196,181,253,0.6), transparent 55%);
        }
        .metric-header{
          display:flex;
          align-items:center;
          gap:10px;
          position:relative;
        }
        .metric .label{
          color:var(--muted);
          font-size:15px;
          letter-spacing:.4px;
        }
        .icon-wrap{
          width:34px;height:34px;
          border-radius:10px;
          display:flex;
          align-items:center;
          justify-content:center;
          background: rgba(255,255,255,0.12);
          box-shadow: inset 0 1px 0 rgba(255,255,255,0.25);
        }
        .icon{
          width:20px;
          height:20px;
        }
        .icon svg{ width:100%; height:100%; stroke-width:1.8; stroke-linecap:round; stroke-linejoin:round; stroke:currentColor; fill:none; }
        .metric-temp .icon{ color:rgba(251,191,36,0.95); }
        .metric-humid .icon{ color:rgba(125,211,252,0.95); }
        .metric-time .icon{ color:rgba(196,181,253,0.95); }
        .metric-soil .icon{ color:rgba(74,222,128,0.95); }
        .metric-score-message .icon{ color:rgba(196,181,253,0.95); }
        .metric .value{
          margin-top:8px;
          font-size:26px;
          font-weight:800;
          letter-spacing:.3px;
          text-align:center;
          word-wrap: break-word;
          overflow-wrap: break-word;
          flex: 1;
          display: flex;
          align-items: center;
          justify-content: center;
          min-height: 0;
        }
        .metric .value.loading {
          color: var(--muted);
          font-style: italic;
          font-weight: 600;
          font-size: 18px;
        }
        .metric-location .value{
          white-space: normal;
          line-height: 1.3;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          position: relative;
        }
        .location-map {
          width: 100%;
          height: 150px;
          border-radius: 8px;
          border: 1px solid rgba(255,255,255,0.1);
          background: rgba(255,255,255,0.05);
          margin-top: 8px;
          overflow: hidden;
        }
        .location-coordinates {
          font-size: 12px;
          color: var(--muted);
          margin-top: 4px;
          font-family: monospace;
        }
        .location-status {
          font-size: 14px;
          padding: 4px 8px;
          border-radius: 6px;
          background: rgba(34,197,94,0.15);
          color: #86efac;
          border: 1px solid rgba(34,197,94,0.3);
        }
        .location-error {
          background: rgba(239,68,68,0.15);
          color: #fca5a5;
          border-color: rgba(239,68,68,0.3);
        }
        .metric-score-message .value{
          white-space: normal;
          line-height: 1.3;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          gap: 8px;
        }
        .metric-score-message .value .score-line{
          font-size: 26px;
          font-weight: 800;
          letter-spacing: .3px;
        }
        .metric-score-message .value .message-line{
          font-size: 26px;
          font-weight: 800;
          letter-spacing: .3px;
        }
        .metric-temp .value,
        .metric-humid .value,
        .metric-soil .value {
          font-size: 26px;
        }
        
        /* PUMP CONTROLS */
        .pump-control{
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
          border:1px solid rgba(255,255,255,0.08);
          border-radius:14px;
          padding:16px;
          margin-bottom:12px;
        }
        .pump-header{
          display:flex;
          align-items:center;
          justify-content:space-between;
          margin-bottom:12px;
        }
        .pump-label{
          font-weight:700;
          font-size:16px;
        }
        .pump-status{
          padding:6px 12px;
          border-radius:999px;
          font-size:12px;
          font-weight:700;
          letter-spacing:.4px;
          will-change: contents;
        }
        .pump-status.on{ 
          background: rgba(34,197,94,.15); 
          color:#86efac; 
          border:1px solid rgba(34,197,94,.35); 
        }
        .pump-status.off{ 
          background: rgba(239,68,68,.12); 
          color:#fca5a5; 
          border:1px solid rgba(239,68,68,.30); 
        }
        .pump-status.auto{ 
          background: rgba(59,130,246,.15); 
          color:#93c5fd; 
          border:1px solid rgba(59,130,246,.35); 
        }
        .pump-status.manual{ 
          background: rgba(251,146,60,.15); 
          color:#fbbf24; 
          border:1px solid rgba(251,146,60,.35); 
        }
        .pump-toggle-btn{
          appearance:none;
          border:0;
          cursor:pointer;
          font-weight:700;
          padding:8px 16px;
          border-radius:10px;
          transition: all 0.2s ease;
          background: linear-gradient(180deg, rgba(59,130,246,.25), rgba(59,130,246,.15));
          color:#93c5fd;
          border:1px solid rgba(59,130,246,.35);
        }
        .pump-toggle-btn:hover:not(:disabled){ 
          background: linear-gradient(180deg, rgba(59,130,246,.35), rgba(59,130,246,.25));
          transform: translateY(-1px);
        }
        .pump-toggle-btn:active:not(:disabled){
          opacity: 0.8;
          transform: translateY(0);
        }
        .pump-toggle-btn:disabled{
          cursor: not-allowed;
          opacity: 0.6;
        }
        
        /* LED CONTROLS */
        .led-control{
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
          border:1px solid rgba(255,255,255,0.08);
          border-radius:14px;
          padding:16px;
          margin-bottom:12px;
        }
        .led-header{
          display:flex;
          align-items:center;
          justify-content:space-between;
          margin-bottom:12px;
        }
        .led-name{ font-weight:700; font-size:16px; }
        .led-status{
          padding:6px 10px;
          border-radius:999px;
          font-size:12px;
          font-weight:700;
          letter-spacing:.4px;
        }
        .led-status.on{ 
          background: rgba(34,197,94,.15); 
          color:#86efac; 
          border:1px solid rgba(34,197,94,.35); 
        }
        .led-status.off{ 
          background: rgba(239,68,68,.12); 
          color:#fca5a5; 
          border:1px solid rgba(239,68,68,.30); 
        }
        .led-buttons{
          display:flex;
          gap:8px;
          margin-bottom:8px;
        }
        .btn-on, .btn-off{
          flex:1;
          appearance:none;
          border:0;
          cursor:pointer;
          font-weight:700;
          padding:10px 14px;
          border-radius:10px;
          transition: all .2s ease;
        }
        .btn-on{
          background: linear-gradient(180deg, rgba(34,197,94,.25), rgba(34,197,94,.15));
          color:#86efac;
          border:1px solid rgba(34,197,94,.35);
        }
        .btn-off{
          background: linear-gradient(180deg, rgba(239,68,68,.25), rgba(239,68,68,.15));
          color:#fca5a5;
          border:1px solid rgba(239,68,68,.35);
        }
        .btn-on:hover{ 
          background: linear-gradient(180deg, rgba(34,197,94,.35), rgba(34,197,94,.25));
          transform: translateY(-1px);
        }
        .btn-off:hover{ 
          background: linear-gradient(180deg, rgba(239,68,68,.35), rgba(239,68,68,.25));
          transform: translateY(-1px);
        }
        
        /* COLOR PICKER */
        .color-section{
          margin-top:12px;
          padding-top:12px;
          border-top:1px solid rgba(255,255,255,0.06);
        }
        .color-label{
          font-size:13px;
          color:var(--muted);
          margin-bottom:8px;
          font-weight:600;
        }
        .color-grid{
          display:grid;
          grid-template-columns: repeat(7, 1fr);
          gap:8px;
          margin-bottom:10px;
        }
        .color-btn{
          width:100%;
          aspect-ratio: 1;
          border-radius:8px;
          border:2px solid rgba(255,255,255,0.15);
          cursor:pointer;
          transition: transform .15s ease, border-color .2s ease, box-shadow .2s ease;
        }
        .color-btn:hover{
          transform: scale(1.15);
          border-color: rgba(255,255,255,0.5);
          box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        }
        .color-btn:active{
          transform: scale(1.05);
        }
        .custom-color{
          display:flex;
          gap:8px;
          align-items:center;
        }
        .custom-color span{
          font-size:12px;
          color:var(--muted);
          font-weight:600;
        }
        .color-input{
          width:50px;
          height:32px;
          border:2px solid rgba(255,255,255,0.15);
          border-radius:8px;
          cursor:pointer;
          background: transparent;
          transition: border-color .2s ease;
        }
        .color-input:hover{
          border-color: rgba(255,255,255,0.35);
        }
        
        .footer{
          margin-top:14px;
          text-align:center;
          color:var(--muted);
          font-size:12px;
          font-weight:600;
          letter-spacing:1px;
        }
        
        .refresh-control {
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
          border:1px solid rgba(255,255,255,0.08);
          border-radius:14px;
          padding:16px;
          margin-bottom:12px;
        }
        .refresh-header {
          display:flex;
          align-items:center;
          justify-content:space-between;
          margin-bottom:12px;
        }
        .refresh-label {
          font-weight:700;
          font-size:16px;
          display:flex;
          align-items:center;
          gap:8px;
        }
        .refresh-label svg {
          width:20px;
          height:20px;
          color:var(--accent);
        }
        .refresh-current {
          padding:6px 12px;
          border-radius:999px;
          font-size:12px;
          font-weight:700;
          letter-spacing:.4px;
          background: rgba(34,211,238,.15);
          color:#7dd3fc;
          border:1px solid rgba(34,211,238,.35);
        }
        .refresh-controls {
          display:flex;
          gap:8px;
          align-items:center;
        }
        .refresh-input {
          background: rgba(255,255,255,.04);
          border:1px solid rgba(255,255,255,.10);
          border-radius:8px;
          padding:8px 12px;
          color:var(--text);
          font-size:14px;
          font-weight:600;
          width:80px;
          text-align:center;
        }
        .refresh-input:focus {
          outline:none;
          border-color: rgba(34,211,238,0.5);
          background: rgba(255,255,255,.06);
        }
        .refresh-btn {
          appearance:none;
          border:0;
          cursor:pointer;
          font-weight:700;
          padding:8px 12px;
          border-radius:8px;
          transition: all 0.2s ease;
          background: linear-gradient(180deg, rgba(34,211,238,.25), rgba(34,211,238,.15));
          color:#7dd3fc;
          border:1px solid rgba(34,211,238,.35);
          font-size:12px;
        }
        .refresh-btn:hover {
          background: linear-gradient(180deg, rgba(34,211,238,.35), rgba(34,211,238,.25));
          transform: translateY(-1px);
        }
        .refresh-presets {
          display:flex;
          gap:6px;
          margin-top:8px;
        }
        .preset-btn {
          appearance:none;
          border:1px solid rgba(255,255,255,0.15);
          background: rgba(255,255,255,0.05);
          color:var(--muted);
          padding:6px 10px;
          border-radius:6px;
          font-size:12px;
          font-weight:600;
          cursor:pointer;
          transition: all 0.2s ease;
        }
        .preset-btn:hover {
          background: rgba(34,211,238,0.15);
          border-color: rgba(34,211,238,0.3);
          color:#7dd3fc;
        }
        .chart-container{
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
          border:1px solid rgba(255,255,255,0.08);
          border-radius:14px;
          padding:16px;
          margin-bottom:12px;
          overflow:hidden;
        }
        .chart-header{
          display:flex;
          align-items:center;
          gap:10px;
          margin-bottom:12px;
        }
        .chart-title{
          font-weight:700;
          font-size:16px;
          color:var(--text);
        }
        .chart-icon{
          width:20px;
          height:20px;
          color:rgba(59,130,246,0.95);
        }
        .chart-icon svg { width:100%; height:100%; stroke-width:2; stroke-linecap:round; stroke-linejoin:round; stroke:currentColor; fill:none; }
        .chart-wrapper{
          width:100%;
          height:300px;
          position:relative;
        }
        .map-container{
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02));
          border:1px solid rgba(255,255,255,0.08);
          border-radius:14px;
          padding:16px;
          margin-bottom:12px;
          overflow:hidden;
        }
        .map-header{
          display:flex;
          align-items:center;
          gap:10px;
          margin-bottom:12px;
        }
        .map-title{
          font-weight:700;
          font-size:16px;
          color:var(--text);
        }
        .map-frame{
          width:100%;
          height:300px;
          border:2px solid rgba(255,255,255,0.1);
          border-radius:12px;
          overflow:hidden;
        }
        .map-icon{
          width:20px;
          height:20px;
          color:rgba(52,211,153,0.95);
        }
        .map-icon svg { width:100%; height:100%; stroke-width:2; stroke-linecap:round; stroke-linejoin:round; stroke:currentColor; fill:none; }
        @media (max-width: 480px){
          .grid{ grid-template-columns: 1fr; }
          .metric-time{ grid-column: 1; }
          .metric-score-message{ grid-column: 1; }
          .refresh-controls{ flex-wrap: wrap; }
          .refresh-presets{ flex-wrap: wrap; }
          .map-frame{ height:250px; }
        }
      </style>
    </head>
    <body>
      <div class="app-layout">
        <div class="mobile-nav-toggle" onclick="toggleSidebar()">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <line x1="3" y1="6" x2="21" y2="6"></line>
            <line x1="3" y1="12" x2="21" y2="12"></line>
            <line x1="3" y1="18" x2="21" y2="18"></line>
          </svg>
        </div>
        
        <div class="sidebar" id="sidebar">
          <div class="sidebar-header">
            <div class="sidebar-brand">
              <div class="logo"></div>
              <span>ESP32 IoT</span>
            </div>
          </div>
          
          <nav class="sidebar-nav">
            <div class="nav-item">
              <a href="/" class="nav-link active">
                <div class="nav-icon">
                  <svg viewBox="0 0 24 24">
                    <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"></path>
                    <polyline points="9,22 9,12 15,12 15,22"></polyline>
                  </svg>
                </div>
                Dashboard
              </a>
            </div>
            
            <div class="nav-item">
              <a href="/settings" class="nav-link">
                <div class="nav-icon">
                  <svg viewBox="0 0 24 24">
                    <circle cx="12" cy="12" r="3"></circle>
                    <path d="M12 1v6m0 12v6m11-7h-6m-12 0H1m15.5-6.5l-4.24 4.24M6.74 6.74L2.5 2.5m15 15l-4.24-4.24M6.74 17.26L2.5 21.5"></path>
                  </svg>
                </div>
                Settings
              </a>
            </div>
          </nav>
        </div>
        
        <div class="main-content">
          <div class='container'>
            <div class="header">
              <div class="title">Dashboard</div>
              <div class="data-source" id="data-source">
                <div class="source-indicator">
                  <span class="source-dot" id="source-dot"></span>
                  <span class="source-text" id="source-text">)rawliteral" + String(coreiot_data.is_valid ? "CoreIOT" : (coreiot_server.length() > 0 ? "Loading" : "Not Configured")) + R"rawliteral(</span>
                </div>
              </div>
            </div>
        
        <!-- SENSOR METRICS -->
        <div class="grid">
          <div class="metric metric-temp">
            <div class="metric-header">
              <div class="icon-wrap">
                <div class="icon">
                  <svg viewBox="0 0 24 24">
                    <path d="M10 13.5V5a2 2 0 0 1 4 0v8.5a4 4 0 1 1-4 0z"/>
                    <circle cx="12" cy="18" r="1.8" fill="currentColor" opacity="0.25"/>
                    <circle cx="12" cy="18" r="1.1"/>
                    <line x1="12" y1="10" x2="12" y2="15"/>
                  </svg>
                </div>
              </div>
              <div class="label">Temperature</div>
            </div>
            <div class="value )rawliteral" + (temperature == "--" ? "loading" : "") + R"rawliteral("><span id='temp'>)rawliteral" + temperature + R"rawliteral(</span><span class="unit">)rawliteral" + (temperature == "--" ? "" : "&deg;C") + R"rawliteral(</span></div>
          </div>
          <div class="metric metric-humid">
            <div class="metric-header">
              <div class="icon-wrap">
                <div class="icon">
                  <svg viewBox="0 0 24 24">
                    <path d="M12 3c-2.5 3.3-5 6.5-5 9.5a5 5 0 0 0 10 0C17 9.5 14.5 6.3 12 3z" fill="currentColor" opacity="0.25"/>
                    <path d="M12 3c-2.5 3.3-5 6.5-5 9.5a5 5 0 0 0 10 0C17 9.5 14.5 6.3 12 3z"/>
                  </svg>
                </div>
              </div>
              <div class="label">Humidity</div>
            </div>
            <div class="value )rawliteral" + (humidity == "--" ? "loading" : "") + R"rawliteral("><span id='hum'>)rawliteral" + humidity + R"rawliteral(</span><span class="unit">)rawliteral" + (humidity == "--" ? "" : "%") + R"rawliteral(</span></div>
          </div>
          <div class="metric metric-soil">
            <div class="metric-header">
              <div class="icon-wrap">
                <div class="icon">
                  <svg viewBox="0 0 24 24">
                    <path d="M12 20v-6" fill="currentColor" opacity="0.25"/>
                    <path d="M12 20v-6"/>
                    <path d="M8 14c0-2 1.5-4 4-4s4 2 4 4" fill="currentColor" opacity="0.25"/>
                    <path d="M8 14c0-2 1.5-4 4-4s4 2 4 4"/>
                    <path d="M10 12c-1 0-2-1-2-2s1-2 2-2 2 1 2 2-1 2-2 2z" fill="currentColor" opacity="0.3"/>
                    <path d="M14 12c-1 0-2-1-2-2s1-2 2-2 2 1 2 2-1 2-2 2z" fill="currentColor" opacity="0.3"/>
                    <path d="M12 14c-1 0-2-1-2-2s1-2 2-2 2 1 2 2-1 2-2 2z" fill="currentColor" opacity="0.3"/>
                    <path d="M11 20h2v2h-2z" fill="currentColor" opacity="0.4"/>
                  </svg>
                </div>
              </div>
              <div class="label">Soil moisture</div>
            </div>
            <div class="value )rawliteral" + (soil == "--" ? "loading" : "") + R"rawliteral("><span id='soil'>)rawliteral" + (soil == "--" ? "--" : ((soil.toFloat() < 10 ? "0" : "") + String((int)soil.toFloat()))) + R"rawliteral(</span><span class="unit">)rawliteral" + (soil == "--" ? "" : "%") + R"rawliteral(</span></div>
          </div>
          <div class="metric metric-time">
            <div class="metric-header">
              <div class="icon-wrap">
                <div class="icon">
                  <svg viewBox="0 0 24 24">
                    <circle cx="12" cy="12" r="10" fill="currentColor" opacity="0.25"/>
                    <circle cx="12" cy="12" r="10"/>
                    <line x1="12" y1="12" x2="12" y2="7"/>
                    <line x1="12" y1="12" x2="16" y2="12"/>
                  </svg>
                </div>
              </div>
              <div class="label">Time</div>
            </div>
            <div class="value" id='time'>--:--:--</div>
          </div>
          <div class="metric metric-score-message">
            <div class="metric-header">
              <div class="icon-wrap">
                <div class="icon">
                  <svg viewBox="0 0 24 24">
                    <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z" fill="currentColor" opacity="0.25"/>
                    <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
                  </svg>
                </div>
              </div>
              <div class="label">Score and Message</div>
            </div>
            <div class="value )rawliteral" + (score == "--" ? "loading" : "") + R"rawliteral(">
              <div class="score-line"><span id='score'>)rawliteral" + score + R"rawliteral(</span></div>
              <div class="message-line )rawliteral" + (message.indexOf("Waiting") >= 0 ? "loading" : "") + R"rawliteral(" id='message'>)rawliteral" + message + R"rawliteral(</div>
            </div>
          </div>
        </div>
        
        <!-- CHART DISPLAY -->
        <div class="chart-container">
          <div class="chart-header">
            <div class="chart-icon">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polyline points="22 6 13.5 15.5 8.5 10.5 2 17"></polyline>
                <polyline points="16 6 22 6 22 12"></polyline>
              </svg>
            </div>
            <div class="chart-title">Sensor Data Chart</div>
          </div>
          <div class="chart-wrapper">
            <canvas id="sensorChart"></canvas>
          </div>
        </div>
        
        <!-- MAP DISPLAY -->
        <div class="map-container">
          <div class="map-header">
            <div class="map-icon">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"></path>
                <circle cx="12" cy="10" r="3"></circle>
              </svg>
            </div>
            <div class="map-title">Current Location</div>
          </div>
          <iframe 
            class="map-frame"
            src="https://www.openstreetmap.org/export/embed.html?bbox=106.80133605864662%2C10.875018410410052%2C106.81133605864662%2C10.885018410410052&layer=mapnik&marker=10.880018410410052%2C106.80633605864662"
            frameborder="0"
            allowfullscreen>
          </iframe>
        </div>
        
        <!-- PUMP CONTROLS -->
        <div class="pump-control" style="margin-top:14px;">
          <div class="pump-header">
            <div class="pump-label">PUMP</div>
            <div class="pump-status" id="pump-state-status">)rawliteral" + pumpState + R"rawliteral(</div>
            <button class="pump-toggle-btn" id="pump-toggle-btn" onclick="togglePump()" title="Toggle pump ON/OFF">Toggle</button>
          </div>
        </div>
        
        <div class="pump-control">
          <div class="pump-header">
            <div class="pump-label">MODE</div>
            <div class="pump-status" id="pump-mode-status">)rawliteral" + pumpMode + R"rawliteral(</div>
            <button class="pump-toggle-btn" onclick="togglePumpMode()" title="Switch between AUTO and MANUAL mode">Toggle</button>
          </div>
        </div>
        
        <!-- PUMP THRESHOLD CONTROL (only visible in AUTO mode) -->
        <div class="refresh-control" id="pump-threshold-control" style="margin-top:14px;display:none;">
          <div class="refresh-header">
            <div class="refresh-label">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"></path>
              </svg>
              Pump Threshold (AUTO Mode)
            </div>
            <div class="refresh-current" id="threshold-current">Min: )rawliteral" + thresholdMin + R"rawliteral(% | Max: )rawliteral" + thresholdMax + R"rawliteral(%</div>
          </div>
          <div class="refresh-controls" style="flex-wrap:wrap;gap:8px;">
            <div style="display:flex;align-items:center;gap:6px;">
              <label style="color:var(--muted);font-size:12px;font-weight:600;">Min:</label>
              <input type="number" class="refresh-input" id="threshold-min-input" min="0" max="100" value=")rawliteral" + thresholdMin + R"rawliteral(" step="0.1" style="width:80px;">
              <span style="color:var(--muted);font-size:12px;font-weight:600;">%</span>
            </div>
            <div style="display:flex;align-items:center;gap:6px;">
              <label style="color:var(--muted);font-size:12px;font-weight:600;">Max:</label>
              <input type="number" class="refresh-input" id="threshold-max-input" min="0" max="100" value=")rawliteral" + thresholdMax + R"rawliteral(" step="0.1" style="width:80px;">
              <span style="color:var(--muted);font-size:12px;font-weight:600;">%</span>
            </div>
            <button class="refresh-btn" onclick="updatePumpThresholds()">Apply</button>
          </div>
          <div style="margin-top:8px;font-size:11px;color:var(--muted);line-height:1.4;">
          </div>
        </div>
        
        <!-- REFRESH RATE CONTROL -->
        <div class="refresh-control" style="margin-top:14px;">
          <div class="refresh-header">
            <div class="refresh-label">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polyline points="23 4 23 10 17 10"></polyline>
                <polyline points="1 20 1 14 7 14"></polyline>
                <path d="M20.49 9A9 9 0 0 0 5.64 5.64L1 10m22 4l-4.64 4.36A9 9 0 0 1 3.51 15"></path>
              </svg>
              Data Refresh Rate
            </div>
            <div class="refresh-current" id="refresh-current">5s</div>
          </div>
          <div class="refresh-controls">
            <input type="number" class="refresh-input" id="refresh-input" min="1" max="60" value="5" step="1">
            <span style="color:var(--muted);font-size:12px;font-weight:600;">seconds</span>
            <button class="refresh-btn" onclick="updateRefreshRate()">Apply</button>
          </div>
          <div class="refresh-presets">
            <button class="preset-btn" onclick="setPresetRefresh(1)">1s</button>
            <button class="preset-btn" onclick="setPresetRefresh(2)">2s</button>
            <button class="preset-btn" onclick="setPresetRefresh(5)">5s</button>
            <button class="preset-btn" onclick="setPresetRefresh(10)">10s</button>
            <button class="preset-btn" onclick="setPresetRefresh(30)">30s</button>
          </div>
        </div>
        
        <!-- LED CONTROLS -->
        <div class="led-control" style="margin-top:14px;">
          <div class="led-header">
            <div class="led-name">💡 LED 1 (Regular)</div>
            <div class="led-status" id="led1-status">)rawliteral" + led1 + R"rawliteral(</div>
          </div>
          <div class="led-buttons">
            <button class="btn-on" onclick="controlLED(1, 'on')">ON</button>
            <button class="btn-off" onclick="controlLED(1, 'off')">OFF</button>
          </div>
        </div>
        
        <div class="led-control">
          <div class="led-header">
            <div class="led-name">🌈 LED 2 (NeoPixel)</div>
            <div class="led-status" id="led2-status">)rawliteral" + led2 + R"rawliteral(</div>
          </div>
          <div class="led-buttons">
            <button class="btn-on" onclick="controlLED(2, 'on', currentColor)">ON</button>
            <button class="btn-off" onclick="controlLED(2, 'off')">OFF</button>
          </div>
          
          <!-- COLOR PICKER -->
          <div class="color-section">
            <div class="color-label">Choose Color:</div>
            <div class="color-grid">
              <div class="color-btn" style="background:#ff0000" onclick="setColor('red')" title="Red"></div>
              <div class="color-btn" style="background:#00ff00" onclick="setColor('green')" title="Green"></div>
              <div class="color-btn" style="background:#0000ff" onclick="setColor('blue')" title="Blue"></div>
              <div class="color-btn" style="background:#ffff00" onclick="setColor('yellow')" title="Yellow"></div>
              <div class="color-btn" style="background:#ff00ff" onclick="setColor('purple')" title="Purple"></div>
              <div class="color-btn" style="background:#00ffff" onclick="setColor('cyan')" title="Cyan"></div>
              <div class="color-btn" style="background:#ffffff" onclick="setColor('white')" title="White"></div>
            </div>
            <div class="custom-color">
              <span>Custom:</span>
              <input type="color" class="color-input" id="customColor" onchange="setCustomColor()" value="#ffffff">
            </div>
          </div>
        </div>
        
            <div class="footer">EMBEDDED SYSTEM - ASSIGNMENT</div>
          </div>
        </div>
      </div>
      
      
      <script>
        let currentColor = 'white';
        let sensorUpdateInterval;
        let pumpUpdateInterval;
        let dataSourceUpdateInterval;
        // Thiết lập refresh rate dựa trên mode
        const isAPMode = window.location.hostname === '192.168.4.1' || window.location.hostname.includes('192.168.4');
        let currentRefreshRate = isAPMode ? 3 : 5; // 3s for AP mode, 5s for STA mode
        
        // Chart variables
        let sensorChart = null;
        const maxDataPoints = 50; // Giữ tối đa 50 điểm dữ liệu
        let chartData = {
          labels: [],
          temperature: [],
          humidity: [],
          soil: []
        };
        
        // Load saved refresh rate from localStorage
        function loadRefreshRate() {
          const saved = localStorage.getItem('sensorRefreshRate');
          if (saved && !isNaN(saved)) {
            currentRefreshRate = parseInt(saved);
            document.getElementById('refresh-input').value = currentRefreshRate;
            updateRefreshDisplay();
          }
        }
        
        function updateRefreshDisplay() {
          document.getElementById('refresh-current').textContent = currentRefreshRate + 's';
        }
        
        function updateRefreshRate() {
          const input = document.getElementById('refresh-input');
          const newRate = parseInt(input.value);
          
          if (isNaN(newRate) || newRate < 1 || newRate > 60) {
            alert('Please enter a valid refresh rate between 1 and 60 seconds.');
            input.value = currentRefreshRate;
            return;
          }
          
          currentRefreshRate = newRate;
          localStorage.setItem('sensorRefreshRate', currentRefreshRate.toString());
          updateRefreshDisplay();
          
          // Clear existing intervals
          if (sensorUpdateInterval) clearInterval(sensorUpdateInterval);
          if (pumpUpdateInterval) clearInterval(pumpUpdateInterval);
          if (dataSourceUpdateInterval) clearInterval(dataSourceUpdateInterval);
          if (dataSourceUpdateInterval) clearInterval(dataSourceUpdateInterval);
          
          // Start new intervals with updated rate
          startDataUpdates();
        }
        
        function setPresetRefresh(seconds) {
          document.getElementById('refresh-input').value = seconds;
          updateRefreshRate();
        }
        
        function startDataUpdates() {
          const intervalMs = currentRefreshRate * 1000;
          
          // Update sensor data
          sensorUpdateInterval = setInterval(() => {
            // Determine if we're in AP mode
            const isAPMode = window.location.hostname === '192.168.4.1' || window.location.hostname.includes('192.168.4');
            const endpoint = isAPMode ? '/api/sensor-data' : '/sensors';
            
            fetch(endpoint)
             .then(res => res.json())
             .then(d => {
               // Handle different data formats between AP mode and STA mode
               let temp, hum, soil, score, message, source;
               
               if (isAPMode) {
                 // AP mode: direct sensor data
                 temp = d.temperature !== undefined ? d.temperature.toFixed(2) : '--';
                 hum = d.humidity !== undefined ? d.humidity.toFixed(2) : '--';
                 soil = d.soil !== undefined ? d.soil.toFixed(1) : '--';
                 score = d.anomaly_score !== undefined ? d.anomaly_score.toFixed(4) : '--';
                 message = d.anomaly_message || 'Normal';
                 source = 'local';
               } else {
                 // STA mode: existing format
                 temp = d.temp;
                 hum = d.hum;
                 soil = d.soil;
                 score = d.score;
                 message = d.message;
                 source = d.source;
               }
               const isLoading = (source === 'loading');
               
               // Cập nhật giá trị và đơn vị
               const tempEl = document.getElementById('temp');
               const humEl = document.getElementById('hum');
               const soilEl = document.getElementById('soil');
               
               tempEl.innerText = temp;
               humEl.innerText = hum;
               soilEl.innerText = (soil === '--') ? '--' : formatSoil(soil);
               
               // Cập nhật đơn vị
               const tempUnit = tempEl.parentElement.querySelector('.unit');
               const humUnit = humEl.parentElement.querySelector('.unit');
               const soilUnit = soilEl.parentElement.querySelector('.unit');
               
               if (tempUnit) {
                 tempUnit.innerHTML = (temp === '--') ? '' : '°C';
               }
               if (humUnit) {
                 humUnit.innerHTML = (hum === '--') ? '' : '%';
               }
               if (soilUnit) {
                 soilUnit.innerHTML = (soil === '--') ? '' : '%';
               }
               
               document.getElementById('score').innerText = score;
               document.getElementById('message').innerText = message;
               
               // Cập nhật chart nếu có dữ liệu hợp lệ
               if (!isLoading && temp !== '--' && hum !== '--' && soil !== '--') {
                 updateChart(parseFloat(temp), parseFloat(hum), parseFloat(soil));
               }
               
               // Debug log for AP mode
               if (isAPMode && temp !== '--') {
                 console.log('AP Mode - Updated:', {temp, hum, soil, score, message});
               }
               
               // Thêm/xóa class loading
               const elements = ['temp', 'hum', 'soil', 'score', 'message'];
               elements.forEach(id => {
                 const el = document.getElementById(id);
                 if (isLoading) {
                   el.classList.add('loading');
                 } else {
                   el.classList.remove('loading');
                 }
               });
             })
             .catch(err => console.error('Error fetching sensor data:', err));
          }, intervalMs);
          
          // Update data source indicator
          dataSourceUpdateInterval = setInterval(() => {
            fetch('/data-source')
             .then(res => res.json())
             .then(d => {
               const dot = document.getElementById('source-dot');
               const text = document.getElementById('source-text');
               
               dot.className = 'source-dot ' + (d.source === 'CoreIOT' ? 'coreiot' : 'local');
               text.textContent = d.source;
             })
             .catch(err => console.error('Error fetching data source:', err));
          }, 10000); // Cập nhật mỗi 10 giây
          
          // Update pump status (keep at 1s for responsiveness)
          pumpUpdateInterval = setInterval(() => {
            fetch('/pumpstatus')
             .then(res => res.json())
             .then(d => {
               updatePumpStatus('pump-state-status', d.state);
               updatePumpMode('pump-mode-status', d.mode);
             })
             .catch(err => console.error('Error fetching pump status:', err));
          }, 1000);
        }
        
        function controlLED(id, action, color) {
          let url = '/toggle?led=' + id + '&action=' + action;
          
          if (color) {
            url += '&color=' + encodeURIComponent(color);
          }
          
          fetch(url)
          .then(response => response.json())
          .then(json => {
            updateStatus('led1-status', json.led1);
            updateStatus('led2-status', json.led2);
          });
        }
        
        function setColor(color) {
          currentColor = color;
          const led2Status = document.getElementById('led2-status').textContent.trim();
          if (led2Status === 'ON') {
            controlLED(2, 'on', color);
          }
        }
        
        function setCustomColor() {
          const colorInput = document.getElementById('customColor');
          currentColor = colorInput.value;
          const led2Status = document.getElementById('led2-status').textContent.trim();
          if (led2Status === 'ON') {
            controlLED(2, 'on', currentColor);
          }
        }
        
        function updateStatus(elementId, state) {
          const el = document.getElementById(elementId);
          const isOn = state.toUpperCase() === 'ON';
          el.textContent = isOn ? 'ON' : 'OFF';
          el.classList.remove('on', 'off');
          el.classList.add(isOn ? 'on' : 'off');
        }
        
        function updateTime() {
          const now = new Date();
          const hours = String(now.getHours()).padStart(2, '0');
          const minutes = String(now.getMinutes()).padStart(2, '0');
          const seconds = String(now.getSeconds()).padStart(2, '0');
          document.getElementById('time').innerText = hours + ':' + minutes + ':' + seconds;
        }
        
        updateStatus('led1-status', ')rawliteral" + led1 + R"rawliteral(');
        updateStatus('led2-status', ')rawliteral" + led2 + R"rawliteral(');
        updateTime();
        setInterval(updateTime, 1000);
        
        // Initialize Chart
        function initChart() {
          const ctx = document.getElementById('sensorChart').getContext('2d');
          
          sensorChart = new Chart(ctx, {
            type: 'line',
            data: {
              labels: chartData.labels,
              datasets: [
                {
                  label: 'Temperature (°C)',
                  data: chartData.temperature,
                  borderColor: 'rgba(251, 191, 36, 0.95)',
                  backgroundColor: 'rgba(251, 191, 36, 0.1)',
                  tension: 0.4,
                  fill: false,
                  borderWidth: 2
                },
                {
                  label: 'Humidity (%)',
                  data: chartData.humidity,
                  borderColor: 'rgba(125, 211, 252, 0.95)',
                  backgroundColor: 'rgba(125, 211, 252, 0.1)',
                  tension: 0.4,
                  fill: false,
                  borderWidth: 2
                },
                {
                  label: 'Soil Moisture (%)',
                  data: chartData.soil,
                  borderColor: 'rgba(74, 222, 128, 0.95)',
                  backgroundColor: 'rgba(74, 222, 128, 0.1)',
                  tension: 0.4,
                  fill: false,
                  borderWidth: 2
                }
              ]
            },
            options: {
              responsive: true,
              maintainAspectRatio: false,
              plugins: {
                legend: {
                  labels: {
                    color: '#e5e7eb',
                    font: {
                      size: 12,
                      weight: '600'
                    }
                  },
                  position: 'top',
                },
                tooltip: {
                  backgroundColor: 'rgba(17, 24, 39, 0.9)',
                  titleColor: '#e5e7eb',
                  bodyColor: '#e5e7eb',
                  borderColor: 'rgba(255, 255, 255, 0.1)',
                  borderWidth: 1,
                  padding: 12,
                  displayColors: true
                }
              },
              scales: {
                x: {
                  ticks: {
                    color: '#9ca3af',
                    maxRotation: 45,
                    minRotation: 0
                  },
                  grid: {
                    color: 'rgba(255, 255, 255, 0.05)'
                  }
                },
                y: {
                  ticks: {
                    color: '#9ca3af'
                  },
                  grid: {
                    color: 'rgba(255, 255, 255, 0.05)'
                  }
                }
              },
              interaction: {
                intersect: false,
                mode: 'index'
              }
            }
          });
        }
        
        // Update chart with new data
        function updateChart(temp, hum, soil) {
          if (!sensorChart) return;
          
          const now = new Date();
          const timeLabel = now.getHours().toString().padStart(2, '0') + ':' + 
                           now.getMinutes().toString().padStart(2, '0') + ':' + 
                           now.getSeconds().toString().padStart(2, '0');
          
          // Thêm dữ liệu mới
          chartData.labels.push(timeLabel);
          chartData.temperature.push(parseFloat(temp));
          chartData.humidity.push(parseFloat(hum));
          chartData.soil.push(parseFloat(soil));
          
          // Giữ chỉ tối đa maxDataPoints điểm
          if (chartData.labels.length > maxDataPoints) {
            chartData.labels.shift();
            chartData.temperature.shift();
            chartData.humidity.shift();
            chartData.soil.shift();
          }
          
          // Cập nhật chart
          sensorChart.update('none'); // 'none' mode để update không có animation
        }
        
        function togglePump() {
          const statusEl = document.getElementById('pump-state-status');
          const modeEl = document.getElementById('pump-mode-status');
          const currentState = statusEl.textContent.trim();
          const currentMode = modeEl.textContent.trim();
          const newState = currentState === 'ON' ? 'OFF' : 'ON';
          
          // Cập nhật UI ngay lập tức (zero delay)
          updatePumpStatus('pump-state-status', newState);
          if (currentMode === 'AUTO') {
            updatePumpMode('pump-mode-status', 'MANUAL');
          }
          
          // Gửi request đến server (không blocking - chạy async)
          fetch('/pump?action=toggle')
          .then(response => response.json())
          .then(json => {
            updatePumpStatus('pump-state-status', json.state);
            updatePumpMode('pump-mode-status', json.mode);
          })
          .catch(error => {
            updatePumpStatus('pump-state-status', currentState);
            updatePumpMode('pump-mode-status', currentMode);
            console.error('Error toggling pump:', error);
          });
        }
        
        function togglePumpMode() {
          const modeEl = document.getElementById('pump-mode-status');
          const currentMode = modeEl.textContent.trim();
          const newMode = currentMode === 'AUTO' ? 'MANUAL' : 'AUTO';
          
          // Cập nhật UI ngay lập tức (zero delay)
          updatePumpMode('pump-mode-status', newMode);
          
          // Nếu chuyển sang AUTO, cần cập nhật pump state từ server
          if (newMode === 'AUTO') {
            // Fetch pump status để cập nhật state mới từ AUTO logic
            setTimeout(() => {
              fetch('/pumpstatus')
                .then(res => res.json())
                .then(d => {
                  updatePumpStatus('pump-state-status', d.state);
                })
                .catch(err => console.error('Error fetching pump status:', err));
            }, 200);
          }
          
          // Gửi request đến server (không blocking - chạy async)
          fetch('/pump?action=toggleMode')
          .then(response => response.json())
          .then(json => {
            updatePumpMode('pump-mode-status', json.mode);
            updatePumpStatus('pump-state-status', json.state);
          })
          .catch(error => {
            updatePumpMode('pump-mode-status', currentMode);
            console.error('Error toggling pump mode:', error);
          });
        }
        
        function updatePumpStatus(elementId, state) {
          const el = document.getElementById(elementId);
          if (!el) return;
          const isOn = state.toUpperCase() === 'ON';
          el.textContent = isOn ? 'ON' : 'OFF';
          el.classList.remove('on', 'off');
          el.classList.add(isOn ? 'on' : 'off');
        }
        
        function updatePumpMode(elementId, mode) {
          const el = document.getElementById(elementId);
          if (!el) return;
          const isAuto = mode.toUpperCase() === 'AUTO';
          el.textContent = isAuto ? 'AUTO' : 'MANUAL';
          el.classList.remove('auto', 'manual');
          el.classList.add(isAuto ? 'auto' : 'manual');
          
          // Hiển thị/ẩn pump threshold control dựa trên mode
          const thresholdControl = document.getElementById('pump-threshold-control');
          if (thresholdControl) {
            thresholdControl.style.display = isAuto ? 'block' : 'none';
          }
        }
        
        function updatePumpThresholds() {
          const minInput = document.getElementById('threshold-min-input');
          const maxInput = document.getElementById('threshold-max-input');
          const min = parseFloat(minInput.value);
          const max = parseFloat(maxInput.value);
          
          if (isNaN(min) || isNaN(max)) {
            alert('Please enter valid numbers for min and max thresholds.');
            return;
          }
          
          if (min < 0 || max > 100 || min >= max) {
            alert('Invalid threshold values. Min must be < Max, and both must be between 0-100.');
            return;
          }
          
          fetch('/pumpthresholds?action=set&min=' + encodeURIComponent(min) + '&max=' + encodeURIComponent(max))
            .then(res => res.json())
            .then(data => {
              if (data.success) {
                document.getElementById('threshold-current').textContent = 'Min: ' + min + '% | Max: ' + max + '%';
                // Thông báo đã được cập nhật thành công (không hiển thị alert)
              } else {
                alert('Error: ' + (data.error || 'Failed to update thresholds'));
              }
            })
            .catch(err => {
              console.error('Error updating pump thresholds:', err);
              alert('Failed to update pump thresholds');
            });
        }
        
        function loadPumpThresholds() {
          fetch('/pumpthresholds?action=get')
            .then(res => res.json())
            .then(data => {
              if (data.min !== undefined && data.max !== undefined) {
                document.getElementById('threshold-min-input').value = data.min;
                document.getElementById('threshold-max-input').value = data.max;
                document.getElementById('threshold-current').textContent = 'Min: ' + data.min + '% | Max: ' + data.max + '%';
              }
            })
            .catch(err => console.error('Error loading pump thresholds:', err));
        }
        
        updatePumpStatus('pump-state-status', ')rawliteral" + pumpState + R"rawliteral(');
        updatePumpMode('pump-mode-status', ')rawliteral" + pumpMode + R"rawliteral(');
        
        // Load pump thresholds on page load
        loadPumpThresholds();
        
        function formatSoil(soilValue) {
          const soilInt = Math.floor(parseFloat(soilValue));
          return (soilInt < 10 ? '0' : '') + soilInt;
        }
        
        // Initialize refresh rate and start updates
        loadRefreshRate();
        initChart();
        startDataUpdates();
        
        // Update data source indicator immediately
        fetch('/data-source')
         .then(res => res.json())
         .then(d => {
           const dot = document.getElementById('source-dot');
           const text = document.getElementById('source-text');
           
           dot.className = 'source-dot ' + (d.source === 'CoreIOT' ? 'coreiot' : 'local');
           text.textContent = d.source;
         })
         .catch(err => console.error('Error fetching initial data source:', err));
        
        function toggleSidebar() {
          const sidebar = document.getElementById('sidebar');
          sidebar.classList.toggle('open');
        }
        
        // Close sidebar when clicking outside on mobile
        document.addEventListener('click', function(e) {
          const sidebar = document.getElementById('sidebar');
          const toggle = document.querySelector('.mobile-nav-toggle');
          if (window.innerWidth <= 768 && !sidebar.contains(e.target) && !toggle.contains(e.target)) {
            sidebar.classList.remove('open');
          }
        });
      </script>
    </body></html>
  )rawliteral";
}

String settingsPage() {
  
  String currentMode = isAPMode ? "AP Mode" : "STA Mode";
  String currentIP = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  
  // Load current CoreIOT settings
  loadCoreIOTCredentials();
  String currentServer = coreiot_server;
  String currentToken = coreiot_token;
  String currentClientId = coreiot_client_id;
  String currentUsername = coreiot_username;
  bool currentUseToken = coreiot_use_token;
  
  return R"rawliteral(
    <!DOCTYPE html><html><head>
      <meta charset='utf-8'>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>Settings</title>
      <style>
        :root{
          --bg:#0f172a;
          --card:#111827;
          --muted:#9ca3af;
          --text:#e5e7eb;
          --primary:#3b82f6;
          --shadow:0 10px 30px rgba(0,0,0,0.35);
          --radius:16px;
        }
        *{box-sizing:border-box}
        body{
          margin:0;
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, "Noto Sans", "Apple Color Emoji", "Segoe UI Emoji";
          color:var(--text);
          background:
            radial-gradient(1200px 600px at -10% -10%, rgba(59,130,246,.15), transparent 40%),
            radial-gradient(800px 400px at 120% 10%, rgba(34,211,238,.12), transparent 40%),
            linear-gradient(180deg, #0b1220, #0f172a 30%, #0b1220 100%);
          min-height:100vh;
          display:flex;
          padding:0;
        }
        .app-layout {
          display:flex;
          width:100%;
          min-height:100vh;
        }
        .sidebar {
          width:280px;
          background: linear-gradient(180deg, rgba(255,255,255,0.06), rgba(255,255,255,0.03)), var(--card);
          border-right:1px solid rgba(255,255,255,0.08);
          backdrop-filter: blur(12px);
          display:flex;
          flex-direction:column;
          position:fixed;
          height:100vh;
          z-index:100;
          overflow-y:auto;
        }
        .sidebar-header {
          padding:24px 20px 16px;
          border-bottom:1px solid rgba(255,255,255,0.06);
        }
        .sidebar-brand {
          display:flex;
          align-items:center;
          gap:12px;
          font-weight:800;
          font-size:18px;
          letter-spacing:.3px;
        }
        .sidebar-brand .logo {
          width:36px;height:36px;border-radius:10px;
          background: conic-gradient(from 180deg at 50% 50%, var(--primary), var(--accent), var(--primary));
          filter: drop-shadow(0 6px 14px rgba(59,130,246,.45));
        }
        .sidebar-nav {
          flex:1;
          padding:20px 0;
        }
        .nav-item {
          margin:0 16px 8px;
        }
        .nav-link {
          display:flex;
          align-items:center;
          gap:14px;
          padding:14px 16px;
          border-radius:12px;
          color:var(--muted);
          text-decoration:none;
          font-weight:600;
          font-size:15px;
          transition: all .2s ease;
          border:1px solid transparent;
        }
        .nav-link:hover {
          background: rgba(255,255,255,0.05);
          color:var(--text);
          border-color: rgba(255,255,255,0.1);
        }
        .nav-link.active {
          background: linear-gradient(135deg, rgba(59,130,246,0.2), rgba(59,130,246,0.1));
          color:#93c5fd;
          border-color: rgba(59,130,246,0.3);
        }
        .nav-icon {
          width:20px;
          height:20px;
          flex-shrink:0;
        }
        .nav-icon svg { width:100%; height:100%; stroke-width:1.8; stroke-linecap:round; stroke-linejoin:round; stroke:currentColor; fill:none; }
        .main-content {
          flex:1;
          margin-left:280px;
          padding:24px;
          overflow-y:auto;
        }
        .container{
          width:100%;
          max-width:900px;
          background: linear-gradient(180deg, rgba(255,255,255,0.04), rgba(255,255,255,0.02)) , var(--card);
          border:1px solid rgba(255,255,255,0.08);
          border-radius: var(--radius);
          box-shadow: var(--shadow);
          padding: 22px;
          backdrop-filter: blur(8px);
        }
        .mobile-nav-toggle {
          display:none;
          position:fixed;
          top:20px;
          left:20px;
          z-index:200;
          background: var(--card);
          border:1px solid rgba(255,255,255,0.1);
          border-radius:10px;
          padding:12px;
          cursor:pointer;
          backdrop-filter: blur(8px);
        }
        .mobile-nav-toggle svg {
          width:24px;
          height:24px;
          color:var(--text);
        }
        @media (max-width: 768px) {
          .sidebar {
            transform: translateX(-100%);
            transition: transform .3s ease;
          }
          .sidebar.open {
            transform: translateX(0);
          }
          .main-content {
            margin-left:0;
            padding:80px 16px 16px;
          }
          .mobile-nav-toggle {
            display:block;
          }
        }
        h2{ margin:6px 0 14px; }
        
        .status-banner{
          background: linear-gradient(135deg, rgba(34,197,94,0.15), rgba(16,185,129,0.10));
          border:1px solid rgba(34,197,94,0.3);
          border-radius:12px;
          padding:12px 16px;
          margin-bottom:16px;
          display:flex;
          align-items:center;
          justify-content:space-between;
        }
        .status-info{
          display:flex;
          flex-direction:column;
          gap:4px;
        }
        .status-mode{
          font-weight:700;
          color:#86efac;
          font-size:14px;
        }
        .status-ip{
          font-size:12px;
          color:var(--muted);
          font-family:monospace;
        }
        .status-icon{
          width:24px;
          height:24px;
          color:#86efac;
        }
        
        form{
          display:grid;
          gap:12px;
        }
        .form-label{
          font-size:13px;
          color:var(--muted);
          margin-bottom:6px;
          font-weight:600;
          display:flex;
          align-items:center;
          gap:6px;
        }
        .form-label svg{
          width:14px;
          height:14px;
        }
        .input{
          background: transparent;
          border:0;
          color:var(--text);
          width:100%;
          font-size:15px;
          outline:none;
          padding:0;
        }
        .input::placeholder{
          color:rgba(156,163,175,0.5);
        }
        .field,
        .password-field{
          background: rgba(255,255,255,.04);
          border:1px solid rgba(255,255,255,.10);
          border-radius:12px;
          padding:12px 14px;
          transition: border-color .2s ease, background .2s ease;
        }
        .field:focus-within,
        .password-field:focus-within{
          border-color: rgba(59,130,246,0.5);
          background: rgba(255,255,255,.06);
        }
        .password-field{ display:flex; align-items:center; gap:8px; }
        .input-group{
          display:grid;
          gap:6px;
        }
        .toggle-pass{
          appearance:none;
          border:0;
          background: rgba(255,255,255,0.08);
          color:var(--muted);
          padding:6px 12px;
          border-radius:999px;
          font-size:12px;
          font-weight:600;
          cursor:pointer;
          transition: background .2s ease,color .2s ease;
        }
        .toggle-pass:hover{
          background: rgba(59,130,246,0.25);
          color:#dbeafe;
        }
        .row{
          display:flex;
          gap:10px;
        }
        .primary{
          appearance:none;border:0;cursor:pointer;font-weight:700;
          color:white;
          background: linear-gradient(180deg, rgba(59,130,246,.25), rgba(59,130,246,.15));
          border:1px solid rgba(59,130,246,.45);
          padding:10px 14px;border-radius:12px;
          flex:1;
        }
        .ghost{
          appearance:none;border:1px solid rgba(255,255,255,.15);cursor:pointer;font-weight:700;
          color:var(--text);
          background: transparent;
          padding:10px 14px;border-radius:12px;
          flex:1;
          transition: all .2s ease;
        }
        .primary:hover{
          background: linear-gradient(180deg, rgba(59,130,246,.35), rgba(59,130,246,.25));
          transform: translateY(-1px);
          box-shadow: 0 6px 18px rgba(59,130,246,.3);
        }
        .ghost:hover{
          background: rgba(255,255,255,0.05);
          border-color: rgba(255,255,255,0.25);
        }
        .primary:disabled{
          opacity: 0.5;
          cursor: not-allowed;
          transform: none;
        }
        
        #msg{ 
          margin-top:12px; 
          padding:12px 14px;
          border-radius:10px;
          font-size:13px;
          font-weight:600;
          display:none;
        }
        #msg.info{
          display:block;
          background: rgba(59,130,246,0.15);
          border:1px solid rgba(59,130,246,0.3);
          color:#93c5fd;
        }
        #msg.success{
          display:block;
          background: rgba(34,197,94,0.15);
          border:1px solid rgba(34,197,94,0.3);
          color:#86efac;
        }
        #msg.error{
          display:block;
          background: rgba(239,68,68,0.15);
          border:1px solid rgba(239,68,68,0.3);
          color:#fca5a5;
        }
        
        .spinner{
          display:inline-block;
          width:16px;
          height:16px;
          border:2px solid rgba(255,255,255,0.2);
          border-top-color:#93c5fd;
          border-radius:50%;
          animation:spin 0.8s linear infinite;
          vertical-align:middle;
          margin-right:8px;
        }
        @keyframes spin{
          to{ transform:rotate(360deg); }
        }
        
        .info-box{
          background: rgba(59,130,246,0.08);
          border:1px solid rgba(59,130,246,0.2);
          border-radius:12px;
          padding:12px 14px;
          margin-top:14px;
          font-size:13px;
          color:var(--muted);
          line-height:1.5;
        }
        .info-box strong{
          color:#93c5fd;
          font-weight:600;
        }
        .info-box code{
          background: rgba(255,255,255,0.08);
          padding:2px 6px;
          border-radius:4px;
          font-family:monospace;
          font-size:12px;
          color:#dbeafe;
        }
        .settings-section {
          margin-top:24px;
          padding-top:24px;
          border-top:1px solid rgba(255,255,255,0.1);
        }
        .setting-item {
          display:flex;
          align-items:center;
          justify-content:space-between;
          padding:16px 0;
          border-bottom:1px solid rgba(255,255,255,0.06);
        }
        .setting-item:last-child {
          border-bottom:none;
        }
        .setting-info {
          flex:1;
        }
        .setting-title {
          font-weight:600;
          font-size:15px;
          color:var(--text);
          margin-bottom:4px;
        }
        .setting-desc {
          font-size:13px;
          color:var(--muted);
          line-height:1.4;
        }
        .setting-control {
          display:flex;
          align-items:center;
          gap:8px;
        }
        .setting-input {
          background: rgba(255,255,255,.04);
          border:1px solid rgba(255,255,255,.10);
          border-radius:8px;
          padding:8px 12px;
          color:var(--text);
          font-size:14px;
          font-weight:600;
          width:80px;
          text-align:center;
        }
        .setting-input:focus {
          outline:none;
          border-color: rgba(59,130,246,0.5);
          background: rgba(255,255,255,.06);
        }
        .setting-btn {
          appearance:none;
          border:0;
          cursor:pointer;
          font-weight:600;
          padding:8px 14px;
          border-radius:8px;
          transition: all 0.2s ease;
          background: linear-gradient(180deg, rgba(59,130,246,.25), rgba(59,130,246,.15));
          color:#93c5fd;
          border:1px solid rgba(59,130,246,.35);
          font-size:13px;
        }
        .setting-btn:hover {
          background: linear-gradient(180deg, rgba(59,130,246,.35), rgba(59,130,246,.25));
          transform: translateY(-1px);
        }
        .setting-current {
          padding:6px 12px;
          border-radius:999px;
          font-size:12px;
          font-weight:700;
          letter-spacing:.4px;
          background: rgba(34,197,94,.15);
          color:#86efac;
          border:1px solid rgba(34,197,94,.35);
          margin-right:8px;
        }
      </style>
    </head>
    <body>
      <div class="app-layout">
        <div class="mobile-nav-toggle" onclick="toggleSidebar()">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <line x1="3" y1="6" x2="21" y2="6"></line>
            <line x1="3" y1="12" x2="21" y2="12"></line>
            <line x1="3" y1="18" x2="21" y2="18"></line>
          </svg>
        </div>
        
        <div class="sidebar" id="sidebar">
          <div class="sidebar-header">
            <div class="sidebar-brand">
              <div class="logo"></div>
              <span>ESP32 IoT</span>
            </div>
          </div>
          
          <nav class="sidebar-nav">
            <div class="nav-item">
              <a href="/" class="nav-link">
                <div class="nav-icon">
                  <svg viewBox="0 0 24 24">
                    <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"></path>
                    <polyline points="9,22 9,12 15,12 15,22"></polyline>
                  </svg>
                </div>
                Dashboard
              </a>
            </div>
            
            <div class="nav-item">
              <a href="/settings" class="nav-link active">
                <div class="nav-icon">
                  <svg viewBox="0 0 24 24">
                    <circle cx="12" cy="12" r="3"></circle>
                    <path d="M12 1v6m0 12v6m11-7h-6m-12 0H1m15.5-6.5l-4.24 4.24M6.74 6.74L2.5 2.5m15 15l-4.24-4.24M6.74 17.26L2.5 21.5"></path>
                  </svg>
                </div>
                Settings
              </a>
            </div>
          </nav>
        </div>
        
        <div class="main-content">
          <div class='container'>
            <div class="header" style="display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;">
              <div class="title" style="font-weight:700;font-size:20px;">Settings</div>
            </div>
        
        <div class="status-banner">
          <div class="status-info">
            <div class="status-mode">)rawliteral" + currentMode + R"rawliteral(</div>
            <div class="status-ip">IP: )rawliteral" + currentIP + R"rawliteral(</div>
          </div>
          <svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <path d="M5 12.55a11 11 0 0 1 14.08 0"></path>
            <path d="M1.42 9a16 16 0 0 1 21.16 0"></path>
            <path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path>
            <line x1="12" y1="20" x2="12.01" y2="20"></line>
          </svg>
        </div>
        
        <form id="wifiForm">
          <div class="input-group">
            <div class="form-label">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M5 12.55a11 11 0 0 1 14.08 0"></path>
                <path d="M1.42 9a16 16 0 0 1 21.16 0"></path>
                <path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path>
                <line x1="12" y1="20" x2="12.01" y2="20"></line>
              </svg>
              WiFi Network
            </div>
            <div class="field">
              <input class="input" name="ssid" id="ssid" placeholder="Enter SSID" required>
            </div>
          </div>
          
          <div class="input-group">
            <div class="form-label">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
                <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
              </svg>
              Password
            </div>
            <div class="password-field">
              <input class="input" name="password" id="pass" type="password" placeholder="Enter password" required>
              <button id="togglePass" class="toggle-pass" type="button" aria-pressed="false">Show</button>
            </div>
          </div>
          

          
          <div class="row">
            <button class="primary" type="submit" id="submitBtn">Connect</button>
            <button class="ghost" type="button" onclick="window.location='/'">Back to Dashboard</button>
          </div>
        </form>
        
        <div id="msg"></div>
        
        <!-- CoreIOT Configuration Section -->
        <div style="margin-top:24px; padding-top:24px; border-top:1px solid rgba(255,255,255,0.1);">
          <h2 style="margin-bottom:16px; font-size:18px; font-weight:700;">CoreIOT Configuration</h2>
          <form id="coreiotForm">
            <div class="input-group">
              <div class="form-label">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path>
                  <polyline points="7.5 4.21 12 6.81 16.5 4.21"></polyline>
                  <polyline points="7.5 19.79 7.5 14.6 3 12"></polyline>
                  <polyline points="21 12 16.5 14.6 16.5 19.79"></polyline>
                  <polyline points="3.27 6.96 12 12.01 20.73 6.96"></polyline>
                  <line x1="12" y1="22.08" x2="12" y2="12"></line>
                </svg>
                Server
              </div>
              <div class="field">
                <input class="input" name="server" id="coreiot_server" placeholder="app.coreiot.io" value=")rawliteral" + currentServer + R"rawliteral(" required>
              </div>
            </div>
            
            <div class="input-group">
              <div class="form-label">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
                  <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
                </svg>
                Authentication Method
              </div>
              <div style="display:flex; gap:12px; margin-bottom:12px;">
                <label style="display:flex; align-items:center; gap:6px; cursor:pointer;">
                  <input type="radio" name="auth_type" value="token" id="auth_token" )rawliteral" + (currentUseToken ? "checked" : "") + R"rawliteral( style="cursor:pointer;">
                  <span>Token</span>
                </label>
                <label style="display:flex; align-items:center; gap:6px; cursor:pointer;">
                  <input type="radio" name="auth_type" value="userpass" id="auth_userpass" )rawliteral" + (!currentUseToken ? "checked" : "") + R"rawliteral( style="cursor:pointer;">
                  <span>Username/Password</span>
                </label>
              </div>
            </div>
            
            <div class="input-group" id="token-group">
              <div class="form-label">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path>
                </svg>
                Access Token
              </div>
              <div class="password-field">
                <input class="input" name="token" id="coreiot_token" type="password" placeholder="Enter access token" value=")rawliteral" + currentToken + R"rawliteral(">
                <button id="toggleToken" class="toggle-pass" type="button">Show</button>
              </div>
            </div>
            
            <div class="input-group" id="clientid-group" style="display:none;">
              <div class="form-label">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect>
                  <line x1="9" y1="9" x2="15" y2="9"></line>
                  <line x1="9" y1="15" x2="15" y2="15"></line>
                </svg>
                Client ID
              </div>
              <div class="field">
                <input class="input" name="client_id" id="coreiot_client_id" placeholder="ESP32Client" value=")rawliteral" + currentClientId + R"rawliteral(">
              </div>
            </div>
            
            <div class="input-group" id="username-group" style="display:none;">
              <div class="form-label">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path>
                  <circle cx="12" cy="7" r="4"></circle>
                </svg>
                Username
              </div>
              <div class="field">
                <input class="input" name="username" id="coreiot_username" placeholder="Enter username" value=")rawliteral" + currentUsername + R"rawliteral(">
              </div>
            </div>
            
            <div class="input-group" id="password-group" style="display:none;">
              <div class="form-label">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
                  <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
                </svg>
                Password
              </div>
              <div class="password-field">
                <input class="input" name="password" id="coreiot_password" type="password" placeholder="Enter password">
                <button id="toggleCoreIOTPass" class="toggle-pass" type="button">Show</button>
              </div>
            </div>
            
            <div class="row">
              <button class="primary" type="submit" id="coreiotSubmitBtn">Save CoreIOT Config</button>
            </div>
          </form>
          
          <div id="coreiotMsg"></div>
        </div>
        
        <div class="info-box">
          <strong>Note:</strong> After connecting to a new WiFi network, the device will switch to <code>STA Mode</code>. 
          If connection fails, it will automatically return to <code>AP Mode</code>.<br><br>
          <strong>CoreIOT:</strong> Save your credentials to enable MQTT connection to CoreIOT platform. Device will reconnect automatically after saving.
        </div>
          </div>
        </div>
      </div>
      
      <script>
        const msgBox = document.getElementById('msg');
        const submitBtn = document.getElementById('submitBtn');
        
        document.getElementById('wifiForm').onsubmit = function(e){
          e.preventDefault();
          
          const ssid = document.getElementById('ssid').value;
          const pass = document.getElementById('pass').value;
          
          submitBtn.disabled = true;
          submitBtn.innerHTML = '<span class="spinner"></span>Connecting...';
          
          msgBox.className = 'info';
          msgBox.innerHTML = '<span class="spinner"></span>Attempting to connect to <strong>' + ssid + '</strong>...';
          
          fetch('/connect?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass))
            .then(r => r.text())
            .then(msg => {
              // Bắt đầu polling để check status
              let checkCount = 0;
              const maxChecks = 20; // 20 lần x 500ms = 10 giây
              
              const checkStatus = setInterval(() => {
                checkCount++;
                
                fetch('/status')
                  .then(r => r.json())
                  .then(data => {
                    if (data.status === 'connected') {
                      clearInterval(checkStatus);
                      msgBox.className = 'success';
                      msgBox.innerHTML = '✓ Connected successfully!<br><br>' +
                        '<strong>New IP:</strong> <code>' + data.ip + '</code><br>' +
                        '<strong>mDNS:</strong> <code>http://' + data.mdns + '</code><br><br>' +
                        '<em>Please connect to the new WiFi network: <strong>' + ssid + '</strong></em><br>' +
                        'Then access: <a href="http://' + data.ip + '" style="color:#86efac">http://' + data.ip + '</a> or ' +
                        '<a href="http://' + data.mdns + '" style="color:#86efac">http://' + data.mdns + '</a>';
                      
                      submitBtn.disabled = false;
                      submitBtn.textContent = 'Connect';
                    } else if (data.status === 'failed' || checkCount >= maxChecks) {
                      clearInterval(checkStatus);
                      msgBox.className = 'error';
                      msgBox.innerHTML = '✗ Connection failed. Device returned to AP Mode.<br>' +
                        'IP: <code>' + data.ip + '</code>';
                      submitBtn.disabled = false;
                      submitBtn.textContent = 'Connect';
                    }
                  })
                  .catch(err => {
                    // Nếu không fetch được, có thể đã đổi mạng
                    if (checkCount >= 5) {
                      clearInterval(checkStatus);
                      msgBox.className = 'info';
                      msgBox.innerHTML = '⚠ Connection may be successful but device is now on different network.<br><br>' +
                        'Please connect your device to <strong>' + ssid + '</strong> and access:<br>' +
                        '• Check your router for ESP32 IP address<br>' +
                        '• Or try: <a href="http://esp32.local" style="color:#93c5fd">http://esp32.local</a>';
                      submitBtn.disabled = false;
                      submitBtn.textContent = 'Connect';
                    }
                  });
              }, 500);
            })
            .catch(err => {
              msgBox.className = 'error';
              msgBox.innerHTML = '✗ Connection request failed: ' + err.message;
              submitBtn.disabled = false;
              submitBtn.textContent = 'Connect';
            });
        };
        
        document.getElementById('togglePass').onclick = function(){
          const passInput = document.getElementById('pass');
          const isPassword = passInput.type === 'password';
          passInput.type = isPassword ? 'text' : 'password';
          this.textContent = isPassword ? 'Hide' : 'Show';
          this.setAttribute('aria-pressed', isPassword ? 'true' : 'false');
        };
        
        // CoreIOT form handling
        const coreiotMsgBox = document.getElementById('coreiotMsg');
        const coreiotSubmitBtn = document.getElementById('coreiotSubmitBtn');
        
        // Toggle between token and username/password fields
        document.querySelectorAll('input[name="auth_type"]').forEach(radio => {
          radio.onchange = function() {
            const isToken = document.getElementById('auth_token').checked;
            document.getElementById('token-group').style.display = isToken ? 'block' : 'none';
            document.getElementById('clientid-group').style.display = isToken ? 'none' : 'block';
            document.getElementById('username-group').style.display = isToken ? 'none' : 'block';
            document.getElementById('password-group').style.display = isToken ? 'none' : 'block';
            
            // Update required attributes
            document.getElementById('coreiot_token').required = isToken;
            document.getElementById('coreiot_client_id').required = !isToken;
            document.getElementById('coreiot_username').required = !isToken;
            document.getElementById('coreiot_password').required = !isToken;
          };
        });
        
        // Trigger on page load
        if (document.getElementById('auth_token').checked) {
          document.getElementById('token-group').style.display = 'block';
          document.getElementById('clientid-group').style.display = 'none';
          document.getElementById('coreiot_token').required = true;
        } else {
          document.getElementById('clientid-group').style.display = 'block';
          document.getElementById('username-group').style.display = 'block';
          document.getElementById('password-group').style.display = 'block';
          document.getElementById('coreiot_client_id').required = true;
          document.getElementById('coreiot_username').required = true;
          document.getElementById('coreiot_password').required = true;
        }
        
        document.getElementById('coreiotForm').onsubmit = function(e){
          e.preventDefault();
          
          const server = document.getElementById('coreiot_server').value;
          const authType = document.querySelector('input[name="auth_type"]:checked').value;
          const token = document.getElementById('coreiot_token').value;
          const clientId = document.getElementById('coreiot_client_id').value;
          const username = document.getElementById('coreiot_username').value;
          const password = document.getElementById('coreiot_password').value;
          
          if (!server) {
            coreiotMsgBox.className = 'error';
            coreiotMsgBox.innerHTML = '✗ Server is required';
            return;
          }
          
          if (authType === 'token' && !token) {
            coreiotMsgBox.className = 'error';
            coreiotMsgBox.innerHTML = '✗ Token is required when using token authentication';
            return;
          }
          
          if (authType === 'userpass') {
            if (!clientId) {
              coreiotMsgBox.className = 'error';
              coreiotMsgBox.innerHTML = '✗ Client ID is required when using username/password authentication';
              return;
            }
            if (!username || !password) {
              coreiotMsgBox.className = 'error';
              coreiotMsgBox.innerHTML = '✗ Username and password are required when using username/password authentication';
              return;
            }
          }
          
          coreiotSubmitBtn.disabled = true;
          coreiotSubmitBtn.innerHTML = '<span class="spinner"></span>Saving...';
          
          const formData = new URLSearchParams();
          formData.append('server', server);
          formData.append('auth_type', authType);
          formData.append('token', token);
          formData.append('client_id', clientId || 'ESP32Client');
          formData.append('username', username);
          formData.append('password', password);
          
          fetch('/coreiot', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: formData.toString()
          })
          .then(r => r.json())
          .then(data => {
            coreiotSubmitBtn.disabled = false;
            coreiotSubmitBtn.textContent = 'Save CoreIOT Config';
            
            if (data.success) {
              coreiotMsgBox.className = 'success';
              coreiotMsgBox.innerHTML = '✓ ' + data.message;
            } else {
              coreiotMsgBox.className = 'error';
              coreiotMsgBox.innerHTML = '✗ ' + (data.error || 'Failed to save configuration');
            }
          })
          .catch(err => {
            coreiotSubmitBtn.disabled = false;
            coreiotSubmitBtn.textContent = 'Save CoreIOT Config';
            coreiotMsgBox.className = 'error';
            coreiotMsgBox.innerHTML = '✗ Request failed: ' + err.message;
          });
        };
        
        document.getElementById('toggleToken').onclick = function(){
          const tokenInput = document.getElementById('coreiot_token');
          const isPassword = tokenInput.type === 'password';
          tokenInput.type = isPassword ? 'text' : 'password';
          this.textContent = isPassword ? 'Hide' : 'Show';
        };
        
        document.getElementById('toggleCoreIOTPass').onclick = function(){
          const passInput = document.getElementById('coreiot_password');
          const isPassword = passInput.type === 'password';
          passInput.type = isPassword ? 'text' : 'password';
          this.textContent = isPassword ? 'Hide' : 'Show';
        };
        
        function toggleSidebar() {
          const sidebar = document.getElementById('sidebar');
          sidebar.classList.toggle('open');
        }
        
        // Close sidebar when clicking outside on mobile
        document.addEventListener('click', function(e) {
          const sidebar = document.getElementById('sidebar');
          const toggle = document.querySelector('.mobile-nav-toggle');
          if (window.innerWidth <= 768 && !sidebar.contains(e.target) && !toggle.contains(e.target)) {
            sidebar.classList.remove('open');
          }
        });
      </script>
    </body></html>
  )rawliteral";
}

// Function để fetch dữ liệu từ CoreIOT
void fetchCoreIOTData() {
  if (isAPMode || !WiFi.isConnected()) {
    use_coreiot_data = false;
    coreiot_data.is_valid = false;
    Serial.println("[CoreIOT] Not connected to internet, using local data");
    return;
  }
  
  // Kiểm tra xem có credentials CoreIOT không
  loadCoreIOTCredentials();
  if (coreiot_server.length() == 0 || coreiot_token.length() == 0) {
    use_coreiot_data = false;
    coreiot_data.is_valid = false;
    Serial.println("[CoreIOT] No credentials configured, using local data");
    return;
  }

  WiFiClient client;
  HttpClient http = HttpClient(client, coreiot_server, 80);
  
  // Thử các endpoint khác nhau để lấy dữ liệu
  String endpoints[] = {
    "/api/v1/" + coreiot_token + "/attributes",
    "/api/v1/" + coreiot_token + "/telemetry/latest", 
    "/api/v1/" + coreiot_token + "/rpc"
  };
  
  for (int i = 0; i < 3; i++) {
    Serial.println("[CoreIOT] Trying endpoint " + String(i + 1) + ": http://" + coreiot_server + endpoints[i]);
    
    http.beginRequest();
    http.get(endpoints[i]);
    http.sendHeader("Accept", "application/json");
    http.endRequest();
    
    int httpResponseCode = http.responseStatusCode();
    String response = http.responseBody();
    
    Serial.println("[CoreIOT] Response code: " + String(httpResponseCode));
    if (response.length() > 0) {
      Serial.println("[CoreIOT] Response: " + response);
    }
    
    if (httpResponseCode == 200 && response.length() > 0) {
      // Parse JSON response
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (error) {
        Serial.println("[CoreIOT] JSON parse error: " + String(error.c_str()));
        continue;
      }
      
      // CoreIOT chỉ lưu trữ pump controls, không có dữ liệu sensor
      bool hasConnection = false;
      
      // Kiểm tra pump controls từ CoreIOT
      if (doc.containsKey("client")) {
        JsonObject client = doc["client"];
        if (client.containsKey("pump_state") || client.containsKey("pump_mode")) {
          hasConnection = true;
          Serial.println("[CoreIOT] Connected - pump controls available");
          
          // Có thể thêm logic xử lý pump control từ CoreIOT ở đây nếu cần
          if (client.containsKey("pump_state")) {
            Serial.println("[CoreIOT] Remote pump state: " + String(client["pump_state"].as<bool>() ? "ON" : "OFF"));
          }
          if (client.containsKey("pump_mode")) {
            Serial.println("[CoreIOT] Remote pump mode: " + client["pump_mode"].as<String>());
          }
        }
      }
      
      if (hasConnection) {
        // Kết nối CoreIOT thành công, nhưng dùng sensor data local
        Serial.println("[CoreIOT] Connected successfully, using local sensor data");
        use_coreiot_data = false;  // Vẫn dùng local data cho sensors
        coreiot_data.is_valid = false;  // Không có sensor data từ CoreIOT
        return;
      } else {
        Serial.println("[CoreIOT] No pump control data in response from endpoint " + String(i + 1));
      }
    } else {
      Serial.println("[CoreIOT] HTTP error " + String(httpResponseCode) + " for endpoint " + String(i + 1));
    }
  }
  
  // Nếu tất cả endpoints đều thất bại, fallback sang local data
  Serial.println("[CoreIOT] All endpoints failed or no data available");
  Serial.println("[CoreIOT] Using local sensor data (CoreIOT platform doesn't store sensor history)");
  use_coreiot_data = false;
  coreiot_data.is_valid = false;
}

// Function gửi dữ liệu lên CoreIOT
void sendDataToCoreIOT(String data) {
  if (isAPMode || !WiFi.isConnected()) {
    Serial.println("[CoreIOT] Not connected to internet, cannot send data");
    return;
  }
  
  loadCoreIOTCredentials();
  if (coreiot_server.length() == 0) {
    Serial.println("[CoreIOT] No server configured, cannot send data");
    return;
  }
  
  WiFiClient client;
  HttpClient http = HttpClient(client, coreiot_server, 80); // Try HTTP port 80 instead of HTTPS 443
  String path = "/api/v1/";
  
  if (coreiot_use_token && coreiot_token.length() > 0) {
    path += coreiot_token + "/telemetry";
  } else {
    Serial.println("[CoreIOT] No valid credentials for sending data");
    return;
  }
  
  Serial.println("[CoreIOT] Sending data to: http://" + coreiot_server + path);
  Serial.println("[CoreIOT] Data: " + data);
  
  http.beginRequest();
  http.post(path);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Content-Length", data.length());
  http.beginBody();
  http.print(data);
  http.endRequest();
  
  int httpResponseCode = http.responseStatusCode();
  
  if (httpResponseCode > 0) {
    String response = http.responseBody();
    Serial.println("[CoreIOT] Response code: " + String(httpResponseCode));
    Serial.println("[CoreIOT] Response: " + response);
  } else {
    Serial.println("[CoreIOT] HTTP error: " + String(httpResponseCode));
  }
}

// Handler để nhận dữ liệu từ CoreIOT (webhook hoặc push data)
void handleCoreIOTData() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }
  
  String payload = server.arg("plain");
  Serial.println("[CoreIOT] Received payload: " + payload);
  
  // Parse JSON data
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (!error) {
    // Cập nhật dữ liệu từ payload
    bool dataUpdated = false;
    
    if (doc.containsKey("temperature")) {
      coreiot_data.temperature = doc["temperature"];
      dataUpdated = true;
    }
    if (doc.containsKey("humidity")) {
      coreiot_data.humidity = doc["humidity"];
      dataUpdated = true;
    }
    if (doc.containsKey("soil")) {
      coreiot_data.soil = doc["soil"];
      dataUpdated = true;
    }
    if (doc.containsKey("anomaly_score")) {
      coreiot_data.anomaly_score = doc["anomaly_score"];
      dataUpdated = true;
    }
    if (doc.containsKey("anomaly_message")) {
      coreiot_data.anomaly_message = doc["anomaly_message"].as<String>();
      dataUpdated = true;
    }
    if (doc.containsKey("pump_state")) {
      coreiot_data.pump_state = doc["pump_state"];
      dataUpdated = true;
    }
    if (doc.containsKey("pump_mode")) {
      coreiot_data.pump_mode = doc["pump_mode"].as<String>();
      dataUpdated = true;
    }
    if (doc.containsKey("led1_state")) {
      coreiot_data.led1_state = doc["led1_state"];
      dataUpdated = true;
    }
    if (doc.containsKey("led2_state")) {
      coreiot_data.led2_state = doc["led2_state"];
      dataUpdated = true;
    }
    
    if (dataUpdated) {
      coreiot_data.last_update = millis();
      coreiot_data.is_valid = true;
      use_coreiot_data = true;
      
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Data received and updated\"}");
      Serial.println("[CoreIOT] Data received and updated from server");
    } else {
      server.send(400, "application/json", "{\"error\":\"No valid data fields found\"}");
      Serial.println("[CoreIOT] No valid data fields found in payload");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
    Serial.println("[CoreIOT] JSON parsing failed: " + String(error.c_str()));
  }
}

// ========== Handlers ==========
void handleRoot() { 
  server.send(200, "text/html", mainPage()); 
}

void handleToggle() {
  int led = server.arg("led").toInt();
  String action = server.arg("action");
  
  if (led == 1) {
    if (action == "on") {
      led1_state = true;
      digitalWrite(LED1_PIN, HIGH);
    } else {
      led1_state = false;
      digitalWrite(LED1_PIN, LOW);
    }
    web_led1_control_enabled = true;
    Serial.println("LED1 " + String(led1_state ? "ON" : "OFF"));
  }
  else if (led == 2) {
    if (action == "on") {
      led2_state = true;
      
      String color = server.arg("color");
      uint32_t rgbColor;
      
      if (color.startsWith("#")) {
        long hexValue = strtol(color.substring(1).c_str(), NULL, 16);
        uint8_t r = (hexValue >> 16) & 0xFF;
        uint8_t g = (hexValue >> 8) & 0xFF;
        uint8_t b = hexValue & 0xFF;
        rgbColor = neoPixel.Color(r, g, b);
        Serial.println("Custom color: R=" + String(r) + " G=" + String(g) + " B=" + String(b));
      } else {
        if (color == "red") rgbColor = neoPixel.Color(255, 0, 0);
        else if (color == "green") rgbColor = neoPixel.Color(0, 255, 0);
        else if (color == "blue") rgbColor = neoPixel.Color(0, 0, 255);
        else if (color == "yellow") rgbColor = neoPixel.Color(255, 255, 0);
        else if (color == "purple") rgbColor = neoPixel.Color(255, 0, 255);
        else if (color == "cyan") rgbColor = neoPixel.Color(0, 255, 255);
        else if (color == "white") rgbColor = neoPixel.Color(255, 255, 255);
        else rgbColor = neoPixel.Color(255, 255, 255);
        Serial.println("Preset color: " + color);
      }
      
      neoPixel.setPixelColor(0, rgbColor);
      neoPixel.show();
    } else {
      led2_state = false;
      neoPixel.setPixelColor(0, neoPixel.Color(0, 0, 0));
      neoPixel.show();
    }
    web_led2_control_enabled = true;
    Serial.println("LED2 " + String(led2_state ? "ON" : "OFF"));
  }
  
  server.send(200, "application/json",
    "{\"led1\":\"" + String(led1_state ? "ON":"OFF") +
    "\",\"led2\":\"" + String(led2_state ? "ON":"OFF") + "\"}");
}

void handleSensors() {
  // AP MODE: Lấy dữ liệu từ local sensors trực tiếp, không cần CoreIOT
  float t, h, soil, s;
  String m, source;
  
  if (isAPMode) {
    // Lấy dữ liệu từ local sensors (thread-safe)
    SensorData_t sensor_data;
    getSensorData(&sensor_data);
    t = sensor_data.temperature;
    h = sensor_data.humidity;
    soil = sensor_data.soil;
    s = glob_anomaly_score;
    m = glob_anomaly_message;
    source = "Local";
  }
  // STA MODE: Lấy dữ liệu từ CoreIOT
  // Flow: Sensors → CoreIOT → Mainserver/LCD hiển thị từ CoreIOT
  // Khi đợi dữ liệu mới từ CoreIOT, vẫn trả về giá trị hiện tại (last known values)
  else if (coreiot_data.is_valid) {
    // Luôn sử dụng giá trị từ CoreIOT (kể cả khi đợi cập nhật mới)
    t = coreiot_data.temperature;
    h = coreiot_data.humidity;
    soil = coreiot_data.soil;
    s = coreiot_data.anomaly_score;
    m = coreiot_data.anomaly_message;
    source = "CoreIOT";
  } else {
    // Chưa có dữ liệu từ CoreIOT lần đầu, lấy từ local để hiển thị
    SensorData_t sensor_data;
    getSensorData(&sensor_data);
    t = sensor_data.temperature;
    h = sensor_data.humidity;
    soil = sensor_data.soil;
    s = glob_anomaly_score;
    m = glob_anomaly_message;
    source = "Loading...";
  }
  
  String json = "{\"temp\":"+String(t)+",\"hum\":"+String(h)+",\"soil\":"+String(soil,1)+",\"score\":"+String(s,4)+",\"message\":\""+m+"\",\"source\":\""+source+"\"}";
  server.send(200, "application/json", json);
}

void handlePump() {
  String action = server.arg("action");
  
  if (action == "toggle") {
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      // Nếu đang ở chế độ AUTO, chuyển sang MANUAL khi toggle
      if (!pump_manual_control) {
        pump_manual_control = true;
        Serial.println("Switched to MANUAL mode");
      }
      pump_state = !pump_state;
      xSemaphoreGive(xMutexPumpControl);
    }
    Serial.println("Pump " + String(pump_state ? "ON" : "OFF"));
  } else if (action == "toggleMode") {
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      bool wasManual = pump_manual_control;
      pump_manual_control = !pump_manual_control;
      
      // Nếu chuyển sang AUTO mode, tính toán lại pump_state dựa trên soil moisture
      if (!pump_manual_control && wasManual) {
        // Chuyển từ MANUAL sang AUTO: tính toán lại state (logic giống maybom.cpp)
        float current_soil;
        if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
          current_soil = glob_soil;
          xSemaphoreGive(xMutexSensorData);
        } else {
          current_soil = glob_soil;
        }
        
        // AUTO mode: bật khi độ ẩm < min, tắt khi độ ẩm >= max
        bool auto_state;
        if (pump_state) {
          // Nếu pump đang bật, chỉ tắt khi đạt ngưỡng max
          auto_state = (current_soil < pump_threshold_max);
        } else {
          // Nếu pump đang tắt, chỉ bật khi dưới ngưỡng min
          auto_state = (current_soil < pump_threshold_min);
        }
        pump_state = auto_state;
        Serial.println("[AUTO] Switched to AUTO mode, calculated new pump state: " + String(current_soil) + "% (min:" + String(pump_threshold_min) + "%, max:" + String(pump_threshold_max) + "%) -> " + String(auto_state ? "ON" : "OFF"));
      }
      
      xSemaphoreGive(xMutexPumpControl);
    }
    Serial.println("Pump Mode: " + String(pump_manual_control ? "MANUAL" : "AUTO"));
  }
  
  // ĐỒNG BỘ: Cập nhật coreiot_data khi thay đổi từ mainserver
  bool current_pump_state;
  bool current_pump_mode;
  if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
    current_pump_state = pump_state;
    current_pump_mode = pump_manual_control;
    xSemaphoreGive(xMutexPumpControl);
  } else {
    current_pump_state = pump_state;
    current_pump_mode = pump_manual_control;
  }
  
  // Cập nhật coreiot_data
  coreiot_data.pump_state = current_pump_state;
  coreiot_data.pump_mode = current_pump_mode ? "MANUAL" : "AUTO";
  coreiot_data.last_update = millis();
  coreiot_data.is_valid = true;
  use_coreiot_data = true;
  
  String state = current_pump_state ? "ON" : "OFF";
  String mode = current_pump_mode ? "MANUAL" : "AUTO";
  server.send(200, "application/json",
    "{\"state\":\"" + state + "\",\"mode\":\"" + mode + "\"}");
  
  // ĐỒNG BỘ: Gửi dữ liệu lên CoreIOT để đồng bộ
  if (!isAPMode && WiFi.isConnected()) {
    // Thử MQTT trước
    if (client.connected()) {
      char attr[128];
      // Sử dụng boolean cho pump_mode để đồng bộ với CoreIOT app (true = MANUAL, false = AUTO)
      snprintf(attr, sizeof(attr),
          "{\"pump_state\": %s, \"pump_mode\": %s}",
          current_pump_state ? "true" : "false",
          current_pump_mode ? "true" : "false"
      );
      if (client.publish("v1/devices/me/attributes", attr)) {
        Serial.println("[SYNC] Pump state/mode pushed to CoreIOT via MQTT:");
        Serial.println(attr);
      } else {
        Serial.println("[SYNC] Failed to publish pump state/mode to CoreIOT");
      }
    } else {
      // Nếu MQTT không kết nối, dùng HTTP
      // Sử dụng boolean cho pump_mode để đồng bộ với CoreIOT app
      String data = "{\"pump_state\":" + String(current_pump_state ? "true" : "false") + ",\"pump_mode\":" + String(current_pump_mode ? "true" : "false") + "}";
      sendDataToCoreIOT(data);
    }
  }

}

void handlePumpStatus() {
  bool current_state;
  bool current_mode;
  
  // Nếu đang sử dụng dữ liệu CoreIOT, trả về dữ liệu từ đó
  if (!isAPMode && use_coreiot_data && coreiot_data.is_valid) {
    current_state = coreiot_data.pump_state;
    current_mode = (coreiot_data.pump_mode == "MANUAL");
  } else {
    // Sử dụng dữ liệu local
    if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
      current_state = pump_state;
      current_mode = pump_manual_control;
      xSemaphoreGive(xMutexPumpControl);
    } else {
      current_state = pump_state;
      current_mode = pump_manual_control;
    }
  }
  
  String state = current_state ? "ON" : "OFF";
  String mode = current_mode ? "MANUAL" : "AUTO";
  server.send(200, "application/json",
    "{\"state\":\"" + state + "\",\"mode\":\"" + mode + "\"}");
}

void handlePumpThresholds() {
  String action = server.arg("action");
  
  if (action == "get") {
    // Trả về giá trị threshold hiện tại
    String json = "{\"min\":" + String(pump_threshold_min) + ",\"max\":" + String(pump_threshold_max) + "}";
    server.send(200, "application/json", json);
  } else if (action == "set") {
    // Lưu giá trị threshold mới
    String minStr = server.arg("min");
    String maxStr = server.arg("max");
    
    if (minStr.length() > 0 && maxStr.length() > 0) {
      float minVal = minStr.toFloat();
      float maxVal = maxStr.toFloat();
      
      // Validate: min < max, và cả hai đều trong khoảng 0-100
      if (minVal >= 0 && maxVal <= 100 && minVal < maxVal) {
        savePumpThresholds(minVal, maxVal);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Pump thresholds updated successfully\"}");
      } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid threshold values. Min must be < Max, and both must be between 0-100\"}");
      }
    } else {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing min or max parameter\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid action. Use 'get' or 'set'\"}");
  }
}

// Handler để cung cấp dữ liệu cảm biến cho biểu đồ trong AP mode
void handleSensorDataAPI() {
  // Lấy dữ liệu cảm biến local (thread-safe)
  SensorData_t sensor_data;
  getSensorData(&sensor_data);
  
  // Lấy pump state (thread-safe)
  bool current_pump_state;
  bool current_pump_mode;
  if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
    current_pump_state = pump_state;
    current_pump_mode = pump_manual_control;
    xSemaphoreGive(xMutexPumpControl);
  } else {
    current_pump_state = pump_state;
    current_pump_mode = pump_manual_control;
  }
  
  // Tạo JSON response với dữ liệu cảm biến local
  String json = "{";
  json += "\"temperature\":" + String(sensor_data.temperature, 2) + ",";
  json += "\"humidity\":" + String(sensor_data.humidity, 2) + ",";
  json += "\"soil\":" + String(sensor_data.soil, 1) + ",";
  json += "\"anomaly_score\":" + String(glob_anomaly_score, 4) + ",";
  json += "\"anomaly_message\":\"" + glob_anomaly_message + "\",";
  json += "\"pump_state\":" + String(current_pump_state ? "true" : "false") + ",";
  json += "\"pump_mode\":\"" + String(current_pump_mode ? "MANUAL" : "AUTO") + "\",";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"mode\":\"AP\"";
  json += "}";
  
  server.send(200, "application/json", json);
  
  // Debug logging every 10 requests
  static int request_count = 0;
  if (++request_count >= 10) {
    Serial.println("[AP MODE] Sent sensor data: " + json);
    request_count = 0;
  }
}

void handleSettings() { 
  server.send(200, "text/html", settingsPage()); 
}

void handleConnect() {
  wifi_ssid = server.arg("ssid");
  wifi_password = server.arg("pass");
  
  // Gửi response ngay để không bị timeout
  server.send(200, "text/plain", "OK");
  
  // Đợi một chút để response được gửi đi
  delay(100);
  
  isAPMode = false;
  connecting = true;
  connect_start_ms = millis();
  connectToWiFi();
}

// Thêm handler để check status kết nối
void handleStatus() {
  String status = "connecting";
  String ip = "";
  
  if (WiFi.status() == WL_CONNECTED) {
    status = "connected";
    ip = WiFi.localIP().toString();
  } else if (millis() - connect_start_ms > 10000) {
    status = "failed";
    ip = WiFi.softAPIP().toString();
  }
  
  String json = "{\"status\":\"" + status + "\",\"ip\":\"" + ip + "\",\"mdns\":\"esp32.local\"}";
  server.send(200, "application/json", json);
}

// Handler để lưu CoreIOT credentials
// Handler để lưu location từ geolocation
void handleCoreIOTConfig() {
  String server_param = server.arg("server");
  String authType = server.arg("auth_type");
  String token = server.arg("token");
  String clientId = server.arg("client_id");
  String username = server.arg("username");
  String password = server.arg("password");
  
  if (server_param.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Server is required\"}");
    return;
  }
  
  bool useToken = (authType == "token");
  
  if (useToken && token.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Token is required when using token authentication\"}");
    return;
  }
  
  if (!useToken) {
    if (clientId.length() == 0) {
      clientId = "ESP32Client";  // Default client ID
    }
    if (username.length() == 0 || password.length() == 0) {
      server.send(400, "application/json", "{\"error\":\"Client ID, Username and password are required when using username/password authentication\"}");
      return;
    }
  } else {
    // For token auth, use default client ID if not provided
    if (clientId.length() == 0) {
      clientId = "ESP32Client";
    }
  }
  
  // Save credentials
  saveCoreIOTCredentials(server_param, token, clientId, username, password, useToken);
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"CoreIOT credentials saved successfully. Device will reconnect automatically.\"}");
}

// ========== WiFi ==========
void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/sensors", HTTP_GET, handleSensors);
  server.on("/pump", HTTP_GET, handlePump);
  server.on("/pumpstatus", HTTP_GET, handlePumpStatus);
  server.on("/pumpthresholds", HTTP_GET, handlePumpThresholds);
  server.on("/api/sensor-data", HTTP_GET, handleSensorDataAPI);  // API for charts in AP mode
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/coreiot", HTTP_POST, handleCoreIOTConfig);
  server.on("/coreiot-data", HTTP_POST, handleCoreIOTData);
  server.on("/data-source", HTTP_GET, []() {
    // THEO CHU TRÌNH MỚI: Luôn hiển thị nguồn dữ liệu là CoreIOT nếu đã có dữ liệu
    // Khi đợi cập nhật mới, vẫn hiển thị CoreIOT (giá trị last known)
    String source;
    if (coreiot_data.is_valid) {
      // Đã có dữ liệu từ CoreIOT (kể cả khi đợi cập nhật mới)
      source = "CoreIOT";
    } else if (coreiot_server.length() > 0) {
      // Đang kết nối nhưng chưa có dữ liệu lần đầu
      source = "Loading...";
    } else {
      // Chưa cấu hình CoreIOT
      source = "Not Configured";
    }
    server.send(200, "application/json", "{\"source\":\"" + source + "\"}");
  });
  server.begin();
}

void startAP() {
  WiFi.mode(WIFI_AP);
  delay(100);
  bool connected = WiFi.softAP(ssid.c_str(), password.c_str());
  if (connected) {
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Setup mDNS
    if (MDNS.begin("esp32")) {
      Serial.println("mDNS responder started: http://esp32.local");
      MDNS.addService("http", "tcp", 80);
    }
  } else {
    Serial.println("Failed to start AP");
  }
  
  isAPMode = true;
  connecting = false;
  
  // Reset CoreIOT data validity when entering AP mode
  // This ensures LCD and web interface use local sensor data
  coreiot_data.is_valid = false;
  use_coreiot_data = false;
  Serial.println("=== SWITCHED TO AP MODE ===");
  Serial.println("[AP MODE] CoreIOT data invalidated - using local sensor data");
  Serial.println("[AP MODE] Website will use /api/sensor-data endpoint");
  Serial.println("[AP MODE] LCD will display local sensor readings");
  Serial.println("========================");
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  Serial.print("Connecting to: ");
  Serial.print(wifi_ssid.c_str());
  Serial.print(" Password: ");
  Serial.println(wifi_password.c_str());
}

// ========== Main task ==========
void main_server_task(void *pvParameters){
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  
  // Load pump thresholds on startup
  loadPumpThresholds();
  
  Serial.println("Web Server Task Started");
  
  // Tự động thử STA mode trước
  Serial.println("Trying STA mode first...");
  isAPMode = false;
  connecting = true;
  connect_start_ms = millis();
  connectToWiFi();
  
  // Đợi 8 giây để thử kết nối
  unsigned long waitStart = millis();
  while (millis() - waitStart < 8000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("✅ STA Connected! IP: ");
      Serial.println(WiFi.localIP());
      
      // Setup mDNS for STA mode
      if (MDNS.begin("esp32")) {
        Serial.println("mDNS responder started: http://esp32.local");
        MDNS.addService("http", "tcp", 80);
      }
      
      isWifiConnected = true;
      isAPMode = false;
      connecting = false;
      xSemaphoreGive(xBinarySemaphoreInternet);
      break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  
  // Nếu STA thất bại → chuyển AP
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ STA failed → Starting AP mode");
    startAP();
    isAPMode = true;
    connecting = false;
    isWifiConnected = false;
    // Reset CoreIOT data validity when switching to AP mode
    coreiot_data.is_valid = false;
    use_coreiot_data = false;
  }
  
  // Setup server
  setupServer();
  Serial.println("Web server ready!");
  
  while(1){
    server.handleClient();
    
    // Handle WiFi connection logic
    if (connecting) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to WiFi! IP: ");
        Serial.println(WiFi.localIP());
        
        // Setup mDNS
        if (MDNS.begin("esp32")) {
          Serial.println("mDNS responder started: http://esp32.local");
          MDNS.addService("http", "tcp", 80);
        }
        
        isWifiConnected = true;
        xSemaphoreGive(xBinarySemaphoreInternet);
        isAPMode = false;
        connecting = false;
      } else if (millis() - connect_start_ms > 10000) {
        Serial.println("WiFi connection timeout, switching back to AP mode");
        startAP();
        setupServer();
        connecting = false;
        isWifiConnected = false;
        // Reset CoreIOT data validity when switching back to AP mode
        coreiot_data.is_valid = false;
        use_coreiot_data = false;
      }
    }
    
    // Fetch CoreIOT data khi ở STA mode
    if (!isAPMode && WiFi.isConnected() && (millis() - last_coreiot_fetch > COREIOT_FETCH_INTERVAL)) {
      fetchCoreIOTData();
      last_coreiot_fetch = millis();
    }
    
    // Debug logging trong AP mode (mỗi 30 giây)
    if (isAPMode) {
      static unsigned long lastAPLog = 0;
      if (millis() - lastAPLog > 30000) {
        SensorData_t sensor_data;
        getSensorData(&sensor_data);
        bool current_pump_state = pump_state;
        bool current_pump_mode = pump_manual_control;
        
        Serial.println("[AP MODE STATUS]");
        Serial.println("  Temperature: " + String(sensor_data.temperature, 2) + "°C");
        Serial.println("  Humidity: " + String(sensor_data.humidity, 2) + "%");
        Serial.println("  Soil: " + String(sensor_data.soil, 1) + "%");
        Serial.println("  Anomaly Score: " + String(glob_anomaly_score, 4));
        Serial.println("  Anomaly Message: " + glob_anomaly_message);
        Serial.println("  Pump State: " + String(current_pump_state ? "ON" : "OFF"));
        Serial.println("  Pump Mode: " + String(current_pump_mode ? "MANUAL" : "AUTO"));
        Serial.println("  Uptime: " + String(millis() / 1000) + "s");
        Serial.println("================");
        lastAPLog = millis();
      }
    }
    
    // Gửi local sensor data lên CoreIOT định kỳ (mỗi 30 giây)
    static unsigned long lastSensorUpload = 0;
    if (!isAPMode && WiFi.isConnected() && (millis() - lastSensorUpload > 30000)) {
      SensorData_t sensor_data;
      getSensorData(&sensor_data);
      
      String sensorJson = "{\"temperature\":" + String(sensor_data.temperature) + 
                         ",\"humidity\":" + String(sensor_data.humidity) + 
                         ",\"soil\":" + String(sensor_data.soil) + 
                         ",\"anomaly_score\":" + String(glob_anomaly_score, 4) + 
                         ",\"anomaly_message\":\"" + glob_anomaly_message + "\"}";
      
      // Thử MQTT trước, nếu không có thì dùng HTTP
      if (client.connected()) {
        client.publish("v1/devices/me/telemetry", sensorJson.c_str());
        Serial.println("[CoreIOT] Sensor data sent via MQTT: " + sensorJson);
      } else {
        sendDataToCoreIOT(sensorJson);
      }
      
      lastSensorUpload = millis();
    }
    
    // Kiểm tra dữ liệu CoreIOT còn hợp lệ không (timeout sau 30 giây)
    if (use_coreiot_data && (millis() - coreiot_data.last_update > 30000)) {
      use_coreiot_data = false;
      Serial.println("[CoreIOT] Data timeout, switching to local data");
    }
    
    // Debug status
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 20000) {
      if (isAPMode) {
        Serial.println("AP Mode - IP: " + WiFi.softAPIP().toString() + " | Data: Local");
      } else {
        Serial.println("STA Mode - IP: " + WiFi.localIP().toString() + " | Data: " + (use_coreiot_data ? "CoreIOT" : "Local"));
      }
      lastDebug = millis();
    }
    
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}