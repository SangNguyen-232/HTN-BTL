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
async function fetchCoreIOTData() {
  if (!COREIOT_CONFIG.isConfigured()) {
    console.error('CoreIOT chưa được cấu hình');
    return null;
  }
  
  try {
    // Lấy telemetry mới nhất
    const telemetryUrl = COREIOT_CONFIG.getUrl('telemetry');
    const telemetryResponse = await fetch(telemetryUrl);
    
    if (!telemetryResponse.ok) {
      throw new Error(`HTTP error! status: ${telemetryResponse.status}`);
    }
    
    const telemetryData = await telemetryResponse.json();
    
    // Lấy attributes (pump state, mode)
    const attributesUrl = COREIOT_CONFIG.getUrl('attributes');
    const attributesResponse = await fetch(attributesUrl);
    let attributesData = {};
    
    if (attributesResponse.ok) {
      attributesData = await attributesResponse.json();
    }
    
    // Merge dữ liệu
    return {
      ...telemetryData,
      ...attributesData
    };
  } catch (error) {
    console.error('Error fetching CoreIOT data:', error);
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

