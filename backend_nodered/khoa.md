# Backend API Endpoints - Node-RED Gateway

  

**Node-RED Middleware: MQTT ↔ REST API Gateway**  

Last Updated: December 29, 2025

  

---

  

## Architecture Overview

  

```

┌─────────┐  MQTT   ┌──────────┐  REST API  ┌──────────┐

│  ESP32  │◄───────►│ Node-RED │◄──────────►│ Frontend │

│         │         │ Gateway  │            │  React   │

└─────────┘         └────┬─────┘            └──────────┘

                         │

                         │ Supabase Client

                         ▼

                   ┌──────────┐

                   │ Supabase │

                   │ Database │

                   └──────────┘

```

  

**Node-RED Vai Trò:**

1. **MQTT Subscriber**: Nhận events từ ESP32 → Lưu vào Supabase

2. **REST API Server**: Expose endpoints cho Frontend đọc/ghi dữ liệu

3. **MQTT Publisher**: Nhận commands từ Frontend → Publish đến ESP32

4. **Data Transformer**: Format data giữa MQTT ↔ REST ↔ Database

  

---

  

## Base URL

  

```

http://localhost:1880/api

```

  

Hoặc khi deploy:

```

https://your-nodered-instance.com/api

```

  

---

  

## Table of Contents

  

