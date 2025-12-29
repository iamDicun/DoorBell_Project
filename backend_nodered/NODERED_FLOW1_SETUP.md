# Node-RED Flow 1 Setup - Button Press Event

## Tổng quan Flow 1
Khi user nhấn nút chuông:
1. ESP32 phát tiếng ding-dong
2. ESP32 chụp ảnh và upload lên Supabase Storage bucket `bell-images`
3. ESP32 publish MQTT: `doorbell/evt/button` với payload `{"image_url":"https://...","timestamp":1735488600}`
4. Node-RED subscribe MQTT → insert vào database table `events`
5. Frontend gọi REST API `GET /api/events` để hiển thị

---

## Bước 1: Cấu hình MQTT Broker trong Node-RED

### 1.1. Thêm MQTT Broker Node
1. Mở Node-RED: http://localhost:1880
2. Kéo node **mqtt in** vào workspace
3. Double-click node → Add new mqtt-broker
4. Cấu hình:
   - **Server**: `1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud`
   - **Port**: `8883` (MQTT over TLS)
   - **Protocol**: `MQTT V3.1.1`
   - **Use TLS**: ✅ Checked
   - **Client ID**: `nodered_backend` (hoặc để trống auto-generate)

### 1.2. Cấu hình Security
Tab **Security**:
- **Username**: `esp_doorbell`
- **Password**: `Hcmus123`

### 1.3. Cấu hình TLS
Tab **TLS Configuration** → Add new tls-config:
- **Name**: `HiveMQ TLS`
- **Verify server certificate**: ✅ Checked (hoặc bỏ nếu gặp lỗi cert)
- **Server Name**: `1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud`

Nhấn **Add** → **Done** → **Deploy**

---

## Bước 2: Tạo Flow xử lý MQTT `doorbell/evt/button`

### 2.1. Node Structure
```
[MQTT In] → [JSON Parser] → [Insert to DB] → [Debug]
```

### 2.2. Node 1: MQTT In
- **Topic**: `doorbell/evt/button`
- **QoS**: `1`
- **Output**: `a parsed JSON object`
- **Name**: `Button Press Event`

### 2.3. Node 2: Function - Parse & Prepare Data
Node: **function** (đặt tên: `Prepare Insert`)

```javascript
// Parse MQTT payload
const payload = msg.payload;

// Validate required fields
if (!payload.image_url || !payload.timestamp) {
    node.warn("Invalid payload: missing image_url or timestamp");
    return null;
}

// Prepare SQL query for Supabase
msg.query = {
    table: "events",
    data: {
        event_type: "button_press",
        image_url: payload.image_url,
        created_at: new Date(payload.timestamp * 1000).toISOString(),
        metadata: {
            source: "doorbell_button",
            raw_timestamp: payload.timestamp
        }
    }
};

return msg;
```

### 2.4. Node 3: Supabase Insert
Node: **http request** (đặt tên: `Insert to Supabase`)

**Method**: `POST`

**URL**: `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events`

**Headers**:
```json
{
    "apikey": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8",
    "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8",
    "Content-Type": "application/json",
    "Prefer": "return=representation"
}
```

**Body**: Tab **JSON**, chọn `msg.query.data`

**Function node trước http request**:
```javascript
msg.payload = msg.query.data;
msg.headers = {
    "apikey": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8",
    "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8",
    "Content-Type": "application/json",
    "Prefer": "return=representation"
};
return msg;
```

### 2.5. Node 4: Debug
Kết nối **http request** → **debug** để xem kết quả insert

---

## Bước 3: Tạo REST API cho Frontend `GET /api/events`

### 3.1. Node Structure
```
[HTTP In] → [Query Supabase] → [Format Response] → [HTTP Response]
```

### 3.2. Node 1: HTTP In
- **Method**: `GET`
- **URL**: `/api/events`
- **Name**: `GET Events`

### 3.3. Node 2: Function - Build Query
Node: **function** (đặt tên: `Build Query Params`)

```javascript
// Parse query parameters: ?limit=20&type=button_press
const limit = msg.req.query.limit || 50;
const eventType = msg.req.query.type || null;

// Build Supabase query URL
let url = "https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events?order=created_at.desc&limit=" + limit;

if (eventType) {
    url += "&event_type=eq." + eventType;
}

msg.url = url;
msg.headers = {
    "apikey": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8",
    "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8"
};

return msg;
```

### 3.4. Node 3: HTTP Request to Supabase
Node: **http request**
- **Method**: `GET`
- **URL**: `{{url}}` (từ msg.url)
- **Return**: `a parsed JSON object`

### 3.5. Node 4: Function - Format Response
Node: **function** (đặt tên: `Format JSON Response`)

```javascript
msg.payload = {
    success: true,
    data: msg.payload,
    count: msg.payload.length
};

msg.statusCode = 200;
return msg;
```

### 3.6. Node 5: HTTP Response
Node: **http response**
- Kết nối từ Format Response

---

## Bước 4: Import Flow JSON (Nhanh)

