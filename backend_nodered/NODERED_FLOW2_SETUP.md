# Node-RED Flow 2 Setup - PIR Motion Detection

## Tổng quan Flow 2
Khi PIR phát hiện chuyển động:
1. ESP32 phát hiện chuyển động PIR (alert level HIGH)
2. ESP32 chụp 3 ảnh liên tiếp (burst mode) mỗi ảnh cách nhau 500ms
3. ESP32 upload từng ảnh lên Supabase Storage bucket `bell-images`
4. ESP32 publish MQTT: `doorbell/evt/pir` với payload:
```json
{
  "image_urls": [
    "https://.../timestamp_pir_1.jpg",
    "https://.../timestamp_pir_2.jpg",
    "https://.../timestamp_pir_3.jpg"
  ],
  "count": 3,
  "timestamp": 1735488700
}
```
5. Node-RED subscribe MQTT → insert 3 records vào database table `events`
6. Frontend gọi `GET /api/events` để hiển thị (filter type=pir_motion)

---

## Bước 1: Tạo Flow xử lý MQTT `doorbell/evt/pir`

### 1.1. Node Structure
```
[MQTT In] → [Parse URLs] → [Insert Each URL] → [Debug]
```

### 1.2. Node 1: MQTT In
- **Topic**: `doorbell/evt/pir`
- **QoS**: `1`
- **Output**: `a parsed JSON object`
- **Name**: `PIR Motion Event`
- **Broker**: (Dùng lại MQTT broker từ Flow 1)

### 1.3. Node 2: Function - Parse and Split URLs
Node: **function** (đặt tên: `Split Burst Images`)

```javascript
// Parse MQTT payload
const payload = msg.payload;

// Validate required fields
if (!payload.image_urls || !Array.isArray(payload.image_urls) || payload.image_urls.length === 0) {
    node.warn("Invalid payload: missing or empty image_urls array");
    return null;
}

const timestamp = payload.timestamp;
const baseTimestamp = new Date(timestamp * 1000).toISOString();

// Create separate message for each image URL
const messages = [];
for (let i = 0; i < payload.image_urls.length; i++) {
    const imageUrl = payload.image_urls[i];
    
    messages.push({
        payload: {
            event_type: 'pir_motion',
            image_url: imageUrl,
            created_at: baseTimestamp,
            metadata: {
                source: 'pir_sensor',
                burst_sequence: i + 1,
                total_burst: payload.count,
                raw_timestamp: timestamp
            }
        },
        headers: {
            'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
            'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
            'Content-Type': 'application/json',
            'Prefer': 'return=representation'
        }
    });
}

return [messages];
```

**Lưu ý**: Function này trả về array of messages, mỗi message sẽ insert 1 ảnh riêng vào database.

### 1.4. Node 3: HTTP Request to Supabase
Node: **http request** (đặt tên: `Insert to Supabase`)

- **Method**: `POST`
- **URL**: `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events`
- **Return**: `a parsed JSON object`

Kết nối từ output của Function node.

### 1.5. Node 4: Debug
Kết nối **http request** → **debug** để xem kết quả

---

## Bước 2: Update REST API `GET /api/events` để filter PIR events

Flow đã có từ Flow 1, chỉ cần test với query parameter `type=pir_motion`:

```bash
curl "http://localhost:1880/api/events?limit=20&type=pir_motion"
```

Response:
```json
{
  "success": true,
  "data": [
    {
      "id": 5,
      "event_type": "pir_motion",
      "image_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488700_pir_1.jpg",
      "created_at": "2024-12-29T13:45:00Z",
      "metadata": {
        "source": "pir_sensor",
        "burst_sequence": 1,
        "total_burst": 3
      }
    },
    {
      "id": 6,
      "event_type": "pir_motion",
      "image_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488700_pir_2.jpg",
      "created_at": "2024-12-29T13:45:00Z",
      "metadata": {
        "source": "pir_sensor",
        "burst_sequence": 2,
        "total_burst": 3
      }
    }
  ],
  "count": 2
}
```

---

## Bước 3: Import Flow JSON cho Flow 2

Copy JSON này vào Node-RED (Menu → Import → Clipboard):

