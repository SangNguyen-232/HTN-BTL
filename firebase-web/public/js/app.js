// Main application logic - Lấy dữ liệu từ CoreIOT
let currentColor = 'white';
let sensorUpdateInterval;
let pumpUpdateInterval;
let currentRefreshRate = 5; // Default 5 seconds

// Kiểm tra config CoreIOT khi load trang
window.addEventListener('DOMContentLoaded', function() {
  if (!COREIOT_CONFIG.isConfigured()) {
    // Thử lấy config từ ESP32 trước, nếu không được thì hiển thị dialog
    tryLoadFromESP32();
  } else {
    // Đã có config, bắt đầu load dữ liệu
    startDataUpdates();
  }
  
  // Load saved refresh rate
  loadRefreshRate();
  updateChartRefreshDisplay();
  initChart();
  
  // Update time
  updateTime();
  setInterval(updateTime, 1000);
});

// Thử lấy CoreIOT config từ ESP32
async function tryLoadFromESP32() {
  // Lấy ESP32 IP từ localStorage hoặc prompt
  let esp32IP = localStorage.getItem('esp32_ip');
  
  if (!esp32IP) {
    // Thử các IP phổ biến của ESP32
    const commonIPs = ['192.168.4.1', 'esp32.local'];
    let found = false;
    
    for (const ip of commonIPs) {
      try {
        const url = `http://${ip}/coreiot-config`;
        const response = await fetch(url, { timeout: 2000 });
        if (response.ok) {
          const data = await response.json();
          if (data.configured && data.server && data.token) {
            COREIOT_CONFIG.server = data.server;
            COREIOT_CONFIG.token = data.token;
            COREIOT_CONFIG.save();
            localStorage.setItem('esp32_ip', ip);
            startDataUpdates();
            found = true;
            return;
          }
        }
      } catch (e) {
        // Continue to next IP
      }
    }
    
    if (!found) {
      // Không tìm thấy ESP32, hiển thị dialog để user nhập
      showConfigDialog();
    }
  } else {
    // Đã có ESP32 IP, thử fetch
    try {
      const url = `http://${esp32IP}/coreiot-config`;
      const response = await fetch(url, { timeout: 2000 });
      if (response.ok) {
        const data = await response.json();
        if (data.configured && data.server && data.token) {
          COREIOT_CONFIG.server = data.server;
          COREIOT_CONFIG.token = data.token;
          COREIOT_CONFIG.save();
          startDataUpdates();
          return;
        }
      }
    } catch (e) {
      console.warn('Không thể kết nối ESP32, sử dụng dialog');
    }
    
    // Nếu không fetch được, hiển thị dialog
    showConfigDialog();
  }
}

// Hiển thị dialog để cấu hình CoreIOT
function showConfigDialog() {
  const esp32IP = prompt('Nhập ESP32 IP (để lấy token tự động) hoặc để trống để nhập thủ công:\nVí dụ: 192.168.1.100 hoặc esp32.local', localStorage.getItem('esp32_ip') || '');
  
  if (esp32IP && esp32IP.trim().length > 0) {
    // Thử lấy từ ESP32
    fetch(`http://${esp32IP.trim()}/coreiot-config`)
      .then(response => response.json())
      .then(data => {
        if (data.configured && data.server && data.token) {
          COREIOT_CONFIG.server = data.server;
          COREIOT_CONFIG.token = data.token;
          COREIOT_CONFIG.save();
          localStorage.setItem('esp32_ip', esp32IP.trim());
          alert('Đã lấy cấu hình CoreIOT từ ESP32 thành công!');
          startDataUpdates();
        } else {
          alert('ESP32 chưa được cấu hình CoreIOT. Vui lòng cấu hình trên ESP32 trước.');
          showManualConfigDialog();
        }
      })
      .catch(err => {
        console.error('Lỗi khi lấy config từ ESP32:', err);
        alert('Không thể kết nối ESP32. Vui lòng nhập thủ công.');
        showManualConfigDialog();
      });
  } else {
    // Nhập thủ công
    showManualConfigDialog();
  }
}

// Hiển thị dialog nhập thủ công
function showManualConfigDialog() {
  const server = prompt('Nhập CoreIOT Server (ví dụ: app.coreiot.io):', COREIOT_CONFIG.server);
  if (!server) {
    alert('Vui lòng nhập CoreIOT Server để tiếp tục!');
    showManualConfigDialog();
    return;
  }
  
  const token = prompt('Nhập CoreIOT Access Token:');
  if (!token) {
    alert('Vui lòng nhập CoreIOT Access Token để tiếp tục!');
    showManualConfigDialog();
    return;
  }
  
  COREIOT_CONFIG.server = server;
  COREIOT_CONFIG.token = token;
  COREIOT_CONFIG.save();
  
  // Bắt đầu load dữ liệu
  startDataUpdates();
}

