# Node-RED Flow 2.1 Setup - PIR Normal Motion Detection (Activity Log)

## Tổng quan Flow 2.1
Khi PIR phát hiện chuyển động bình thường (mỗi lần phát hiện):
1. ESP32 register PIR detection
2. ESP32 publish MQTT: `doorbell/evt/pir` với payload `{"event":"motion_detected","timestamp":xxx,"level":"normal"}`
3. Node-RED subscribe MQTT → insert vào database table `events` với event_type = `motion_detected`
4. Frontend gọi `GET /api/events` → Tab "Nhật ký" hiển thị "Phát hiện chuyển động"

**Lưu ý**: Flow 2.1 này KHÁC với Flow 2 (HIGH ALERT):
- **Flow 2.1**: Mỗi lần phát hiện → Ghi log đơn giản (không chụp ảnh)
- **Flow 2**: HIGH ALERT (4+ detections trong 20s) → Chụp 3 ảnh burst → Tab "Báo động"

---

## Bước 1: Tạo Flow xử lý MQTT `doorbell/evt/pir`

### 1.1. Node Structure
```
[MQTT In] → [Prepare Insert] → [Insert to DB] → [Debug]
```

### 1.2. Node 1: MQTT In
- **Topic**: `doorbell/evt/pir`
- **QoS**: `1`
- **Output**: `a parsed JSON object`
- **Name**: `PIR Motion Log`
- **Broker**: Same as Flow 1

### 1.3. Node 2: Function - Prepare Insert
Node: **function** (đặt tên: `Prepare Motion Log`)

```javascript
// Parse MQTT payload
const payload = msg.payload;

// Validate required fields
if (!payload.event || !payload.timestamp) {
    node.warn("Invalid payload: missing event or timestamp");
    return null;
}

// Prepare Supabase insert payload
msg.payload = {
    event_type: 'motion_detected',
    image_url: null,  // No image for normal motion detection
    created_at: new Date(payload.timestamp * 1000).toISOString(),
    metadata: {
        source: 'pir_sensor',
        raw_timestamp: payload.timestamp,
        level: payload.level || 'normal',
        device_id: payload.device_id,
        message: 'Phát hiện chuyển động'
    }
};

msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

return msg;
```

### 1.4. Node 3: HTTP Request - Insert to Supabase
Node: **http request** (đặt tên: `Insert Motion Log`)
- **Method**: `POST`
- **URL**: `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events`
- **Return**: `a parsed JSON object`

### 1.5. Node 4: Debug
Kết nối để xem kết quả insert

---

## Bước 2: Import Flow JSON (Nhanh)

Copy đoạn JSON này vào Node-RED (Menu → Import → Clipboard):

```json
[
    {
        "id": "mqtt_pir_log",
        "type": "mqtt in",
        "z": "flow2_1",
        "name": "PIR Motion Log",
        "topic": "doorbell/evt/pir",
        "qos": "1",
        "datatype": "json",
        "broker": "mqtt_broker",
        "x": 140,
        "y": 300,
        "wires": [["prepare_motion_log"]]
    },
    {
        "id": "prepare_motion_log",
        "type": "function",
        "z": "flow2_1",
        "name": "Prepare Motion Log",
        "func": "const payload = msg.payload;\n\nif (!payload.event || !payload.timestamp) {\n    node.warn('Invalid payload');\n    return null;\n}\n\nmsg.payload = {\n    event_type: 'motion_detected',\n    image_url: null,\n    created_at: new Date(payload.timestamp * 1000).toISOString(),\n    metadata: {\n        source: 'pir_sensor',\n        raw_timestamp: payload.timestamp,\n        level: payload.level || 'normal',\n        device_id: payload.device_id,\n        message: 'Phát hiện chuyển động'\n    }\n};\n\nmsg.headers = {\n    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n    'Content-Type': 'application/json',\n    'Prefer': 'return=representation'\n};\n\nreturn msg;",
        "x": 360,
        "y": 300,
        "wires": [["insert_motion_log"]]
    },
    {
        "id": "insert_motion_log",
        "type": "http request",
        "z": "flow2_1",
        "name": "Insert Motion Log",
        "method": "POST",
        "url": "https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events",
        "x": 590,
        "y": 300,
        "wires": [["debug_motion"]]
    },
    {
        "id": "debug_motion",
        "type": "debug",
        "z": "flow2_1",
        "name": "Motion Log Response",
        "x": 810,
        "y": 300
    }
]
```

---

## Bước 3: Test Flow 2.1

