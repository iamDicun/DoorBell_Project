# Backend Node-RED - ESP32 Doorbell System

Node-RED middleware xử lý dữ liệu giữa ESP32, Supabase và Frontend.

---

## 📁 Cấu Trúc Thư Mục

```
backend_nodered/
├── flows.json                    # Node-RED flow definitions (QUAN TRỌNG)
├── package.json                  # Dependencies
├── settings.js                   # Node-RED settings
├── create_sensor_table.sql       # SQL script tạo bảng sensor_data
├── .config.*.json                # Node-RED runtime configs
├── lib/                          # Node-RED libraries
├── node_modules/                 # Dependencies
│
└── Setup Guides:
    ├── NODERED_FLOW1_SETUP.md    # Flow 1: Button Press → Images
    ├── NODERED_FLOW2_SETUP.md    # Flow 2: PIR Alert → Burst 3 Images
    ├── NODERED_FLOW2_1_SETUP.md  # Flow 2.1: PIR Motion → Log
    ├── NODERED_FLOW3_SETUP.md    # Flow 3: Voice Note Recording
    └── NODERED_FLOW4_SETUP.md    # Flow 4: Sensor Data Telemetry
```

---

## 🚀 Quick Start

### 1. Cài Đặt

```bash
cd backend_nodered
npm install
```

### 2. Khởi Động Node-RED

```bash
node-red --userDir .
```

Hoặc dùng script:
```bash
start-nodered.bat
```

### 3. Truy Cập

Mở browser: **http://localhost:1880**

### 4. Deploy Flows

Click nút **Deploy** (đỏ, góc trên phải)

---

## 🔄 Flows Overview

| Flow | MQTT Topic | Chức Năng | API Endpoint |
|------|------------|-----------|--------------|
| **Flow 1** | `doorbell/evt/button` | Button press → Images | `GET /api/events?type=button_press` |
| **Flow 2** | `doorbell/evt/pir_alert` | PIR burst 3 images | `GET /api/events?type=motion_detected` |
| **Flow 2.1** | `doorbell/evt/pir` | PIR motion log | Direct Supabase query |
| **Flow 3** | `doorbell/evt/voice` | Voice notes | `GET /api/events?type=voice_note` |
| **Flow 4** | `doorbell/sensor/temp` | Temperature data | `GET /api/sensors/latest?type=temperature` |

---

## 🌐 API Endpoints

### 1. GET /api/events

Lấy danh sách events từ database.

**Query Parameters**:
- `type` - Event type filter: `button_press`, `voice_note`, `motion_detected`
- `limit` - Số lượng records (default: 50)

**Example**:
```bash
curl http://localhost:1880/api/events?type=button_press&limit=20
```

**Response**:
```json
{
  "success": true,
  "data": [
    {
      "id": 1,
      "created_at": "2025-12-29T12:30:00Z",
      "event_type": "button_press",
      "image_url": "https://...supabase.co/.../image.jpg",
      "metadata": {...}
    }
  ],
  "count": 1
}
```

### 2. GET /api/sensors/latest

Lấy giá trị sensor mới nhất.

**Query Parameters**:
- `type` - Sensor type: `temperature`, `humidity` (default: `temperature`)

**Example**:
```bash
curl http://localhost:1880/api/sensors/latest?type=temperature
```

**Response**:
```json
{
  "success": true,
  "data": {
    "id": 1,
    "sensor_type": "temperature",
    "value": 28.5,
    "unit": "C",
    "created_at": "2025-12-29T12:30:00Z"
  },
  "timestamp": "2025-12-29T12:30:05Z"
}
```

---

## 🗄️ Database Setup

### Tạo Bảng sensor_data

```bash
# Chạy SQL script trong Supabase SQL Editor
cat create_sensor_table.sql
```

Hoặc copy paste vào Supabase Dashboard → SQL Editor.

---

## 🔧 Configuration

### MQTT Broker

File: `flows.json` → MQTT Broker Node

```json
{
  "broker": "1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud",
  "port": "8883",
  "tls": true,
  "clientid": "nodered_debug_123"
}
```

### Supabase

API Keys trong function nodes:

```javascript
msg.headers = {
  'apikey': 'YOUR_SUPABASE_ANON_KEY',
  'Authorization': 'Bearer YOUR_SUPABASE_ANON_KEY'
};
```

---

## 📚 Setup Guides

Chi tiết từng flow:

1. **[NODERED_FLOW1_SETUP.md](./NODERED_FLOW1_SETUP.md)** - Button Press Images
2. **[NODERED_FLOW2_SETUP.md](./NODERED_FLOW2_SETUP.md)** - PIR Alert Burst
3. **[NODERED_FLOW2_1_SETUP.md](./NODERED_FLOW2_1_SETUP.md)** - PIR Motion Log
4. **[NODERED_FLOW3_SETUP.md](./NODERED_FLOW3_SETUP.md)** - Voice Notes
5. **[NODERED_FLOW4_SETUP.md](./NODERED_FLOW4_SETUP.md)** - Sensor Telemetry

---

## 🐛 Troubleshooting

### Node-RED không start

```bash
# Check port 1880 đã được dùng chưa
netstat -ano | findstr :1880

# Kill process nếu cần
taskkill /PID <PID> /F
```

### MQTT Connection Failed

1. Check credentials trong MQTT broker config
2. Verify HiveMQ Cloud broker URL
3. Test với MQTT Explorer external

### API Returns 404

1. Verify flows đã deploy
2. Check Node-RED console có errors
3. Test trực tiếp: `curl http://localhost:1880/api/events`

### Database Insert Failed

1. Check Supabase API keys
2. Verify table tồn tại: `sensor_data`, `events`
3. Check RLS policies cho phép insert

---

## 📝 Notes

- **Port**: 1880 (default Node-RED)
- **Protocol**: HTTP (local), MQTTS (HiveMQ)
- **Database**: Supabase PostgreSQL
- **Node.js**: v22.17.0+
- **Node-RED**: v4.1.2

---

## 🔗 Related

- **ESP32 Firmware**: `../firmware-esp32/`
- **React Frontend**: `../frontend_react/`
- **Docs**: `../firmware-esp32/docs/BACKEND_API_SPECIFICATION.md`

---

**Status**: ✅ Production Ready