// Load saved refresh rate from localStorage
function loadRefreshRate() {
  const saved = localStorage.getItem('sensorRefreshRate');
  if (saved && !isNaN(saved)) {
    currentRefreshRate = parseInt(saved);
    const refreshInput = document.getElementById('refresh-input');
    if (refreshInput) {
      refreshInput.value = currentRefreshRate;
    }
    updateRefreshDisplay();
  }
}

function updateRefreshDisplay() {
  const refreshCurrent = document.getElementById('refresh-current');
  if (refreshCurrent) {
    refreshCurrent.textContent = currentRefreshRate + 's';
  }
  updateChartRefreshDisplay();
}

function updateChartRefreshDisplay() {
  const chartRefreshRate = document.getElementById('chart-refresh-rate');
  if (chartRefreshRate) {
    chartRefreshRate.textContent = currentRefreshRate + 's';
  }
}

function updateRefreshRate() {
  const input = document.getElementById('refresh-input');
  const newRate = parseInt(input.value);
  
  if (isNaN(newRate) || newRate < 1 || newRate > 60) {
    alert('Vui lòng nhập refresh rate hợp lệ từ 1 đến 60 giây.');
    input.value = currentRefreshRate;
    return;
  }
  
  currentRefreshRate = newRate;
  localStorage.setItem('sensorRefreshRate', currentRefreshRate.toString());
  updateRefreshDisplay();
  
  // Clear existing intervals
  if (sensorUpdateInterval) clearInterval(sensorUpdateInterval);
  if (pumpUpdateInterval) clearInterval(pumpUpdateInterval);
  
  // Start new intervals with updated rate
  startDataUpdates();
}

function setPresetRefresh(seconds) {
  const input = document.getElementById('refresh-input');
  if (input) {
    input.value = seconds;
    updateRefreshRate();
  }
}

// Bắt đầu cập nhật dữ liệu từ CoreIOT
function startDataUpdates() {
  const intervalMs = currentRefreshRate * 1000;
  
  // Update sensor data từ CoreIOT
  sensorUpdateInterval = setInterval(async () => {
    const data = await fetchCoreIOTData();
    
    if (data) {
      // Parse dữ liệu từ CoreIOT (ThingsBoard format)
      // Dữ liệu có thể ở format: {temperature: value} hoặc {temperature: {value: value, ts: timestamp}}
      let temp = '--', hum = '--', soil = '--', score = '--', message = 'Normal';
      
      // Xử lý temperature
      if (data.temperature !== undefined) {
        const tempVal = (typeof data.temperature === 'object' && data.temperature.value !== undefined) 
          ? data.temperature.value : data.temperature;
        temp = parseFloat(tempVal).toFixed(2);
      }
      
      // Xử lý humidity
      if (data.humidity !== undefined) {
        const humVal = (typeof data.humidity === 'object' && data.humidity.value !== undefined) 
          ? data.humidity.value : data.humidity;
        hum = parseFloat(humVal).toFixed(2);
      }
      
      // Xử lý soil moisture (có thể có tên soil_moisture_value hoặc soil)
      if (data.soil_moisture_value !== undefined) {
        const soilVal = (typeof data.soil_moisture_value === 'object' && data.soil_moisture_value.value !== undefined) 
          ? data.soil_moisture_value.value : data.soil_moisture_value;
        soil = parseFloat(soilVal).toFixed(1);
      } else if (data.soil !== undefined) {
        const soilVal = (typeof data.soil === 'object' && data.soil.value !== undefined) 
          ? data.soil.value : data.soil;
        soil = parseFloat(soilVal).toFixed(1);
      }
      
      // Xử lý anomaly score
      if (data.anomaly_score !== undefined) {
        const scoreVal = (typeof data.anomaly_score === 'object' && data.anomaly_score.value !== undefined) 
          ? data.anomaly_score.value : data.anomaly_score;
        score = parseFloat(scoreVal).toFixed(4);
      }
      
      // Xử lý anomaly message
      if (data.anomaly_message !== undefined) {
        const msgVal = (typeof data.anomaly_message === 'object' && data.anomaly_message.value !== undefined) 
          ? data.anomaly_message.value : data.anomaly_message;
        message = String(msgVal) || 'Normal';
      }
      
      // Cập nhật hiển thị bằng hàm chung
      const tempVal = (temp === '--') ? undefined : parseFloat(temp);
      const humVal = (hum === '--') ? undefined : parseFloat(hum);
      const soilVal = (soil === '--') ? undefined : parseFloat(soil);
      const scoreVal = (score === '--') ? undefined : parseFloat(score);
      
      updateSensorDisplay(tempVal, humVal, soilVal, scoreVal, message);
      
      // Cập nhật chart nếu có dữ liệu hợp lệ
      if (tempVal !== undefined && humVal !== undefined && soilVal !== undefined) {
        updateChart(tempVal, humVal, soilVal);
      }
      
      // Cập nhật data source indicator
      const dot = document.getElementById('source-dot');
      const text = document.getElementById('source-text');
      if (dot) {
        dot.className = 'source-dot coreiot';
      }
      if (text) {
        text.textContent = 'CoreIOT';
      }
    } else {
      // Không lấy được dữ liệu, hiển thị loading
      const elements = ['temp', 'hum', 'soil', 'score', 'message'];
      elements.forEach(id => {
        const el = document.getElementById(id);
        if (el) {
          el.classList.add('loading');
        }
      });
      
      // Cập nhật data source indicator
      const dot = document.getElementById('source-dot');
      const text = document.getElementById('source-text');
      if (dot) {
        dot.className = 'source-dot local';
      }
      if (text) {
        text.textContent = 'No Connection';
      }
    }
  }, intervalMs);
  
  // Update pump status từ CoreIOT attributes (giảm frequency để tránh spam)
  pumpUpdateInterval = setInterval(async () => {
    const data = await fetchCoreIOTData();
    
    if (data) {
      // Lấy pump state và mode từ attributes
      const pumpState = data.pump_state !== undefined ? (data.pump_state ? 'ON' : 'OFF') : null;
      const pumpMode = data.pump_mode !== undefined ? data.pump_mode : null;
      
      if (pumpState) {
        updatePumpStatus('pump-state-status', pumpState);
      }
      if (pumpMode) {
        updatePumpMode('pump-mode-status', pumpMode);
      }
    }
  }, 5000); // Tăng lên 5 giây thay vì 1 giây để giảm số request
}

