// Chart functionality
const canvas = document.getElementById('sensorChart');
const ctx = canvas.getContext('2d');

// Chart data
let chartData = {
  labels: [],
  temperature: [],
  humidity: [],
  soil: []
};
const maxDataPoints = 50;

// Hàm vẽ đường line với marker
function drawLine(dataArray, color, stepX, mapY, padding) {
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  
  for (let i = 0; i < dataArray.length; i++) {
    let x = padding + (i * stepX);
    let y = mapY(dataArray[i]);
    
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
  
  // Vẽ marker (chấm tròn) tại mỗi điểm dữ liệu
  for (let i = 0; i < dataArray.length; i++) {
    let x = padding + (i * stepX);
    let y = mapY(dataArray[i]);
    
    ctx.beginPath();
    ctx.fillStyle = color;
    ctx.arc(x, y, 3, 0, Math.PI * 2);
    ctx.fill();
    
    // Vẽ viền trắng cho marker
    ctx.beginPath();
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.8)';
    ctx.lineWidth = 1;
    ctx.arc(x, y, 3, 0, Math.PI * 2);
    ctx.stroke();
  }
}

// Hàm vẽ biểu đồ thủ công
function drawCustomChart() {
  const w = canvas.width;
  const h = canvas.height;
  const paddingTop = 20;
  const paddingBottom = 40;
  const paddingLeft = 45;
  const paddingRight = 20;
  const chartWidth = w - paddingLeft - paddingRight;
  const chartHeight = h - paddingTop - paddingBottom;

  ctx.clearRect(0, 0, w, h);
  
  // Vẽ lưới ngang và nhãn trục Y (0-100)
  ctx.strokeStyle = 'rgba(255,255,255,0.1)';
  ctx.lineWidth = 1;
  ctx.fillStyle = '#9ca3af';
  ctx.font = '11px Arial';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';

  // Vẽ được dồng 10 phần (0, 10, ..., 100)
  for(let i=0; i<=10; i++) {
    let y = paddingTop + (i * chartHeight / 10);
    let value = 100 - (i * 10);
    
    // Vẽ đường lưới ngang
    ctx.beginPath();
    ctx.moveTo(paddingLeft, y);
    ctx.lineTo(w - paddingRight, y);
    ctx.stroke();
    
    // Vẽ nhãn trục Y
    ctx.fillText(value.toString(), paddingLeft - 8, y);
  }
  
  // Vẽ trục Y
  ctx.beginPath();
  ctx.strokeStyle = 'rgba(255,255,255,0.3)';
  ctx.lineWidth = 1;
  ctx.moveTo(paddingLeft, paddingTop);
  ctx.lineTo(paddingLeft, h - paddingBottom);
  ctx.stroke();
  
  // Vẽ trục X
  ctx.beginPath();
  ctx.strokeStyle = 'rgba(255,255,255,0.3)';
  ctx.lineWidth = 1;
  ctx.moveTo(paddingLeft, h - paddingBottom);
  ctx.lineTo(w - paddingRight, h - paddingBottom);
  ctx.stroke();

  const mapY = (val) => {
     if(val > 100) val = 100;
     if(val < 0) val = 0;
     // Map giá trị 0-100 trực tiếp sang tọa độ Y
     // Giá trị 100 ở đỉnh (paddingTop), giá trị 0 ở đáy (paddingTop + chartHeight)
     return paddingTop + chartHeight - (val / 100) * chartHeight;
  };
  
  const len = chartData.labels.length;
  if (len < 2) return; 

  const stepX = chartWidth / (len - 1);

  // Vẽ 3 đường dữ liệu với marker
  drawLine(chartData.temperature, 'rgba(251, 191, 36, 0.95)', stepX, mapY, paddingLeft);
  drawLine(chartData.humidity, 'rgba(125, 211, 252, 0.95)', stepX, mapY, paddingLeft);
  drawLine(chartData.soil, 'rgba(74, 222, 128, 0.95)', stepX, mapY, paddingLeft);
  
  // Vẽ nhãn trục X (thời gian)
  ctx.fillStyle = '#9ca3af';
  ctx.font = '10px Arial';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  
  // Tính toán số nhãn cần hiển thị dựa trên refresh rate
  const targetLabels = 9;
  const labelInterval = Math.max(1, Math.floor(len / targetLabels));
  
  for(let i = 0; i < len; i += labelInterval) {
    if (i < chartData.labels.length) {
      let x = paddingLeft + (i * stepX);
      let timeLabel = chartData.labels[i];
      
      // Vẽ đường lưới dọc mờ
      ctx.beginPath();
      ctx.strokeStyle = 'rgba(255,255,255,0.05)';
      ctx.lineWidth = 1;
      ctx.moveTo(x, paddingTop);
      ctx.lineTo(x, h - paddingBottom);
      ctx.stroke();
      
      // Vẽ nhãn thời gian
      ctx.fillText(timeLabel, x, h - paddingBottom + 8);
    }
  }
  
  // Vẽ nhãn thời gian cuối cùng nếu chưa được vẽ
  if (len > 0) {
    let lastIndex = len - 1;
    let shouldShowLast = (lastIndex % labelInterval !== 0);
    
    if (shouldShowLast || lastIndex === 0) {
      let x = paddingLeft + (lastIndex * stepX);
      let prevLabelX = paddingLeft + ((lastIndex - labelInterval) * stepX);
      
      if (lastIndex === 0 || (x - prevLabelX) > 50) {
        ctx.fillText(chartData.labels[lastIndex], x, h - paddingBottom + 8);
      }
    }
  }
}

// Hàm resize canvas
function resizeCanvas() {
  const parent = canvas.parentElement;
  canvas.width = parent.clientWidth;
  canvas.height = parent.clientHeight;
  drawCustomChart(); 
}

// Hàm khởi tạo Canvas
function initChart() {
  resizeCanvas();
  window.addEventListener('resize', resizeCanvas);
}

// Cập nhật dữ liệu chart
function updateChart(temp, hum, soil) {
  const now = new Date();
  const hours = String(now.getHours()).padStart(2, '0');
  const minutes = String(now.getMinutes()).padStart(2, '0');
  const seconds = String(now.getSeconds()).padStart(2, '0');
  const timeLabel = hours + ':' + minutes + ':' + seconds;
  
  chartData.labels.push(timeLabel);
  chartData.temperature.push(parseFloat(temp));
  chartData.humidity.push(parseFloat(hum));
  chartData.soil.push(parseFloat(soil));
  
  // Logic giới hạn 50 điểm (Circular Buffer)
  if (chartData.labels.length > maxDataPoints) {
    chartData.labels.shift();
    chartData.temperature.shift();
    chartData.humidity.shift();
    chartData.soil.shift();
  }
  
  // Gọi hàm vẽ thủ công
  drawCustomChart();
}