1. [Data Flow Overview](#data-flow-overview)

2. [MQTT Topics Reference](#mqtt-topics-reference)

3. [Events API](#events-api)

4. [Voice Notes API](#voice-notes-api)

5. [Sensor Logs API](#sensor-logs-api)

6. [Device Settings API](#device-settings-api)

7. [Command API](#command-api)

8. [Error Responses](#error-responses)

  

---

  

## Data Flow Overview

  

### Flow 1: ESP32 → Frontend (Read Data)

  

```

ESP32 chụp ảnh khi nhấn nút

    │

    ▼ MQTT Publish

Topic: doorbell/evt/button

Payload: { "image_url": "https://...", "timestamp": 1735488600 }

    │

    ▼ Node-RED subscribes

[MQTT In] → [Parse JSON] → [Insert Supabase]

                            INSERT INTO events (...)

    │

    ▼ Data đã lưu

Frontend gọi REST API

    │

    ▼ HTTP Request

GET /api/events

    │

    ▼ Node-RED handles

[HTTP In] → [Query Supabase] → [Format JSON] → [HTTP Response]

            SELECT * FROM events

```

  

### Flow 2: Frontend → ESP32 (Send Commands)

  

```

Frontend muốn bật còi

    │

    ▼ HTTP Request

POST /api/commands/siren

Body: { "action": "on", "duration": 5 }

    │

    ▼ Node-RED handles

[HTTP In] → [Validate] → [Publish MQTT] → [HTTP Response]

                         Topic: doorbell/cmd/siren

                         Payload: { "action": "on", "duration": 5 }

    │

    ▼ MQTT delivered

ESP32 subscribes doorbell/cmd/siren → Bật còi 5 giây

```

  

---

  

## MQTT Topics Reference

  

### Node-RED sẽ **subscribe** các topics này để nhận data từ ESP32:
### 1. `doorbell/evt/button` (Flow 1)

ESP32 publish khi có người nhấn nút chuông

  

**Payload từ ESP32:**

```json

{

  "image_url": "https://xxx.supabase.co/.../1735488600_button.jpg",

  "timestamp": 1735488600

}

```

  

**Node-RED xử lý:**

```

[MQTT In: doorbell/evt/button]

    ↓

[Function: Parse payload]

    ↓

[Supabase Insert]

    INSERT INTO events (event_type, image_url, description)

    VALUES ('button_press', payload.image_url, 'Có khách nhấn chuông')

```

  

### 2. `doorbell/evt/pir` (Flow 2)

ESP32 publish khi PIR phát hiện chuyển động

  

**Payload từ ESP32:**

```json

{

  "images": [

    "https://xxx.supabase.co/.../1735489500_motion_1.jpg",

    "https://xxx.supabase.co/.../1735489500_motion_2.jpg",

    "https://xxx.supabase.co/.../1735489500_motion_3.jpg"

  ],

  "timestamp": 1735489500

}

```

  

**Node-RED xử lý:**

```

[MQTT In: doorbell/evt/pir]

    ↓

[Function: Parse payload]

    ↓

[Supabase Insert]

    INSERT INTO events (event_type, images, description)

    VALUES ('motion_detected', payload.images, 'Phát hiện chuyển động')

    ↓

[Check device_settings.alarm_enabled]

    ↓ if true

[MQTT Out: doorbell/cmd/siren]

    Publish { "action": "on", "duration": 5 }

```

  

### 3. `doorbell/evt/voice` (Flow 3)

ESP32 publish khi có ghi âm từ khách

  

**Payload từ ESP32:**

```json

{

  "audio_url": "https://xxx.supabase.co/.../1735490400_voice.wav",

  "duration_seconds": 15,

  "timestamp": 1735490400

}

```

  

**Node-RED xử lý:**

```

[MQTT In: doorbell/evt/voice]

    ↓

[Supabase Insert]

    INSERT INTO voice_notes (audio_url, duration_seconds)

    VALUES (payload.audio_url, payload.duration_seconds)

```

  

### 4. `doorbell/sensor/temp` (Flow 4)

ESP32 publish log nhiệt độ định kỳ (mỗi 5 phút)

  

**Payload từ ESP32:**

```json

{

  "temperature": 28.5,

  "humidity": 65.2,

  "timestamp": 1735490500

}

```

  

**Node-RED xử lý:**

```

[MQTT In: doorbell/sensor/temp]

    ↓

[Supabase Insert]

    INSERT INTO sensor_logs (temperature, humidity)

    VALUES (payload.temperature, payload.humidity)

```

  

### 5. `doorbell/evt/snapshot` (Flow 5)

ESP32 publish khi chụp ảnh thủ công (command từ Frontend)

  

**Payload từ ESP32:**

```json

{

  "command_id": "cmd_1735490600_snapshot",

  "image_url": "https://xxx.supabase.co/.../1735490600_snapshot.jpg",

  "timestamp": 1735490600

}

```

  

**Node-RED xử lý:**

```

[MQTT In: doorbell/evt/snapshot]

    ↓

[Supabase Insert]

    INSERT INTO events (event_type, image_url, description)

    VALUES ('snapshot', payload.image_url, 'Chụp ảnh thủ công')

```

  

---

  

### Node-RED sẽ **publish** các topics này để gửi commands đến ESP32:
### 1. `doorbell/cmd/snapshot`

Frontend yêu cầu chụp ảnh

  

**Node-RED publish:**

```json

{

  "command_id": "cmd_1735490600_snapshot",

  "timestamp": 1735490600

}

```

  

### 2. `doorbell/cmd/speak`

Frontend gửi audio message để ESP32 phát

  

**Node-RED publish:**

```json

{

  "command_id": "cmd_1735490700_speak",

  "audio_url": "https://xxx.supabase.co/.../message.wav",

  "volume": 80,

  "timestamp": 1735490700

}

```

  

### 3. `doorbell/cmd/siren`

Frontend bật/tắt còi

  

**Node-RED publish:**

```json

{

  "command_id": "cmd_1735490800_siren",

  "action": "on",

  "duration": 5,

  "timestamp": 1735490800

}

```

  

### 4. `doorbell/cmd/settings`

Node-RED notify ESP32 khi settings thay đổi

  

**Node-RED publish:**

```json

{

  "speaker_volume": 80,

  "pir_enabled": true,

  "alarm_enabled": false,

  "timestamp": 1735490900

}

```

  

---

  

## Table of Contents
1. [Events API](#events-api)

2. [Voice Notes API](#voice-notes-api)

3. [Sensor Logs API](#sensor-logs-api)

4. [Device Settings API](#device-settings-api)

5. [Command API](#command-api)

6. [Error Responses](#error-responses)

---
## Events API

### 1. Get All Events

  

Lấy danh sách các events (button press, motion, snapshot)

  

**Endpoint:** `GET /api/events`

  

**Query Parameters:**

```

?limit=20          # Số lượng records (default: 20, max: 100)

?offset=0          # Phân trang (default: 0)

?type=button_press # Filter theo loại (optional)

?unread=true       # Chỉ lấy chưa đọc (optional)

```

  

**Request Example:**

```bash

GET /api/events?limit=20&unread=true

```

  

**Response Success (200):**

```json

{

  "success": true,

  "data": [

    {

      "id": 123,

      "created_at": "2025-12-29T14:30:00.000Z",

      "event_type": "button_press",

      "image_url": "https://xxx.supabase.co/storage/v1/object/public/bell-images/1735488600_button.jpg",

      "images": null,

      "description": "Có khách nhấn chuông",

      "is_read": false,

      "metadata": null

    },

    {

      "id": 124,

      "created_at": "2025-12-29T14:45:00.000Z",

      "event_type": "motion_detected",

      "image_url": null,

      "images": [

        "https://xxx.supabase.co/.../1735489500_motion_1.jpg",

        "https://xxx.supabase.co/.../1735489500_motion_2.jpg",

        "https://xxx.supabase.co/.../1735489500_motion_3.jpg"

      ],

      "description": "Phát hiện chuyển động",

      "is_read": false,

      "metadata": {

        "pir_triggered": true,

        "alarm_activated": true

      }

    },

    {

      "id": 125,

      "created_at": "2025-12-29T15:00:00.000Z",

      "event_type": "snapshot",

      "image_url": "https://xxx.supabase.co/.../1735490400_snapshot.jpg",

      "images": null,

      "description": "Chụp ảnh thủ công",

      "is_read": true,

      "metadata": null

    }

  ],

  "total": 125,

  "limit": 20,

  "offset": 0

}

```

  

**Node-RED Flow Implementation:**

  

```

┌──────────────┐

│  HTTP In     │ GET /api/events

│  /api/events │

└──────┬───────┘

       │

       ▼

┌──────────────────┐

│  Function Node   │ Parse query params

│  "Parse Params"  │ msg.limit = req.query.limit || 20

└──────┬───────────┘ msg.offset = req.query.offset || 0

       │             msg.type = req.query.type

       │             msg.unread = req.query.unread === 'true'

       ▼

┌──────────────────┐

│ Supabase Node    │ Query: SELECT * FROM events

│ "Query Events"   │ WHERE ($type IS NULL OR event_type = $type)

└──────┬───────────┘   AND ($unread IS NULL OR is_read = false)

       │             ORDER BY created_at DESC

       │             LIMIT $limit OFFSET $offset

       ▼

┌──────────────────┐

│  Function Node   │ Format response:

│ "Format JSON"    │ return {

└──────┬───────────┘   payload: {

       │                 success: true,

       │                 data: msg.payload,

       │                 total: msg.total,

       │                 limit: msg.limit,

       │                 offset: msg.offset

       │               },

       │               statusCode: 200

       ▼             }

┌──────────────────┐

│  HTTP Response   │ Return JSON to Frontend

└──────────────────┘

```

  

**Function Node Code:**

  

```javascript

// Parse Params Node

const limit = parseInt(msg.req.query.limit) || 20;

const offset = parseInt(msg.req.query.offset) || 0;

const type = msg.req.query.type || null;

const unread = msg.req.query.unread === 'true';

  

msg.limit = Math.min(limit, 100); // Max 100

msg.offset = offset;

msg.type = type;

msg.unread = unread;

  

// Build query

let query = 'SELECT * FROM events WHERE 1=1';

const params = [];

  

if (type) {

    params.push(type);

    query += ` AND event_type = $${params.length}`;

}

  

if (unread) {

    query += ' AND is_read = false';

}

  

query += ' ORDER BY created_at DESC LIMIT $' + (params.length + 1) + ' OFFSET $' + (params.length + 2);

params.push(limit, offset);

  

msg.payload = { query, params };

return msg;

```

  

```javascript

// Format JSON Node

msg.statusCode = 200;

msg.payload = {

    success: true,

    data: msg.payload, // From Supabase query result

    total: msg.total || msg.payload.length,

    limit: msg.limit,

    offset: msg.offset

};

return msg;

```

  

---

  

### 2. Get Event by ID

  

Lấy chi tiết 1 event

  

**Endpoint:** `GET /api/events/:id`

  

**Request Example:**

```bash

GET /api/events/123

```

  

**Response Success (200):**

```json

{

  "success": true,

  "data": {

    "id": 123,

    "created_at": "2025-12-29T14:30:00.000Z",

    "event_type": "button_press",

    "image_url": "https://xxx.supabase.co/.../1735488600_button.jpg",

    "images": null,

    "description": "Có khách nhấn chuông",

    "is_read": false,

    "metadata": null

  }

}

```

  

**Response Error (404):**

```json

{

  "success": false,

  "error": "Event not found"

}

```

  

---

  

### 3. Mark Event as Read

  

Đánh dấu event đã đọc

  

**Endpoint:** `PATCH /api/events/:id/read`

  

**Request Body:** (optional)

```json

{

  "is_read": true

}

```

  

**Request Example:**

```bash

PATCH /api/events/123/read

Content-Type: application/json

  

{

  "is_read": true

}

```

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Event marked as read",

  "data": {

    "id": 123,

    "is_read": true,

    "updated_at": "2025-12-29T15:30:00.000Z"

  }

}

```

  

**Node-RED Flow Implementation:**

  

```

┌──────────────┐

│  HTTP In     │ PATCH /api/events/:id/read

└──────┬───────┘

       │

       ▼

┌──────────────────┐

│  Function Node   │ Extract event ID from URL

│  "Extract ID"    │ msg.eventId = msg.req.params.id

└──────┬───────────┘ msg.isRead = msg.payload.is_read || true

       │

       ▼

┌──────────────────┐

│ Supabase Node    │ UPDATE events

│ "Update Event"   │ SET is_read = $isRead

└──────┬───────────┘ WHERE id = $eventId

       │             RETURNING id, is_read

       ▼

┌──────────────────┐

│  Function Node   │ return {

│ "Format Response"│   payload: {

└──────┬───────────┘     success: true,

       │                 message: "Event marked as read",

       │                 data: msg.payload[0]

       │               },

       │               statusCode: 200

       ▼             }

┌──────────────────┐

│  HTTP Response   │

└──────────────────┘

```

  

---

  

### 4. Delete Event

  

Xóa event (nếu cần)

  

**Endpoint:** `DELETE /api/events/:id`

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Event deleted"

}

```

  

---

  

## Voice Notes API

  

### 1. Get All Voice Notes

  

Lấy danh sách tin nhắn thoại

  

**Endpoint:** `GET /api/voice-notes`

  

**Query Parameters:**

```

?limit=20       # Số lượng records

?offset=0       # Phân trang

?unlistened=true # Chỉ lấy chưa nghe

```

  

**Request Example:**

```bash

GET /api/voice-notes?unlistened=true

```

  

**Response Success (200):**

```json

{

  "success": true,

  "data": [

    {

      "id": 45,

      "created_at": "2025-12-29T14:30:00.000Z",

      "audio_url": "https://xxx.supabase.co/storage/v1/object/public/bell-audio/1735488600_voice.wav",

      "duration_seconds": 15,

      "transcription": null,

      "is_listened": false,

      "metadata": {

        "sample_rate": 16000,

        "format": "wav"

      }

    },

    {

      "id": 46,

      "created_at": "2025-12-29T15:00:00.000Z",

      "audio_url": "https://xxx.supabase.co/.../1735490400_voice.wav",

      "duration_seconds": 23,

      "transcription": null,

      "is_listened": false,

      "metadata": null

    }

  ],

  "total": 46,

  "limit": 20,

  "offset": 0

}

```

  

**Node-RED Flow:**

```

[HTTP In] → [Supabase Query] → [HTTP Response]

           ↓

    SELECT * FROM voice_notes

    WHERE ($unlistened IS NULL OR is_listened = false)

    ORDER BY created_at DESC

    LIMIT $limit OFFSET $offset

```

  

---

  

### 2. Mark Voice Note as Listened

  

Đánh dấu đã nghe

  

**Endpoint:** `PATCH /api/voice-notes/:id/listened`

  

**Request Body:**

```json

{

  "is_listened": true

}

```

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Voice note marked as listened",

  "data": {

    "id": 45,

    "is_listened": true

  }

}

```

  

---

  

### 3. Delete Voice Note

  

**Endpoint:** `DELETE /api/voice-notes/:id`

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Voice note deleted"

}

```

  

---

  

## Sensor Logs API

  

### 1. Get Sensor Logs

  

Lấy log nhiệt độ/độ ẩm

  

**Endpoint:** `GET /api/sensor-logs`

  

**Query Parameters:**

```

?hours=24        # Lấy log trong X giờ gần nhất (default: 24)

?limit=100       # Số lượng records (default: 100)

?type=temperature # Filter theo sensor_type

```

  

**Request Example:**

```bash

GET /api/sensor-logs?hours=24&limit=100

```

  

**Response Success (200):**

```json

{

  "success": true,

  "data": [

    {

      "id": 1234,

      "created_at": "2025-12-29T15:30:00.000Z",

      "sensor_type": "temperature",

      "temperature": 28.5,

      "humidity": 65.2,

      "metadata": null

    },

    {

      "id": 1235,

      "created_at": "2025-12-29T15:35:00.000Z",

      "sensor_type": "temperature",

      "temperature": 28.7,

      "humidity": 64.8,

      "metadata": null

    }

  ],

  "total": 288,

  "hours": 24

}

```

  

**Node-RED Flow:**

```

[HTTP In] → [Parse Params] → [Query DB] → [HTTP Response]

           ↓

    SELECT * FROM sensor_logs

    WHERE created_at > NOW() - INTERVAL '$hours hours'

    ORDER BY created_at DESC

    LIMIT $limit

```

  

---

  

### 2. Get Sensor Statistics

  

Lấy thống kê trung bình theo giờ (cho charts)

  

**Endpoint:** `GET /api/sensor-logs/stats`

  

**Query Parameters:**

```

?days=7          # Thống kê X ngày gần nhất (default: 7)

?interval=hour   # Nhóm theo hour/day (default: hour)

```

  

**Request Example:**

```bash

GET /api/sensor-logs/stats?days=7&interval=hour

```

  

**Response Success (200):**

```json

{

  "success": true,

  "data": [

    {

      "timestamp": "2025-12-29T15:00:00.000Z",

      "avg_temperature": 28.6,

      "min_temperature": 27.5,

      "max_temperature": 29.8,

      "avg_humidity": 65.0,

      "count": 12

    },

    {

      "timestamp": "2025-12-29T14:00:00.000Z",

      "avg_temperature": 28.2,

      "min_temperature": 27.8,

      "max_temperature": 29.2,

      "avg_humidity": 66.5,

      "count": 12

    }

  ],

  "interval": "hour",

  "days": 7

}

```

  

**Node-RED Query:**

```sql

SELECT

  date_trunc('hour', created_at) as timestamp,

  AVG(temperature) as avg_temperature,

  MIN(temperature) as min_temperature,

  MAX(temperature) as max_temperature,

  AVG(humidity) as avg_humidity,

  COUNT(*) as count

FROM sensor_logs

WHERE created_at > NOW() - INTERVAL '$days days'

GROUP BY timestamp

ORDER BY timestamp DESC

```

  

---

  

### 3. Get Latest Reading

  

Lấy giá trị mới nhất

  

**Endpoint:** `GET /api/sensor-logs/latest`

  

**Response Success (200):**

```json

{

  "success": true,

  "data": {

    "temperature": 28.5,

    "humidity": 65.2,

    "timestamp": "2025-12-29T15:30:00.000Z"

  }

}

```

  

**Node-RED Query:**

```sql

SELECT temperature, humidity, created_at as timestamp

FROM sensor_logs

ORDER BY created_at DESC

LIMIT 1

```

  

---

  

## Device Settings API

  

### 1. Get Settings

  

Lấy cài đặt hiện tại

  

**Endpoint:** `GET /api/settings`

  

**Response Success (200):**

```json

{

  "success": true,

  "data": {

    "id": 1,

    "updated_at": "2025-12-29T15:30:00.000Z",

    "alarm_enabled": false,

    "do_not_disturb": false,

    "speaker_volume": 80,

    "pir_enabled": true,

    "notifications_enabled": true,

    "firmware_version": "v1.0.0",

    "last_sync": "2025-12-29T15:25:00.000Z",

    "metadata": null

  }

}

```

  

**Node-RED Flow:**

```

[HTTP In] → [Query DB] → [HTTP Response]

           ↓

    SELECT * FROM device_settings WHERE id = 1

```

  

---

  

### 2. Update Settings

  

Cập nhật cài đặt (partial update)

  

**Endpoint:** `PATCH /api/settings`

  

**Request Body:**

```json

{

  "alarm_enabled": true,

  "speaker_volume": 70,

  "do_not_disturb": false

}

```

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Settings updated",

  "data": {

    "id": 1,

    "updated_at": "2025-12-29T15:35:00.000Z",

    "alarm_enabled": true,

    "speaker_volume": 70,

    "do_not_disturb": false,

    "pir_enabled": true,

    "notifications_enabled": true,

    "firmware_version": "v1.0.0",

    "last_sync": "2025-12-29T15:25:00.000Z"

  }

}

```

  

**Node-RED Flow Implementation:**

  

```

┌──────────────┐

│  HTTP In     │ PATCH /api/settings

└──────┬───────┘

       │

       ▼

┌──────────────────┐

│  Function Node   │ Validate & extract fields

│  "Parse Body"    │ msg.updates = {

└──────┬───────────┘   alarm_enabled: payload.alarm_enabled,

       │               speaker_volume: payload.speaker_volume,

       │               pir_enabled: payload.pir_enabled,

       │               ...

       ▼             }

┌──────────────────┐

│ Supabase Node    │ UPDATE device_settings

│ "Update Settings"│ SET field1=$1, field2=$2, ...

└──────┬───────────┘ WHERE id = 1

       │             RETURNING *

       ▼

┌──────────────────┐

│  Function Node   │ Prepare MQTT payload

│ "Prep MQTT"      │ msg.mqttPayload = {

└──────┬───────────┘   speaker_volume: result.speaker_volume,

       │               pir_enabled: result.pir_enabled,

       │               alarm_enabled: result.alarm_enabled,

       │               timestamp: Date.now()

       ▼             }

┌──────────────────┐

│  MQTT Out Node   │ Topic: doorbell/cmd/settings

│ "Notify ESP32"   │ Payload: msg.mqttPayload

└──────┬───────────┘ QoS: 1, Retain: false

       │

       ▼

┌──────────────────┐

│  Function Node   │ return {

│ "Format Response"│   payload: {

└──────┬───────────┘     success: true,

       │                 message: "Settings updated",

       │                 data: msg.payload

       │               },

       │               statusCode: 200

       ▼             }

┌──────────────────┐

│  HTTP Response   │

└──────────────────┘

```

  

**⚠️ Quan trọng:** Sau khi update settings, Node-RED phải publish MQTT để ESP32 biết:

  

**MQTT Payload gửi đến ESP32:**

```

Topic: doorbell/cmd/settings

Payload:

{

  "alarm_enabled": true,

  "speaker_volume": 70,

  "pir_enabled": true,

  "timestamp": 1735490100

}

```

  

---

  

## Command API

  

Commands để điều khiển ESP32 từ Frontend

  

### 1. Take Snapshot

  

Chụp ảnh thủ công

  

**Endpoint:** `POST /api/commands/snapshot`

  

**Request Body:** (optional)

```json

{

  "description": "Chụp ảnh thử nghiệm"

}

```

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Snapshot command sent to ESP32",

  "command_id": "cmd_1735490500_snapshot"

}

```

  

**Node-RED Flow Implementation:**

  

```

┌──────────────┐

│  HTTP In     │ POST /api/commands/snapshot

└──────┬───────┘

       │

       ▼

┌──────────────────┐

│  Function Node   │ Generate command ID

│ "Generate ID"    │ const cmdId = 'cmd_' + Date.now() + '_snapshot'

└──────┬───────────┘ msg.commandId = cmdId

       │             msg.mqttPayload = {

       │               command_id: cmdId,

       │               timestamp: Date.now() / 1000

       ▼             }

┌──────────────────┐

│  MQTT Out Node   │ Topic: doorbell/cmd/snapshot

│ "Send to ESP32"  │ Payload: msg.mqttPayload

└──────┬───────────┘ QoS: 1, Retain: false

       │

       ▼

┌──────────────────┐

│  Function Node   │ return {

│ "Format Response"│   payload: {

└──────┬───────────┘     success: true,

       │                 message: "Snapshot command sent to ESP32",

       │                 command_id: msg.commandId

       │               },

       │               statusCode: 200

       ▼             }

┌──────────────────┐

│  HTTP Response   │

└──────────────────┘

```

  

**MQTT Command gửi đến ESP32:**

```

Topic: doorbell/cmd/snapshot

Payload:

{

  "command_id": "cmd_1735490500_snapshot",

  "timestamp": 1735490500

}

```

  

**ESP32 Response (sẽ được xử lý bởi MQTT subscriber khác):**

  

Node-RED cũng cần subscribe topic này để nhận kết quả:

  

```

┌──────────────────┐

│  MQTT In Node    │ Subscribe: doorbell/evt/snapshot

│ "ESP32 Response" │

└──────┬───────────┘

       │ Receives:

       │ {

       │   "command_id": "cmd_1735490500_snapshot",

       │   "image_url": "https://...",

       │   "timestamp": 1735490502

       ▼ }

┌──────────────────┐

│  Function Node   │ Parse payload

│  "Parse Result"  │

└──────┬───────────┘

       │

       ▼

┌──────────────────┐

│ Supabase Node    │ INSERT INTO events

│ "Save Event"     │ (event_type, image_url, description)

└──────────────────┘ VALUES ('snapshot', payload.image_url,

                             'Chụp ảnh thủ công')

```

  

---

  

### 2. Send Audio Message

  

Gửi tin nhắn audio đến ESP32 để phát qua loa

  

**Endpoint:** `POST /api/commands/speak`

  

**Request Body:**

```json

{

  "audio_url": "https://xxx.supabase.co/storage/v1/object/public/bell-audio/1735490600_message.wav",

  "message_type": "predefined",

  "message_text": "Vui lòng đợi",

  "volume": 80

}

```

  

**Fields:**

- `audio_url` (required): URL file audio đã upload

- `message_type` (optional): `predefined`, `custom`, `recorded`

- `message_text` (optional): Text mô tả

- `volume` (optional): Âm lượng 0-100 (default: current setting)

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Audio playback command sent to ESP32",

  "command_id": "cmd_1735490600_speak"

}

```

  

**Node-RED Flow Implementation:**

  

```

┌──────────────┐

│  HTTP In     │ POST /api/commands/speak

└──────┬───────┘

       │

       ▼

┌──────────────────┐

│  Function Node   │ Validate audio_url

│ "Validate Input" │ if (!payload.audio_url) throw error

└──────┬───────────┘ msg.volume = payload.volume || 80

       │

       ▼

┌──────────────────┐

│  Function Node   │ const cmdId = 'cmd_' + Date.now() + '_speak'

│ "Generate ID"    │ msg.mqttPayload = {

└──────┬───────────┘   command_id: cmdId,

       │               audio_url: payload.audio_url,

       │               volume: msg.volume,

       │               timestamp: Date.now() / 1000

       ▼             }

┌──────────────────┐

│  MQTT Out Node   │ Topic: doorbell/cmd/speak

│ "Send to ESP32"  │ Payload: msg.mqttPayload

└──────┬───────────┘ QoS: 1

       │

       ▼

┌──────────────────┐

│  Function Node   │ return {

│ "Format Response"│   payload: {

└──────┬───────────┘     success: true,

       │                 message: "Audio playback command sent",

       │                 command_id: cmdId

       │               },

       │               statusCode: 200

       ▼             }

┌──────────────────┐

│  HTTP Response   │

└──────────────────┘

```

  

**MQTT Command gửi đến ESP32:**

```

Topic: doorbell/cmd/speak

Payload:

{

  "command_id": "cmd_1735490600_speak",

  "audio_url": "https://xxx.supabase.co/.../1735490600_message.wav",

  "volume": 80,

  "timestamp": 1735490600

}

```

  

**ESP32 Response Handling:**

  

```

┌──────────────────┐

│  MQTT In Node    │ Subscribe: doorbell/evt/speak_status

│ "Playback Status"│

└──────┬───────────┘

       │ Receives:

       │ {

       │   "command_id": "cmd_1735490600_speak",

       │   "status": "completed",  // or "error"

       │   "error": null,

       ▼   "timestamp": 1735490605

┌──────────────────┐ }

│  Function Node   │ Log status (optional)

│  "Log Status"    │ Or notify Frontend via WebSocket

└──────────────────┘

```

  

---

  

### 3. Control Siren/Alarm

  

Bật/tắt còi báo động

  

**Endpoint:** `POST /api/commands/siren`

  

**Request Body:**

```json

{

  "action": "on",

  "duration": 5

}

```

  

**Fields:**

- `action` (required): `on` hoặc `off`

- `duration` (optional): Số giây bật còi (default: 5, max: 30)

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Siren command sent to ESP32",

  "command_id": "cmd_1735490700_siren",

  "action": "on",

  "duration": 5

}

```

  

**Node-RED Flow:**

```

[HTTP In] → [Validate Action] → [Publish MQTT] → [Update Settings] → [HTTP Response]

           ↓                                      ↓

    POST /api/commands/siren         UPDATE device_settings

           ↓                         SET alarm_enabled = true

    Topic: doorbell/cmd/siren        WHERE id = 1

    Payload:

    {

      "action": "on",

      "duration": 5

    }

```

  

**MQTT Details:**

- **Topic:** `doorbell/cmd/siren`

- **Payload:**

  ```json

  {

    "command_id": "cmd_1735490700_siren",

    "action": "on",

    "duration": 5,

    "timestamp": 1735490700

  }

  ```

  

**Expected Response from ESP32:**

```

Topic: doorbell/evt/siren_status

Payload:

{

  "command_id": "cmd_1735490700_siren",

  "status": "active",  // or "completed", "error"

  "timestamp": 1735490700

}

```

  

---

  

### 4. Update Volume

  

Cập nhật âm lượng loa

  

**Endpoint:** `POST /api/commands/volume`

  

**Request Body:**

```json

{

  "volume": 75

}

```

  

**Response Success (200):**

```json

{

  "success": true,

  "message": "Volume updated",

  "volume": 75

}

```

  

**Node-RED Flow:**

```

[HTTP In] → [Update DB] → [Publish MQTT] → [HTTP Response]

           ↓

    UPDATE device_settings

    SET speaker_volume = $volume

    WHERE id = 1

           ↓

    Topic: doorbell/cmd/volume

    Payload: { "volume": 75 }

```

  

---

  

## Error Responses

  

### Standard Error Format

  

Tất cả errors đều trả về format:

  

```json

{

  "success": false,

  "error": "Error message",

  "code": "ERROR_CODE",

  "details": {}

}

```

  

### HTTP Status Codes

  

- **200 OK** - Request thành công

- **201 Created** - Resource được tạo

- **400 Bad Request** - Request body invalid

- **404 Not Found** - Resource không tồn tại

- **500 Internal Server Error** - Lỗi server

  

### Common Errors

  

**400 - Validation Error:**

```json

{

  "success": false,

  "error": "Validation failed",

  "code": "VALIDATION_ERROR",

  "details": {

    "field": "speaker_volume",

    "message": "Volume must be between 0 and 100"

  }

}

```

  

**404 - Not Found:**

```json

{

  "success": false,

  "error": "Event not found",

  "code": "NOT_FOUND"

}

```

  

**500 - Server Error:**

```json

{

  "success": false,

  "error": "Database connection failed",

  "code": "DB_ERROR",

  "details": {

    "message": "Connection timeout"

  }

}

```

  

---

  

## Node-RED Implementation Guide
### Complete Node-RED Setup

#### 1. Install Required Nodes

  

```bash

# In Node-RED UI, go to Manage Palette → Install

node-red-contrib-supabase

node-red-dashboard (optional, for debugging UI)

```

  

Hoặc từ command line:

```bash

cd ~/.node-red

npm install node-red-contrib-supabase

```

  

#### 2. Configure Supabase Connection

  

Tạo **Config Node** cho Supabase:

- **Supabase URL:** `https://your-project.supabase.co`

- **Supabase Key:** Service Role Key (từ Supabase Dashboard → Settings → API)

  

#### 3. Configure MQTT Broker

  

Tạo **MQTT Broker Node**:

- **Server:** `aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud`

- **Port:** `8883` (TLS)

- **Use TLS:** ✅ Enabled

- **Username:** `your_hivemq_username`

- **Password:** `your_hivemq_password`

- **Client ID:** `nodered_gateway_001`

  

---

  

### Example: Complete Flow for "Get Events"

  

**Import JSON vào Node-RED:**

  

```json

[

  {

    "id": "http_get_events",

    "type": "http in",

    "name": "GET /api/events",

    "url": "/api/events",

    "method": "get",

    "wires": [["parse_events_params"]]

  },

  {

    "id": "parse_events_params",

    "type": "function",

    "name": "Parse Query Params",

    "func": "const limit = parseInt(msg.req.query.limit) || 20;\nconst offset = parseInt(msg.req.query.offset) || 0;\nconst type = msg.req.query.type || null;\nconst unread = msg.req.query.unread === 'true';\n\nmsg.limit = Math.min(limit, 100);\nmsg.offset = offset;\n\n// Build Supabase query\nlet query = { \n  table: 'events',\n  select: '*',\n  order: { column: 'created_at', ascending: false },\n  limit: msg.limit,\n  offset: msg.offset\n};\n\nif (type) query.eq = { event_type: type };\nif (unread) query.eq = { ...query.eq, is_read: false };\n\nmsg.payload = query;\nreturn msg;",

    "wires": [["supabase_query_events"]]

  },

  {

    "id": "supabase_query_events",

    "type": "supabase",

    "name": "Query Events from Supabase",

    "method": "select",

    "wires": [["format_events_response"]]

  },

  {

    "id": "format_events_response",

    "type": "function",

    "name": "Format JSON Response",

    "func": "msg.statusCode = 200;\nmsg.payload = {\n  success: true,\n  data: msg.payload,\n  total: msg.payload.length,\n  limit: msg.limit,\n  offset: msg.offset\n};\nreturn msg;",

    "wires": [["http_response_events"]]

  },

  {

    "id": "http_response_events",

    "type": "http response",

    "name": "Return Response",

    "wires": []

  }

]

```

  

---

  

### Example: MQTT Listener for Button Press Events

  

**Flow: ESP32 → MQTT → Node-RED → Supabase**

  

```json

[

  {

    "id": "mqtt_button_press",

    "type": "mqtt in",

    "name": "Listen: doorbell/evt/button",

    "topic": "doorbell/evt/button",

    "qos": "1",

    "broker": "hivemq_broker",

    "wires": [["parse_button_event"]]

  },

  {

    "id": "parse_button_event",

    "type": "function",

    "name": "Parse Button Event",

    "func": "const data = JSON.parse(msg.payload);\n\nmsg.payload = {\n  table: 'events',\n  insert: {\n    event_type: 'button_press',\n    image_url: data.image_url,\n    description: 'Có khách nhấn chuông',\n    is_read: false\n  }\n};\n\nreturn msg;",

    "wires": [["supabase_insert_event"]]

  },

  {

    "id": "supabase_insert_event",

    "type": "supabase",

    "name": "Insert Event to DB",

    "method": "insert",

    "wires": [["log_success"]]

  },

  {

    "id": "log_success",

    "type": "debug",

    "name": "Log: Event Saved",

    "wires": []

  }

]

```

  

---

  

### Example: Command Flow - Take Snapshot

  

**Flow: Frontend → HTTP → Node-RED → MQTT → ESP32**

  

```json

[

  {

    "id": "http_snapshot_cmd",

    "type": "http in",

    "name": "POST /api/commands/snapshot",

    "url": "/api/commands/snapshot",

    "method": "post",

    "wires": [["gen_snapshot_id"]]

  },

  {

    "id": "gen_snapshot_id",

    "type": "function",

    "name": "Generate Command ID",

    "func": "const cmdId = 'cmd_' + Date.now() + '_snapshot';\n\nmsg.commandId = cmdId;\nmsg.mqttTopic = 'doorbell/cmd/snapshot';\nmsg.mqttPayload = JSON.stringify({\n  command_id: cmdId,\n  timestamp: Math.floor(Date.now() / 1000)\n});\n\nreturn msg;",

    "wires": [["mqtt_publish_snapshot", "http_response_snapshot"]]

  },

  {

    "id": "mqtt_publish_snapshot",

    "type": "mqtt out",

    "name": "Publish to ESP32",

    "topic": "",

    "qos": "1",

    "broker": "hivemq_broker",

    "wires": []

  },

  {

    "id": "http_response_snapshot",

    "type": "function",

    "name": "Format Response",

    "func": "msg.statusCode = 200;\nmsg.payload = {\n  success: true,\n  message: 'Snapshot command sent to ESP32',\n  command_id: msg.commandId\n};\nreturn msg;",

    "wires": [["http_response_cmd"]]

  },

  {

    "id": "http_response_cmd",

    "type": "http response",

    "name": "Return Response",

    "wires": []

  }

]

```

  

---

  

### Error Handling Template

  

Thêm **Catch Node** cho mọi flow:

  

```json

[

  {

    "id": "catch_all_errors",

    "type": "catch",

    "name": "Catch All Errors",

    "scope": null,

    "wires": [["format_error_response"]]

  },

  {

    "id": "format_error_response",

    "type": "function",

    "name": "Format Error",

    "func": "msg.statusCode = msg.statusCode || 500;\nmsg.payload = {\n  success: false,\n  error: msg.error?.message || 'Internal server error',\n  code: msg.errorCode || 'INTERNAL_ERROR'\n};\nreturn msg;",

    "wires": [["http_error_response"]]

  },

  {

    "id": "http_error_response",

    "type": "http response",

    "name": "Return Error",

    "wires": []

  }

]

```

  

---

  

### Function Node Utilities

  

**Reusable Functions:**

  

```javascript

// Validate Required Fields

function validateRequired(payload, fields) {

  for (const field of fields) {

    if (!payload[field]) {

      throw new Error(`Missing required field: ${field}`);

    }

  }

}

  

// Generate Command ID

function generateCommandId(type) {

  return `cmd_${Date.now()}_${type}`;

}

  

// Format Success Response

function successResponse(data, message = null) {

  return {

    success: true,

    message: message,

    data: data

  };

}

  

// Format Error Response

function errorResponse(error, code = 'ERROR') {

  return {

    success: false,

    error: error,

    code: code

  };

}

```

  

---

  

### Testing Flows

  

**1. Test HTTP Endpoints:**

```bash

# Test GET events

curl http://localhost:1880/api/events?limit=5

  

# Test snapshot command

curl -X POST http://localhost:1880/api/commands/snapshot \

  -H "Content-Type: application/json"

```

  

**2. Test MQTT:**

Dùng MQTT client (MQTT Explorer, mosquitto_pub) để publish test messages:

```bash

mosquitto_pub -h aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud \

  -p 8883 -t doorbell/evt/button \

  -m '{"image_url":"https://test.jpg","timestamp":1735490000}' \

  -u username -P password --cafile ca.crt

```

  

**3. Monitor Debug:**

- Sử dụng Debug nodes để xem data flow

- Check Node-RED Debug panel (🐛 icon)

  

---

  

### Production Deployment

  

**1. Environment Variables:**

```bash

# ~/.node-red/settings.js

module.exports = {

  functionGlobalContext: {

    SUPABASE_URL: process.env.SUPABASE_URL,

    SUPABASE_KEY: process.env.SUPABASE_KEY,

    MQTT_BROKER: process.env.MQTT_BROKER,

    MQTT_USERNAME: process.env.MQTT_USERNAME,

    MQTT_PASSWORD: process.env.MQTT_PASSWORD

  }

}

```

  

**2. Security:**

- ✅ Bật HTTPS cho Node-RED

- ✅ Thêm authentication (HTTP Basic Auth hoặc JWT)

- ✅ Rate limiting cho API endpoints

- ✅ Validate inputs trước khi xử lý

  

**3. Logging:**

```javascript

// Add to all critical flows

node.warn(`[${new Date().toISOString()}] Event received: ${JSON.stringify(msg.payload)}`);

```

  

---

  

## Testing with cURL

  

### Get Events

```bash

curl -X GET "http://localhost:1880/api/events?limit=10&unread=true"

```

  

### Mark Event as Read

```bash

curl -X PATCH "http://localhost:1880/api/events/123/read" \

  -H "Content-Type: application/json" \

  -d '{"is_read": true}'

```

  

### Update Settings

```bash

curl -X PATCH "http://localhost:1880/api/settings" \

  -H "Content-Type: application/json" \

  -d '{

    "alarm_enabled": true,

    "speaker_volume": 70

  }'

```

  

### Send Snapshot Command

```bash

curl -X POST "http://localhost:1880/api/commands/snapshot" \

  -H "Content-Type: application/json" \

  -d '{"description": "Test snapshot"}'

```

  

### Send Audio Message

```bash

curl -X POST "http://localhost:1880/api/commands/speak" \

  -H "Content-Type: application/json" \

  -d '{

    "audio_url": "https://xxx.supabase.co/.../message.wav",

    "message_type": "predefined",

    "volume": 80

  }'

```

  

---

  

## Frontend Integration Examples

  

### React/Axios Example

  

```javascript

import axios from 'axios';

  

const API_BASE = 'http://localhost:1880/api';

  

// Get events

const getEvents = async (unread = false) => {

  const response = await axios.get(`${API_BASE}/events`, {

    params: { limit: 20, unread }

  });

  return response.data;

};

  

// Mark as read

const markEventRead = async (eventId) => {

  const response = await axios.patch(`${API_BASE}/events/${eventId}/read`, {

    is_read: true

  });

  return response.data;

};

  

// Update settings

const updateSettings = async (settings) => {

  const response = await axios.patch(`${API_BASE}/settings`, settings);

  return response.data;

};

  

// Send snapshot command

const takeSnapshot = async () => {

  const response = await axios.post(`${API_BASE}/commands/snapshot`);

  return response.data;

};

  

// Send audio message

const sendAudioMessage = async (audioUrl, volume = 80) => {

  const response = await axios.post(`${API_BASE}/commands/speak`, {

    audio_url: audioUrl,

    volume

  });

  return response.data;

};

  

// Toggle siren

const toggleSiren = async (action, duration = 5) => {

  const response = await axios.post(`${API_BASE}/commands/siren`, {

    action, // 'on' or 'off'

    duration

  });

  return response.data;

};

```

  

---

  

## Security Considerations

  

1. **Authentication:** Thêm JWT token hoặc API key

2. **Rate Limiting:** Giới hạn số request/phút

3. **CORS:** Cấu hình CORS cho frontend domain

4. **Input Validation:** Validate tất cả inputs

5. **HTTPS:** Sử dụng HTTPS trong production

  

---

  

## References

  

- [Node-RED HTTP Documentation](https://nodered.org/docs/user-guide/nodes#http)

- [Supabase REST API](https://supabase.com/docs/guides/api)

- [MQTT Topics Documentation](./BACKEND_API_SPECIFICATION.md)

  

---

  

**Maintained by:** DoorBell Project Team  

**Last Review:** December 29, 2025