// Hàm cập nhật hiển thị sensor data (tương tự ESP32 mainserver)
function updateSensorDisplay(temperature, humidity, soil, anomaly_score = 0, anomaly_message = 'Normal') {
  // Format giá trị hiển thị
  const tempStr = (temperature !== undefined && !isNaN(temperature)) ? parseFloat(temperature).toFixed(2) : '--';
  const humStr = (humidity !== undefined && !isNaN(humidity)) ? parseFloat(humidity).toFixed(2) : '--';
  const soilStr = (soil !== undefined && !isNaN(soil)) ? parseFloat(soil).toFixed(1) : '--';
  const scoreStr = (anomaly_score !== undefined && !isNaN(anomaly_score)) ? parseFloat(anomaly_score).toFixed(4) : '--';
  const messageStr = anomaly_message || 'Normal';
  
  // Cập nhật giá trị hiển thị
  const tempEl = document.getElementById('temp');
  const humEl = document.getElementById('hum');
  const soilEl = document.getElementById('soil');
  const scoreEl = document.getElementById('score');
  const messageEl = document.getElementById('message');
  
  if (tempEl) tempEl.innerText = tempStr;
  if (humEl) humEl.innerText = humStr;
  if (soilEl) soilEl.innerText = (soilStr === '--') ? '--' : formatSoil(soilStr);
  if (scoreEl) scoreEl.innerText = scoreStr;
  if (messageEl) messageEl.innerText = messageStr;
  
  // Cập nhật đơn vị
  const tempUnit = tempEl ? tempEl.parentElement.querySelector('.unit') : null;
  const humUnit = humEl ? humEl.parentElement.querySelector('.unit') : null;
  const soilUnit = soilEl ? soilEl.parentElement.querySelector('.unit') : null;
  
  if (tempUnit) {
    tempUnit.innerHTML = (tempStr === '--') ? '' : '°C';
  }
  if (humUnit) {
    humUnit.innerHTML = (humStr === '--') ? '' : '%';
  }
  if (soilUnit) {
    soilUnit.innerHTML = (soilStr === '--') ? '' : '%';
  }
  
  // Xóa class loading khi có dữ liệu hợp lệ
  const elements = ['temp', 'hum', 'soil', 'score', 'message'];
  elements.forEach(id => {
    const el = document.getElementById(id);
    if (el) {
      if (tempStr !== '--' || humStr !== '--' || soilStr !== '--') {
        el.classList.remove('loading');
      } else {
        el.classList.add('loading');
      }
    }
  });
  
  console.log(`[Display] Cập nhật hiển thị: T=${tempStr}°C, H=${humStr}%, S=${soilStr}%, Score=${scoreStr}, Msg=${messageStr}`);
}

