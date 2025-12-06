// CoreIOT API Configuration
const COREIOT_CONFIG = {
  // Lấy từ localStorage hoặc prompt user nhập
  server: localStorage.getItem('coreiot_server') || 'app.coreiot.io',
  token: localStorage.getItem('coreiot_token') || '',
  
  // API endpoints của CoreIOT (ThingsBoard format)
  // Format: http://{server}/api/v1/{token}/keys/timeseries
  endpoints: {
    // Lấy telemetry mới nhất (ThingsBoard format)
    telemetry: '/api/v1/{token}/keys/timeseries',
    // Lấy attributes (pump state, mode, etc)
    attributes: '/api/v1/{token}/attributes',
    // Gửi RPC commands (điều khiển pump, LED)
    rpc: '/api/v1/{token}/rpc/twoway',
    // Lấy latest telemetry values
    latest: '/api/v1/{token}/keys/timeseries/latest'
  },
  
  // Helper function để build URL
  getUrl: function(endpoint) {
    // Sử dụng https nếu không có protocol
    const protocol = this.server.startsWith('http') ? '' : 'https://';
    const url = `${protocol}${this.server}${this.endpoints[endpoint].replace('{token}', this.token)}`;
    return url;
  },
  
  // Lưu config
  save: function() {
    localStorage.setItem('coreiot_server', this.server);
    localStorage.setItem('coreiot_token', this.token);
  },
  
  // Kiểm tra đã config chưa
  isConfigured: function() {
    return this.server && this.token && this.server.length > 0 && this.token.length > 0;
  },
  
  // Cấu hình server và token
  configure: function(server, token) {
    this.server = server || '';
    this.token = token || '';
    this.save();
  }
};

// Auto-load configuration từ localStorage
(function() {
  const savedServer = localStorage.getItem('coreiot_server');
  const savedToken = localStorage.getItem('coreiot_token');
  
  if (savedServer && savedToken) {
    try {
      COREIOT_CONFIG.server = savedServer;
      COREIOT_CONFIG.token = savedToken;
      console.log('[Config] Loaded CoreIOT configuration from localStorage');
    } catch (error) {
      console.warn('[Config] Failed to load saved configuration:', error);
    }
  }
})();

// Function để lấy dữ liệu từ CoreIOT
let lastErrorTime = 0;
let errorCount = 0;
const ERROR_LOG_INTERVAL = 5000; // Chỉ log error mỗi 5 giây

async function fetchCoreIOTData() {
  if (!COREIOT_CONFIG.isConfigured()) {
    return null;
  }
  
  try {
    // Thử lấy dữ liệu từ ThingsBoard API
    // Format: GET /api/v1/{accessToken}/keys/timeseries/latest?keys=temperature,humidity,soil_moisture_value,anomaly_score,anomaly_message
    const keys = 'temperature,humidity,soil_moisture_value,anomaly_score,anomaly_message';
    const protocol = COREIOT_CONFIG.server.startsWith('http') ? '' : 'https://';
    const telemetryUrl = `${protocol}${COREIOT_CONFIG.server}/api/v1/${COREIOT_CONFIG.token}/keys/timeseries/latest?keys=${keys}`;
    
    let telemetryData = null;
    let lastError = null;
    
    try {
      const telemetryResponse = await fetch(telemetryUrl, {
        method: 'GET',
        headers: {
          'Accept': 'application/json'
        }
      });
      
      if (telemetryResponse.ok) {
        telemetryData = await telemetryResponse.json();
        errorCount = 0; // Reset error count khi thành công
      } else {
        lastError = `HTTP ${telemetryResponse.status}`;
        
        // Thử fallback với format khác
        const fallbackUrl = `${protocol}${COREIOT_CONFIG.server}/api/v1/${COREIOT_CONFIG.token}/keys/timeseries?keys=${keys}&startTs=0&endTs=${Date.now()}&limit=1&agg=NONE`;
        const fallbackResponse = await fetch(fallbackUrl);
        if (fallbackResponse.ok) {
          const fallbackData = await fallbackResponse.json();
          // Chuyển đổi format từ time-series sang latest values
          telemetryData = {};
          Object.keys(fallbackData).forEach(key => {
            if (fallbackData[key] && fallbackData[key].length > 0) {
              telemetryData[key] = fallbackData[key][0].value;
            }
          });
          errorCount = 0;
        }
      }
    } catch (err) {
      lastError = err.message;
    }
    
    if (!telemetryData) {
      // Tất cả format đều fail, log error (giới hạn frequency)
      const now = Date.now();
      if (now - lastErrorTime > ERROR_LOG_INTERVAL) {
        errorCount++;
        console.warn(`[CoreIOT] Không thể lấy dữ liệu (lần ${errorCount}). Vui lòng kiểm tra lại Server và Token.`, lastError || 'Unknown error');
        lastErrorTime = now;
      }
      return null;
    }
    
    // Lấy attributes (pump state, mode) - optional
    let attributesData = {};
    try {
      const attributesUrl = `${protocol}${COREIOT_CONFIG.server}/api/v1/${COREIOT_CONFIG.token}/attributes?clientKeys=pump_state,pump_mode&sharedKeys=pump_state,pump_mode`;
      const attributesResponse = await fetch(attributesUrl);
      
      if (attributesResponse.ok) {
        const attrResponse = await attributesResponse.json();
        // Merge client và shared attributes
        if (attrResponse.client) {
          Object.assign(attributesData, attrResponse.client);
        }
        if (attrResponse.shared) {
          Object.assign(attributesData, attrResponse.shared);
        }
      }
    } catch (err) {
      // Attributes là optional, không cần log error
    }
    
    // Xử lý và merge dữ liệu
    const processedData = {};
    
    if (telemetryData) {
      // Convert ThingsBoard format to simple key-value
      Object.keys(telemetryData).forEach(key => {
        if (telemetryData[key] && typeof telemetryData[key] === 'object' && telemetryData[key].value !== undefined) {
          processedData[key] = telemetryData[key].value;
        } else {
          processedData[key] = telemetryData[key];
        }
      });
      
      // Map soil_moisture_value to soil for compatibility
      if (processedData.soil_moisture_value !== undefined) {
        processedData.soil = processedData.soil_moisture_value;
      }
    }
    
    // Merge với attributes data
    return {
      ...processedData,
      ...attributesData
    };
  } catch (error) {
    const now = Date.now();
    if (now - lastErrorTime > ERROR_LOG_INTERVAL) {
      errorCount++;
      console.warn(`[CoreIOT] Lỗi kết nối (lần ${errorCount}):`, error.message);
      lastErrorTime = now;
    }
    return null;
  }
}

// Function để gửi RPC command đến CoreIOT
async function sendRPCCommand(method, params) {
  if (!COREIOT_CONFIG.isConfigured()) {
    console.error('CoreIOT chưa được cấu hình');
    return null;
  }
  
  try {
    const rpcUrl = COREIOT_CONFIG.getUrl('rpc');
    const response = await fetch(rpcUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        method: method,
        params: params
      })
    });
    
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    
    return await response.json();
  } catch (error) {
    console.error('Error sending RPC command:', error);
    return null;
  }
}

