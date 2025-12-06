#include "mainserver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "global.h"

// WiFi state variables
bool isAPMode = true;
unsigned long connect_start_ms = 0;
bool connecting = false;

// Adafruit IO data
AdafruitData adafruit_data = {};
bool use_adafruit_data = false;
unsigned long last_adafruit_fetch = 0;
const unsigned long ADAFRUIT_FETCH_INTERVAL = 10000; // 10 seconds

WebServer server(80);

// Settings Page - Simple and Fast
String settingsPage() {
  String currentMode = isAPMode ? "AP Mode" : "STA Mode";
  String currentIP = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  
  // Load current Adafruit IO settings
  loadAdafruitCredentials();
  String currentUsername = adafruit_username;
  String currentKey = adafruit_key;
  
  return R"rawliteral(
    <!DOCTYPE html><html><head>
      <meta charset='utf-8'>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>Settings</title>
      <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          min-height: 100vh;
          display: flex;
          align-items: center;
          justify-content: center;
          padding: 20px;
        }
        .container {
          background: white;
          border-radius: 16px;
          box-shadow: 0 20px 60px rgba(0,0,0,0.3);
          max-width: 500px;
          width: 100%;
          padding: 40px;
        }
        h1 { 
          font-size: 28px;
          margin-bottom: 8px;
          color: #333;
        }
        .status {
          background: #f0f4ff;
          padding: 16px;
          border-radius: 10px;
          margin-bottom: 28px;
          font-size: 14px;
          color: #666;
        }
        .status-line {
          display: flex;
          justify-content: space-between;
          margin-bottom: 8px;
        }
        .status-line:last-child { margin-bottom: 0; }
        .status-label { font-weight: 600; color: #333; }
        .status-value { color: #667eea; font-weight: 500; }
        
        .form-group {
          margin-bottom: 20px;
        }
        label {
          display: block;
          font-weight: 600;
          color: #333;
          margin-bottom: 8px;
          font-size: 14px;
        }
        input {
          width: 100%;
          padding: 12px;
          border: 2px solid #e0e0e0;
          border-radius: 8px;
          font-size: 14px;
          transition: border-color 0.3s;
        }
        input:focus {
          outline: none;
          border-color: #667eea;
        }
        button {
          width: 100%;
          padding: 12px;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          color: white;
          border: none;
          border-radius: 8px;
          font-size: 16px;
          font-weight: 600;
          cursor: pointer;
          transition: transform 0.2s;
          margin-top: 10px;
        }
        button:hover { transform: translateY(-2px); }
        button:active { transform: translateY(0); }
        
        .message {
          padding: 12px;
          border-radius: 8px;
          margin-top: 16px;
          text-align: center;
          font-size: 14px;
          display: none;
        }
        .message.success {
          background: #d4edda;
          color: #155724;
          display: block;
        }
        .message.error {
          background: #f8d7da;
          color: #721c24;
          display: block;
        }
        .spinner {
          display: none;
          text-align: center;
          color: #667eea;
          font-size: 14px;
          margin-top: 10px;
        }
        .spinner.show { display: block; }
        
        .section-title {
          font-size: 16px;
          font-weight: 700;
          color: #333;
          margin-top: 24px;
          margin-bottom: 16px;
          padding-bottom: 8px;
          border-bottom: 2px solid #667eea;
        }
      </style>
    </head><body>
      <div class="container">
        <h1>⚙️ Settings</h1>
        
        <div class="status">
          <div class="status-line">
            <span class="status-label">Mode:</span>
            <span class="status-value">)rawliteral" + currentMode + R"rawliteral(</span>
          </div>
          <div class="status-line">
            <span class="status-label">IP:</span>
            <span class="status-value">)rawliteral" + currentIP + R"rawliteral(</span>
          </div>
        </div>
        
        <div class="section-title">📡 WiFi Connection</div>
        <form id="wifiForm">
          <div class="form-group">
            <label for="ssid">Network Name (SSID)</label>
            <input type="text" id="ssid" name="ssid" placeholder="WiFi name" required>
          </div>
          
          <div class="form-group">
            <label for="pass">Password</label>
            <input type="password" id="pass" name="pass" placeholder="WiFi password" required>
          </div>
          
          <button type="submit">🔗 Connect WiFi</button>
          <div class="spinner" id="wifiSpinner">Connecting...</div>
          <div class="message" id="wifiMessage"></div>
        </form>
        
        <div class="section-title">🌐 Adafruit IO Config</div>
        <form id="settingsForm">
          <div class="form-group">
            <label for="username">Adafruit IO Username</label>
            <input type="text" id="username" name="username" value=")rawliteral" + currentUsername + R"rawliteral(" required>
          </div>
          
          <div class="form-group">
            <label for="key">Adafruit IO Key</label>
            <input type="password" id="key" name="key" value=")rawliteral" + currentKey + R"rawliteral(" required>
          </div>
          
          <button type="submit">💾 Save Adafruit Config</button>
          <div class="spinner" id="spinner">Saving...</div>
          <div class="message" id="message"></div>
        </form>
      </div>
      
      <script>
        // WiFi Form Handler
        document.getElementById('wifiForm').addEventListener('submit', async function(e) {
          e.preventDefault();
          const ssid = document.getElementById('ssid').value;
          const pass = document.getElementById('pass').value;
          const spinner = document.getElementById('wifiSpinner');
          const message = document.getElementById('wifiMessage');
          
          spinner.classList.add('show');
          message.style.display = 'none';
          
          try {
            const response = await fetch('/connect?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass));
            if (response.ok) {
              message.textContent = '✅ Connecting to ' + ssid + '...';
              message.classList.add('success');
              message.style.display = 'block';
            } else {
              message.textContent = '❌ Connection failed';
              message.classList.add('error');
              message.style.display = 'block';
            }
          } catch (error) {
            message.textContent = '❌ Error: ' + error.message;
            message.classList.add('error');
            message.style.display = 'block';
          } finally {
            spinner.classList.remove('show');
          }
        });
        
        // Adafruit Settings Form Handler
        document.getElementById('settingsForm').addEventListener('submit', async function(e) {
          e.preventDefault();
          const spinner = document.getElementById('spinner');
          const message = document.getElementById('message');
          const username = document.getElementById('username').value;
          const key = document.getElementById('key').value;
          
          spinner.classList.add('show');
          message.classList.remove('success', 'error');
          
          try {
            const response = await fetch('/adafruit', {
              method: 'POST',
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
              body: 'username=' + encodeURIComponent(username) + '&key=' + encodeURIComponent(key)
            });
            
            if (response.ok) {
              message.textContent = '✅ Configuration saved! Restart ESP32 to apply.';
              message.classList.add('success');
            } else {
              message.textContent = '❌ Failed to save configuration';
              message.classList.add('error');
            }
          } catch (error) {
            message.textContent = '❌ Error: ' + error.message;
            message.classList.add('error');
          } finally {
            spinner.classList.remove('show');
            message.style.display = 'block';
          }
        });
      </script>
    </body></html>
  )rawliteral";
}

// Handlers
void handleRoot() {
  Serial.println("[Web] Root page requested - redirecting to /settings");
  // Redirect to settings page for faster load
  server.sendHeader("Location", "/settings", true);
  server.send(302, "text/plain", "");
}

void handleSettings() { 
  server.send(200, "text/html", settingsPage()); 
}

void handleConnect() {
  wifi_ssid = server.arg("ssid");
  wifi_password = server.arg("pass");
  
  // Validate input
  if (wifi_ssid.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"SSID cannot be empty\"}");
    return;
  }
  
  Serial.println(F("=== WiFi Connection Request ==="));
  Serial.print(F("SSID: "));
  Serial.println(wifi_ssid);
  Serial.print(F("Password length: "));
  Serial.println(wifi_password.length());
  
  // Send response immediately
  server.send(200, F("text/plain"), F("OK"));
  
  // Wait a bit for response to be sent
  delay(200);
  
  isAPMode = false;
  connecting = true;
  connect_start_ms = millis();
  connectToWiFi();
  
  Serial.println(F("WiFi connection process started..."));
}