Copy đoạn JSON này vào Node-RED (Menu → Import → Clipboard):

```json
[
    {
        "id": "mqtt_button_in",
        "type": "mqtt in",
        "z": "flow1",
        "name": "Button Press Event",
        "topic": "doorbell/evt/button",
        "qos": "1",
        "datatype": "json",
        "broker": "mqtt_broker",
        "x": 150,
        "y": 100,
        "wires": [["prepare_insert"]]
    },
    {
        "id": "prepare_insert",
        "type": "function",
        "z": "flow1",
        "name": "Prepare Insert",
        "func": "const payload = msg.payload;\n\nif (!payload.image_url || !payload.timestamp) {\n    node.warn('Invalid payload');\n    return null;\n}\n\nmsg.payload = {\n    event_type: 'button_press',\n    image_url: payload.image_url,\n    created_at: new Date(payload.timestamp * 1000).toISOString(),\n    metadata: {\n        source: 'doorbell_button',\n        raw_timestamp: payload.timestamp\n    }\n};\n\nmsg.headers = {\n    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n    'Content-Type': 'application/json',\n    'Prefer': 'return=representation'\n};\n\nreturn msg;",
        "x": 370,
        "y": 100,
        "wires": [["insert_db"]]
    },
    {
        "id": "insert_db",
        "type": "http request",
        "z": "flow1",
        "name": "Insert to Supabase",
        "method": "POST",
        "url": "https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events",
        "x": 570,
        "y": 100,
        "wires": [["debug1"]]
    },
    {
        "id": "debug1",
        "type": "debug",
        "z": "flow1",
        "name": "DB Response",
        "x": 760,
        "y": 100
    },
    {
        "id": "http_get_events",
        "type": "http in",
        "z": "flow1",
        "name": "GET /api/events",
        "url": "/api/events",
        "method": "get",
        "x": 150,
        "y": 250,
        "wires": [["build_query"]]
    },
    {
        "id": "build_query",
        "type": "function",
        "z": "flow1",
        "name": "Build Query",
        "func": "const limit = msg.req.query.limit || 50;\nconst eventType = msg.req.query.type || null;\n\nlet url = 'https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events?order=created_at.desc&limit=' + limit;\n\nif (eventType) {\n    url += '&event_type=eq.' + eventType;\n}\n\nmsg.url = url;\nmsg.headers = {\n    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',\n    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8'\n};\n\nreturn msg;",
        "x": 340,
        "y": 250,
        "wires": [["query_db"]]
    },
    {
        "id": "query_db",
        "type": "http request",
        "z": "flow1",
        "name": "Query Supabase",
        "method": "GET",
        "url": "",
        "x": 530,
        "y": 250,
        "wires": [["format_response"]]
    },
    {
        "id": "format_response",
        "type": "function",
        "z": "flow1",
        "name": "Format Response",
        "func": "msg.payload = {\n    success: true,\n    data: msg.payload,\n    count: msg.payload.length\n};\nmsg.statusCode = 200;\nreturn msg;",
        "x": 730,
        "y": 250,
        "wires": [["http_response"]]
    },
    {
        "id": "http_response",
        "type": "http response",
        "z": "flow1",
        "name": "",
        "x": 930,
        "y": 250
    }
]
```

**Lưu ý**: Sau khi import, bạn cần thêm MQTT broker configuration (`mqtt_broker` ID) như bước 1.

---

## Bước 5: Test Flow

### 5.1. Test MQTT với MQTT Explorer hoặc ESP32
1. ESP32 publish: `doorbell/evt/button` với payload:
```json
{
    "image_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488600_button.jpg",
    "timestamp": 1735488600
}
```

2. Kiểm tra Node-RED Debug panel → Xem "DB Response" có status 201

3. Kiểm tra Supabase Table Editor → Table `events` có record mới

### 5.2. Test REST API
```bash
curl http://localhost:1880/api/events
```

Response:
```json
{
    "success": true,
    "data": [
        {
            "id": 1,
            "event_type": "button_press",
            "image_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735488600_button.jpg",
            "created_at": "2024-12-29T12:30:00Z",
            "metadata": {
                "source": "doorbell_button",
                "raw_timestamp": 1735488600
            }
        }
    ],
    "count": 1
}
```

---

## Troubleshooting

### 1. MQTT Connection Failed
- Check HiveMQ Cloud dashboard: Broker có running không?
- Verify credentials: `esp_doorbell / Hcmus123`
- Check port 8883 (TLS) có bị firewall block không

### 2. Supabase Insert 401 Unauthorized
- Verify `apikey` và `Authorization` Bearer token đúng
- Check Supabase RLS policies: Table `events` có `anon` insert permission chưa

### 3. Frontend CORS Error
- Node-RED cần thêm CORS headers. Thêm function node trước `http response`:
```javascript
msg.headers = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type"
};
return msg;
```

---

## Kết luận
Flow 1 hoàn tất! ESP32 → Supabase Storage → MQTT → Node-RED → Database → Frontend API.
