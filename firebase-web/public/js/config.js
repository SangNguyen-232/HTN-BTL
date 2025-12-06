// CoreIOT API Configuration
const COREIOT_CONFIG = {
  // Lấy từ localStorage hoặc prompt user nhập
  server: localStorage.getItem('coreiot_server') || 'app.coreiot.io',
  token: localStorage.getItem('coreiot_token') || '',
  
  // API endpoints của CoreIOT
  // Format: http://{server}/api/v1/{token}/telemetry/latest
  endpoints: {
    // Lấy telemetry mới nhất
    telemetry: '/api/v1/{token}/telemetry/latest',
    // Lấy attributes (pump state, mode, etc)
    attributes: '/api/v1/{token}/attributes',
    // Gửi RPC commands (điều khiển pump, LED)
    rpc: '/api/v1/{token}/rpc'
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
  }
};

// Function để lấy dữ liệu từ CoreIOT
let lastErrorTime = 0;
let errorCount = 0;
const ERROR_LOG_INTERVAL = 5000; // Chỉ log error mỗi 5 giây

async function fetchCoreIOTData() {
  if (!COREIOT_CONFIG.isConfigured()) {
    return null;
  }
  
  try {
    // Thử nhiều format endpoint khác nhau
    const endpointFormats = [
      '/api/v1/{token}/telemetry/latest',
      '/api/v1/{token}/telemetry',
      '/api/plugins/telemetry/{token}/values/latest',
      '/api/plugins/telemetry/{token}/values'
    ];
    
    let telemetryData = null;
    let lastError = null;
    
    // Thử từng format endpoint
    for (const endpointFormat of endpointFormats) {
      try {
        const protocol = COREIOT_CONFIG.server.startsWith('http') ? '' : 'https://';
        const telemetryUrl = `${protocol}${COREIOT_CONFIG.server}${endpointFormat.replace('{token}', COREIOT_CONFIG.token)}`;
        
        const telemetryResponse = await fetch(telemetryUrl, {
          method: 'GET',
          headers: {
            'Accept': 'application/json'
          }
        });
        
        if (telemetryResponse.ok) {
          telemetryData = await telemetryResponse.json();
          errorCount = 0; // Reset error count khi thành công
          break; // Thành công, dừng thử các format khác
        } else if (telemetryResponse.status !== 404) {
          // Nếu không phải 404, có thể là lỗi khác (401, 403, etc)
          lastError = `HTTP ${telemetryResponse.status}`;
        }
      } catch (err) {
        lastError = err.message;
        continue; // Thử format tiếp theo
      }
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
      const attributesFormats = [
        '/api/v1/{token}/attributes',
        '/api/plugins/telemetry/{token}/attributes'
      ];
      
      for (const attrFormat of attributesFormats) {
        const protocol = COREIOT_CONFIG.server.startsWith('http') ? '' : 'https://';
        const attributesUrl = `${protocol}${COREIOT_CONFIG.server}${attrFormat.replace('{token}', COREIOT_CONFIG.token)}`;
        const attributesResponse = await fetch(attributesUrl);
        
        if (attributesResponse.ok) {
          attributesData = await attributesResponse.json();
          break;
        }
      }
    } catch (err) {
      // Attributes là optional, không cần log error
    }
    
    // Merge dữ liệu
    return {
      ...telemetryData,
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