### 3.1. Test với MQTT Explorer
Publish message đến `doorbell/evt/pir`:
```json
{
    "event": "motion_detected",
    "timestamp": 1735488800,
    "level": "normal",
    "device_id": "ESP32_12345678"
}
```

### 3.2. Kiểm tra kết quả
1. **Node-RED Debug Panel**: Xem "Motion Log Response" với status 201
2. **Supabase Table Editor**: Table `events` có 1 record mới:
   ```
   | id | event_type      | image_url | created_at          | metadata           |
   |----|-----------------|-----------|---------------------|--------------------|
   | 20 | motion_detected | null      | 2024-12-29 13:00:00 | {"level":"normal"} |
   ```

3. **Frontend**: Tab "Nhật ký" hiển thị "Phát hiện chuyển động - 29/12/2024 13:00"

---

## Bước 4: Giải thích sự khác biệt Flow 2 vs Flow 2.1

### Comparison Table

| Feature | Flow 2.1 (Normal Motion) | Flow 2 (HIGH ALERT) |
|---------|--------------------------|---------------------|
| **Trigger** | Mỗi lần PIR detect (debounce 3s) | 4+ detections trong 20s |
| **Photos** | Không chụp ảnh | 3 ảnh burst |
| **MQTT Topic** | `doorbell/evt/pir` | `doorbell/evt/pir_alert` |
| **Payload** | `{"event":"motion_detected","timestamp":xxx,"level":"normal"}` | `{"image_urls":["..."],"count":3,"timestamp":xxx,"level":"high"}` |
| **Node-RED** | Direct insert 1 log | Split array → 3 inserts |
| **DB Records** | 1 record, no image | 3 records with images |
| **Frontend Tab** | Nhật ký | Báo động |
| **event_type** | `motion_detected` | `pir_motion` |
| **Frequency** | Cao (mỗi 3s nếu có chuyển động) | Thấp (chỉ khi suspicious) |

### Flow Diagram

```
PIR Sensor Detection
    ↓
registerPIRDetection() in ESP32
    ├─ Store in history buffer
    ├─ Check debounce (3s)
    └─ Publish MQTT: doorbell/evt/pir  ← Flow 2.1
           {"event":"motion_detected","level":"normal"}
           ↓
       Node-RED → Insert log → Frontend "Nhật ký"
    ↓
checkPIRAlertLevel() in main loop
    ├─ Count detections in last 20s
    └─ If >= 4 detections: HIGH ALERT  ← Flow 2
           ↓
       captureSecurityBurst()
           ↓
       Upload 3 photos to Supabase
           ↓
       Publish MQTT: doorbell/evt/pir_alert
           {"image_urls":["..."],"count":3,"level":"high"}
           ↓
       Node-RED → Split → 3 inserts → Frontend "Báo động"
```

---

## Bước 5: Update Frontend để hiển thị Motion Logs

Hiện tại Frontend đã có Tab "Nhật ký", chỉ cần đảm bảo fetch đúng:

```javascript
// LogsTab.jsx - Filter by event_type
const response = await axios.get('http://localhost:1880/api/events?type=motion_detected&limit=50');
```

Hoặc fetch tất cả events và filter trong UI.

---

## Troubleshooting

### 1. Quá nhiều motion logs (spam)
**Nguyên nhân**: PIR sensor quá nhạy hoặc debounce time quá ngắn

**Giải pháp**: Tăng `PIR_DETECTION_DEBOUNCE_MS` trong config.h:
```cpp
#define PIR_DETECTION_DEBOUNCE_MS 5000  // Increase from 3s to 5s
```

### 2. Không thấy motion logs trong Frontend
**Nguyên nhân**: Frontend filter sai event_type

**Kiểm tra**: 
```sql
SELECT * FROM events WHERE event_type='motion_detected' ORDER BY created_at DESC LIMIT 10;
```

### 3. HIGH ALERT không trigger dù có nhiều chuyển động
**Nguyên nhân**: `registerPIRDetection()` không được gọi từ event_manager

**Kiểm tra**: Serial monitor ESP32 có dòng `[PIR-REG] Detection registered`?

---

## Kết luận
Flow 2.1 hoàn tất! 

**Tóm tắt 2 flows PIR**:
- **Flow 2.1** (`doorbell/evt/pir`): Normal motion → Log only → Tab "Nhật ký"
- **Flow 2** (`doorbell/evt/pir_alert`): HIGH ALERT → 3 photos → Tab "Báo động"

Cả 2 flows hoạt động song song và độc lập!