```json
[
    {
        "id": "mqtt_pir_in",
        "type": "mqtt in",
        "z": "flow2",
        "name": "PIR Motion Event",
        "topic": "doorbell/evt/pir",
        "qos": "1",
        "datatype": "json",
        "broker": "mqtt_broker",
        "x": 150,
        "y": 200,
        "wires": [["split_images"]]
    },
    {
        "id": "split_images",
        "type": "function",
        "z": "flow2",
        "name": "Split Burst Images",
        "func": "const payload = msg.payload;\n\nif (!payload.image_urls || !Array.isArray(payload.image_urls) || payload.image_urls.length === 0) {\n    node.warn('Invalid payload: missing or empty image_urls');\n    return null;\n}\n\nconst timestamp = payload.timestamp;\nconst baseTimestamp = new Date(timestamp * 1000).toISOString();\n\nconst messages = [];\nfor (let i = 0; i < payload.image_urls.length; i++) {\n    const imageUrl = payload.image_urls[i];\n    \n    messages.push({\n        payload: {\n            event_type: 'pir_motion',\n            image_url: imageUrl,\n            created_at: baseTimestamp,\n            metadata: {\n                source: 'pir_sensor',\n                burst_sequence: i + 1,\n                total_burst: payload.count,\n                raw_timestamp: timestamp\n            }\n        },\n        headers: {\n            'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n            'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n            'Content-Type': 'application/json',\n            'Prefer': 'return=representation'\n        }\n    });\n}\n\nreturn [messages];",
        "outputs": 1,
        "x": 380,
        "y": 200,
        "wires": [["insert_db_pir"]]
    },
    {
        "id": "insert_db_pir",
        "type": "http request",
        "z": "flow2",
        "name": "Insert to Supabase",
        "method": "POST",
        "url": "https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events",
        "x": 590,
        "y": 200,
        "wires": [["debug_pir"]]
    },
    {
        "id": "debug_pir",
        "type": "debug",
        "z": "flow2",
        "name": "PIR DB Response",
        "x": 800,
        "y": 200
    }
]
```

**Lưu ý**: Cần link `mqtt_broker` config từ Flow 1.

---

## Bước 4: Test Flow 2

### 4.1. Test với MQTT Explorer
Publish message đến topic `doorbell/evt/pir`:

```json
{
  "image_urls": [
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488700_pir_1.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488700_pir_2.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488700_pir_3.jpg"
  ],
  "count": 3,
  "timestamp": 1735488700
}
```

### 4.2. Kiểm tra Database
1. Mở Supabase Table Editor → Table `events`
2. Filter `event_type = 'pir_motion'`
3. Xem có 3 records với `burst_sequence: 1, 2, 3`

### 4.3. Test REST API
```bash
curl "http://localhost:1880/api/events?type=pir_motion"
```

---

## Bước 5: Frontend Update để hiển thị PIR burst images

Frontend đã fetch tất cả events, chỉ cần thêm filter:

### Trong SecurityDashboard/index.jsx:
```javascript
// Filter events by type
const buttonPressEvents = events.filter(e => e.event_type === 'button_press');
const pirMotionEvents = events.filter(e => e.event_type === 'pir_motion');

// Group PIR burst images by timestamp
const groupedPirEvents = pirMotionEvents.reduce((acc, event) => {
  const timestamp = event.metadata?.raw_timestamp;
  if (!acc[timestamp]) {
    acc[timestamp] = [];
  }
  acc[timestamp].push(event);
  return acc;
}, {});
```

### Hiển thị burst images trong ImageGallery:
```jsx
{Object.entries(groupedPirEvents).map(([timestamp, burstImages]) => (
  <div key={timestamp} className="burst-container">
    <h4>🚨 PIR Motion - {new Date(timestamp * 1000).toLocaleString()}</h4>
    <div className="burst-grid">
      {burstImages.map(img => (
        <img key={img.id} src={img.image_url} alt={`Burst ${img.metadata.burst_sequence}`} />
      ))}
    </div>
  </div>
))}
```

---

## Khác biệt Flow 1 vs Flow 2

| Feature | Flow 1 (Button) | Flow 2 (PIR) |
|---------|----------------|--------------|
| Trigger | User nhấn nút | PIR phát hiện chuyển động |
| Photos | 1 ảnh | 3 ảnh (burst) |
| MQTT Topic | `doorbell/evt/button` | `doorbell/evt/pir` |
| Payload | `{"image_url":"...", "timestamp":xxx}` | `{"image_urls":["...","...","..."], "count":3, "timestamp":xxx}` |
| DB Records | 1 record | 3 records (burst_sequence: 1,2,3) |
| Event Type | `button_press` | `pir_motion` |

---

## Troubleshooting

### 1. Chỉ insert 1 ảnh thay vì 3
- Kiểm tra Function node `Split Burst Images` có return `[messages]` (array of messages)
- Kiểm tra output của function node có 3 messages riêng biệt

### 2. Supabase insert duplicate timestamp
- Đúng! PIR burst sẽ có cùng timestamp vì chụp cùng lúc
- Dùng `metadata.burst_sequence` để phân biệt

### 3. Frontend hiển thị lộn xộn
- Group images theo `metadata.raw_timestamp`
- Sort theo `metadata.burst_sequence` để hiển thị đúng thứ tự

---

## Kết luận
Flow 2 hoàn tất! ESP32 PIR detection → Burst 3 photos → Supabase Storage → MQTT → Node-RED → 3 DB records → Frontend API.