void handleStatus() {
  String status = "connecting";
  String ip = "";
  
  if (WiFi.status() == WL_CONNECTED) {
    status = "connected";
    ip = WiFi.localIP().toString();
  } else if (millis() - connect_start_ms > 20000) {
    status = "failed";
    ip = WiFi.softAPIP().toString();
  }
  
  String json = "{\"status\":\"" + status + "\",\"ip\":\"" + ip + "\"}";
  server.send(200, "application/json", json);
}

void handleAdafruitIOConfig() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }
  
  String username = server.arg("username");
  String key = server.arg("key");
  
  // Validate input
  if (username.length() == 0 || key.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Username and key are required\"}");
    return;
  }
  
  // Save credentials
  saveAdafruitCredentials(username, key);
  
  Serial.println(F("[Settings] Adafruit IO credentials saved"));
  Serial.print(F("Username: "));
  Serial.println(username);
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved\"}");
}

// Setup and WiFi management
void connectToWiFi() {
  Serial.println("Connecting to WiFi: " + wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/adafruit", HTTP_POST, handleAdafruitIOConfig);
  
  // Add 404 handler for debugging
  server.onNotFound([]() {
    Serial.print("[Web] 404 Not Found: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "404: Not Found");
  });
  
  server.begin();
  Serial.println("[Web] Server started on port 80");
}

void startAP() {
  WiFi.mode(WIFI_AP);
  delay(100);
  bool connected = WiFi.softAP(ssid.c_str(), password.c_str());
  if (connected) {
    Serial.print(F("AP IP address: "));
    Serial.println(WiFi.softAPIP());
    
    // Setup mDNS
    if (MDNS.begin("esp32")) {
      Serial.println(F("mDNS responder started: http://esp32.local"));
      MDNS.addService("http", "tcp", 80);
    }
  } else {
    Serial.println(F("Failed to start AP"));
  }
  
  isAPMode = true;
  connecting = false;
}

void main_server_task(void *pvParameters){
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  
  Serial.println(F("Web Server Task Started"));
  
  // Try STA mode first
  Serial.println(F("Trying STA mode first..."));
  isAPMode = false;
  connecting = true;
  connect_start_ms = millis();
  connectToWiFi();
  
  // Wait 8 seconds to try connection
  unsigned long waitStart = millis();
  while (millis() - waitStart < 8000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("✅ STA Connected! IP: "));
      Serial.println(WiFi.localIP());
      
      // Setup mDNS for STA mode
      if (MDNS.begin("esp32")) {
        Serial.println(F("mDNS responder started: http://esp32.local"));
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
  
  // If STA failed → switch to AP
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("❌ STA failed → Starting AP mode"));
    startAP();
    isAPMode = true;
    connecting = false;
    isWifiConnected = false;
  }
  
  // Setup server
  setupServer();
  Serial.println(F("Web server ready!"));
  
  while(1){
    // Handle web server in both AP and STA modes
    server.handleClient();
    
    // Handle WiFi connection logic
    if (connecting) {
      unsigned long elapsed = millis() - connect_start_ms;
      
      // Log progress every 3 seconds
      static unsigned long lastLog = 0;
      if (elapsed - lastLog > 3000) {
        Serial.print(F("[WiFi] Connecting... ("));
        Serial.print(elapsed / 1000);
        Serial.println(F("s elapsed)"));
        lastLog = elapsed;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("✅ Connected to WiFi!"));
        Serial.print(F("  IP: "));
        Serial.println(WiFi.localIP());
        
        // Setup mDNS
        if (MDNS.begin("esp32")) {
          Serial.println(F("mDNS responder started: http://esp32.local"));
          MDNS.addService("http", "tcp", 80);
        }
        
        isWifiConnected = true;
        xSemaphoreGive(xBinarySemaphoreInternet);
        isAPMode = false;
        connecting = false;
      } else if (elapsed > 20000) {
        // Timeout after 20 seconds
        Serial.println(F("WiFi connection timeout (20s), switching back to AP mode"));
        startAP();
        connecting = false;
        isWifiConnected = false;
      }
    }
    
    // Debug logging in AP mode (every 30 seconds)
    if (isAPMode) {
      static unsigned long lastAPLog = 0;
      if (millis() - lastAPLog > 30000) {
        Serial.println("AP Mode - IP: " + WiFi.softAPIP().toString() + " | Data: Local");
        lastAPLog = millis();
      }
    }
    
    // Debug logging in STA mode (every 20 seconds)
    if (!isAPMode && WiFi.isConnected()) {
      static unsigned long lastDebug = 0;
      if (millis() - lastDebug > 20000) {
        Serial.println("STA Mode - IP: " + WiFi.localIP().toString() + " | Data: Adafruit IO");
        lastDebug = millis();
      }
    }
    
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