// Control LED qua CoreIOT RPC
async function controlLED(id, action, color) {
  if (!COREIOT_CONFIG.isConfigured()) {
    alert('Vui lòng cấu hình CoreIOT trước!');
    return;
  }
  
  const result = await sendRPCCommand('setLED', {
    id: id,
    action: action,
    color: color
  });
  
  if (result) {
    // Cập nhật UI
    if (result.led1 !== undefined) {
      updateStatus('led1-status', result.led1);
    }
    if (result.led2 !== undefined) {
      updateStatus('led2-status', result.led2);
    }
  }
}

function setColor(color) {
  currentColor = color;
  const led2Status = document.getElementById('led2-status');
  if (led2Status && led2Status.textContent.trim() === 'ON') {
    controlLED(2, 'on', color);
  }
}

function setCustomColor() {
  const colorInput = document.getElementById('customColor');
  if (colorInput) {
    currentColor = colorInput.value;
    const led2Status = document.getElementById('led2-status');
    if (led2Status && led2Status.textContent.trim() === 'ON') {
      controlLED(2, 'on', currentColor);
    }
  }
}

function updateStatus(elementId, state) {
  const el = document.getElementById(elementId);
  if (!el) return;
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
  const timeEl = document.getElementById('time');
  if (timeEl) {
    timeEl.innerText = hours + ':' + minutes + ':' + seconds;
  }
}

// Toggle Pump qua CoreIOT RPC
async function togglePump() {
  if (!COREIOT_CONFIG.isConfigured()) {
    alert('Vui lòng cấu hình CoreIOT trước!');
    return;
  }
  
  const statusEl = document.getElementById('pump-state-status');
  const modeEl = document.getElementById('pump-mode-status');
  if (!statusEl || !modeEl) return;
  
  const currentState = statusEl.textContent.trim();
  const currentMode = modeEl.textContent.trim();
  const newState = currentState === 'ON' ? 'OFF' : 'ON';
  
  // Cập nhật UI ngay lập tức
  updatePumpStatus('pump-state-status', newState);
  if (currentMode === 'AUTO') {
    updatePumpMode('pump-mode-status', 'MANUAL');
  }
  
  // Gửi RPC command đến CoreIOT
  const result = await sendRPCCommand('togglePump', {});
  
  if (result) {
    if (result.state) {
      updatePumpStatus('pump-state-status', result.state);
    }
    if (result.mode) {
      updatePumpMode('pump-mode-status', result.mode);
    }
  } else {
    // Rollback nếu lỗi
    updatePumpStatus('pump-state-status', currentState);
    updatePumpMode('pump-mode-status', currentMode);
  }
}

async function togglePumpMode() {
  if (!COREIOT_CONFIG.isConfigured()) {
    alert('Vui lòng cấu hình CoreIOT trước!');
    return;
  }
  
  const modeEl = document.getElementById('pump-mode-status');
  if (!modeEl) return;
  
  const currentMode = modeEl.textContent.trim();
  const newMode = currentMode === 'AUTO' ? 'MANUAL' : 'AUTO';
  
  // Cập nhật UI ngay lập tức
  updatePumpMode('pump-mode-status', newMode);
  
  // Gửi RPC command đến CoreIOT
  const result = await sendRPCCommand('togglePumpMode', {});
  
  if (result) {
    if (result.mode) {
      updatePumpMode('pump-mode-status', result.mode);
    }
    if (result.state) {
      updatePumpStatus('pump-state-status', result.state);
    }
  } else {
    // Rollback nếu lỗi
    updatePumpMode('pump-mode-status', currentMode);
  }
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

async function updatePumpThresholds() {
  if (!COREIOT_CONFIG.isConfigured()) {
    alert('Vui lòng cấu hình CoreIOT trước!');
    return;
  }
  
  const minInput = document.getElementById('threshold-min-input');
  const maxInput = document.getElementById('threshold-max-input');
  if (!minInput || !maxInput) return;
  
  const min = parseFloat(minInput.value);
  const max = parseFloat(maxInput.value);
  
  if (isNaN(min) || isNaN(max)) {
    alert('Vui lòng nhập số hợp lệ cho min và max thresholds.');
    return;
  }
  
  if (min < 0 || max > 100 || min >= max) {
    alert('Giá trị không hợp lệ. Min phải < Max, và cả hai phải từ 0-100.');
    return;
  }
  
  // Gửi RPC command đến CoreIOT
  const result = await sendRPCCommand('setPumpThresholds', {
    min: min,
    max: max
  });
  
  if (result && result.success) {
    const thresholdCurrent = document.getElementById('threshold-current');
    if (thresholdCurrent) {
      thresholdCurrent.textContent = 'Min: ' + min + '% | Max: ' + max + '%';
    }
  } else {
    alert('Lỗi: ' + (result ? result.error : 'Không thể cập nhật thresholds'));
  }
}

async function loadPumpThresholds() {
  if (!COREIOT_CONFIG.isConfigured()) return;
  
  const data = await fetchCoreIOTData();
  if (data && data.pump_threshold_min !== undefined && data.pump_threshold_max !== undefined) {
    const minInput = document.getElementById('threshold-min-input');
    const maxInput = document.getElementById('threshold-max-input');
    const thresholdCurrent = document.getElementById('threshold-current');
    
    if (minInput) minInput.value = data.pump_threshold_min;
    if (maxInput) maxInput.value = data.pump_threshold_max;
    if (thresholdCurrent) {
      thresholdCurrent.textContent = 'Min: ' + data.pump_threshold_min + '% | Max: ' + data.pump_threshold_max + '%';
    }
  }
}

function formatSoil(soilValue) {
  const soilInt = Math.floor(parseFloat(soilValue));
  return (soilInt < 10 ? '0' : '') + soilInt;
}

function toggleSidebar() {
  const sidebar = document.getElementById('sidebar');
  if (sidebar) {
    sidebar.classList.toggle('open');
  }
}

// Close sidebar when clicking outside on mobile
document.addEventListener('click', function(e) {
  const sidebar = document.getElementById('sidebar');
  const toggle = document.querySelector('.mobile-nav-toggle');
  if (window.innerWidth <= 768 && sidebar && toggle && !sidebar.contains(e.target) && !toggle.contains(e.target)) {
    sidebar.classList.remove('open');
  }
});

// Modal Functions
function showConfigModal() {
  const modal = document.getElementById('configModal');
  const serverInput = document.getElementById('serverInput');
  const tokenInput = document.getElementById('tokenInput');
  
  // Load current config
  if (COREIOT_CONFIG.isConfigured()) {
    serverInput.value = COREIOT_CONFIG.server || '';
    tokenInput.value = COREIOT_CONFIG.token || '';
  }
  
  modal.classList.add('show');
}

function hideConfigModal() {
  const modal = document.getElementById('configModal');
  modal.classList.remove('show');
}

function saveConfig() {
  const serverInput = document.getElementById('serverInput');
  const tokenInput = document.getElementById('tokenInput');
  
  const server = serverInput.value.trim();
  const token = tokenInput.value.trim();
  
  if (!server || !token) {
    alert('Vui lòng nhập đầy đủ Server URL và Token!');
    return;
  }
  
  // Validate server URL format
  if (!server.startsWith('http://') && !server.startsWith('https://')) {
    if (confirm('Server URL không có http:// hoặc https://. Thêm https:// tự động?')) {
      serverInput.value = 'https://' + server;
    } else {
      return;
    }
  }
  
  // Save config
  try {
    COREIOT_CONFIG.configure(serverInput.value.trim(), token);
    alert('Cấu hình đã được lưu thành công!');
    hideConfigModal();
    
    // Restart data updates with new config
    startDataUpdates();
    
    console.log('[Config] CoreIOT configuration saved:', {
      server: serverInput.value.trim(),
      token: token ? token.substring(0, 8) + '...' : 'empty'
    });
  } catch (error) {
    console.error('[Config] Error saving configuration:', error);
    alert('Lỗi khi lưu cấu hình: ' + error.message);
  }
}

function clearConfig() {
  if (confirm('Bạn có chắc muốn xóa cấu hình CoreIOT?')) {
    try {
      COREIOT_CONFIG.configure('', '');
      localStorage.removeItem('coreiot_server');
      localStorage.removeItem('coreiot_token');
      
      alert('Đã xóa cấu hình CoreIOT!');
      hideConfigModal();
      
      // Update UI to show disconnected state
      const dot = document.getElementById('source-dot');
      const text = document.getElementById('source-text');
      if (dot) dot.className = 'source-dot';
      if (text) text.textContent = 'Not Configured';
      
      console.log('[Config] CoreIOT configuration cleared');
    } catch (error) {
      console.error('[Config] Error clearing configuration:', error);
      alert('Lỗi khi xóa cấu hình: ' + error.message);
    }
  }
}

