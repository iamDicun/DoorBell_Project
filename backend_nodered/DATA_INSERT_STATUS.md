# Backend Flow Data Insert Status

## ✅ Kiểm Tra Hoàn Tất

Tất cả 5 flows đã được cấu hình insert dữ liệu vào Supabase.

---

## 📊 Flow Summary

| Flow | MQTT Topic | Insert Target | Table | Field chính |
|------|------------|---------------|-------|-------------|
| **Flow 1** | `doorbell/evt/button` | ✅ `/rest/v1/events` | `events` | `image_url` |
| **Flow 2** | `doorbell/evt/pir_alert` | ✅ `/rest/v1/events` | `events` | `image_url` (3 records) |
| **Flow 2.1** | `doorbell/evt/pir` | ✅ `/rest/v1/events` | `events` | `image_url: null` |
| **Flow 3** | `doorbell/evt/voice` | ✅ `/rest/v1/events` | `events` | `audio_url` |
| **Flow 4** | `doorbell/sensor/temp` | ✅ `/rest/v1/sensor_data` | `sensor_data` | `value` |

---

## 🔍 Chi Tiết Từng Flow

### Flow 1: Button Press → Images

**MQTT → Database Flow**:
```
ESP32 → Upload image to Supabase Storage → Get image_url
     → Publish MQTT doorbell/evt/button {image_url, timestamp}
     → Node-RED receives MQTT
     → prepare_insert function (add event_type, metadata)
     → insert_db HTTP POST /rest/v1/events
     → Supabase inserts record
```

**Database Record**:
```json
{
  "event_type": "button_press",
  "image_url": "https://...supabase.co/.../image.jpg",
  "created_at": "2025-12-29T12:30:00Z",
  "metadata": {
    "source": "doorbell_button",
    "raw_timestamp": 1735470000
  }
}
```

**Node-RED Nodes**:
- `mqtt_button_in` → Subscribe `doorbell/evt/button`
- `prepare_insert` → Format payload
- `insert_db` → POST to Supabase
- `debug1` → Log response

---

### Flow 2: PIR Alert → Burst 3 Images

**MQTT → Database Flow**:
```
ESP32 → Capture 3 images
     → Upload 3 images to Supabase Storage
     → Get 3 URLs
     → Publish MQTT doorbell/evt/pir_alert {images: [url1, url2, url3]}
     → Node-RED receives MQTT
     → split_images: Split array thành 3 messages
     → prepare_pir_insert: Format mỗi message
     → insert_db_pir: Insert 3 records riêng biệt
```

**Database Records** (3 records):
```json
// Record 1
{
  "event_type": "motion_detected",
  "image_url": "https://.../image1.jpg",
  "metadata": {
    "level": "high",
    "message": "Suspicious activity detected",
    "burst_index": 0
  }
}
// Record 2, 3 tương tự
```

**Node-RED Nodes**:
- `mqtt_pir_alert` → Subscribe `doorbell/evt/pir_alert`
- `split_images` → Split array
- `prepare_pir_insert` → Format each
- `insert_db_pir` → POST to Supabase (3 times)
- `debug_pir` → Log responses

---

### Flow 2.1: PIR Motion Log

**MQTT → Database Flow**:
```
ESP32 → PIR detects motion (normal level)
     → Publish MQTT doorbell/evt/pir {event, timestamp}
     → Node-RED receives MQTT
     → prepare_motion_log (no image_url, just log)
     → insert_motion_log POST /rest/v1/events
     → Supabase inserts record
```

**Database Record**:
```json
{
  "event_type": "motion_detected",
  "image_url": null,
  "created_at": "2025-12-29T12:30:00Z",
  "metadata": {
    "source": "pir_sensor",
    "level": "normal",
    "message": "Phát hiện chuyển động"
  }
}
```

**Node-RED Nodes**:
- `mqtt_pir_log` → Subscribe `doorbell/evt/pir`
- `prepare_motion_log` → Format payload (no image)
- `insert_motion_log` → POST to Supabase
- `debug_motion` → Log response

---

### Flow 3: Voice Note Recording

**MQTT → Database Flow**:
```
ESP32 → Record audio (WAV)
     → Upload audio to Supabase Storage (bell-audio bucket)
     → Get audio_url
     → Publish MQTT doorbell/evt/voice {audio_url, timestamp, duration_ms}
     → Node-RED receives MQTT
     → prepare_voice_insert (add event_type, metadata)
     → insert_voice_db POST /rest/v1/events
     → Supabase inserts record
```

**Database Record**:
```json
{
  "event_type": "voice_note",
  "audio_url": "https://.../voice_1735470000.wav",
  "description": "Voice note recorded",
  "created_at": "2025-12-29T12:30:00Z",
  "metadata": {
    "source": "doorbell_microphone",
    "device_id": "ESP32_DOORBELL",
    "duration_ms": 3000
  }
}
```

**Node-RED Nodes**:
- `mqtt_voice_in` → Subscribe `doorbell/evt/voice`
- `prepare_voice_insert` → Format payload
- `insert_voice_db` → POST to Supabase
- `debug_voice` → Log response

---

### Flow 4: Sensor Data Telemetry

**MQTT → Database Flow**:
```
ESP32 → Read temperature sensor
     → Publish MQTT doorbell/sensor/temp {value, unit, timestamp}
     → Node-RED receives MQTT
     → prepare_sensor_insert (format for sensor_data table)
     → insert_sensor_db POST /rest/v1/sensor_data
     → Supabase inserts record
```

**Database Record**:
```json
{
  "sensor_type": "temperature",
  "value": 28.5,
  "unit": "C",
  "created_at": "2025-12-29T12:30:00Z",
  "metadata": {
    "device_id": "ESP32_DOORBELL",
    "raw_timestamp": 1735470000
  }
}
```

**Node-RED Nodes**:
- `mqtt_sensor_in` → Subscribe `doorbell/sensor/temp`
- `prepare_sensor_insert` → Format payload
- `insert_sensor_db` → POST to Supabase `sensor_data` table
- `debug_sensor` → Log response

---

## 🗄️ Database Tables

### Table: `events`

Dùng cho Flow 1, 2, 2.1, 3

```sql
CREATE TABLE events (
    id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    event_type TEXT NOT NULL,
    image_url TEXT,
    audio_url TEXT,
    description TEXT,
    metadata JSONB
);
```

**Event Types**:
- `button_press` - Flow 1
- `motion_detected` - Flow 2, 2.1
- `voice_note` - Flow 3

### Table: `sensor_data`

Dùng cho Flow 4

```sql
CREATE TABLE sensor_data (
    id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    sensor_type TEXT NOT NULL,
    value NUMERIC NOT NULL,
    unit TEXT NOT NULL,
    metadata JSONB
);
```

**Sensor Types**:
- `temperature` - Flow 4
- (Future: `humidity`, `distance`, etc.)

---

## 📈 Data Flow Complete

```
┌────────────────────────────────────────────────┐
│           ESP32 Smart Doorbell                 │
│                                                │
│  Button → Upload Image → Supabase Storage     │
│         → MQTT Publish                         │
│  PIR    → Upload Images → Supabase Storage    │
│         → MQTT Publish                         │
│  Voice  → Upload Audio → Supabase Storage     │
│         → MQTT Publish                         │
│  Sensor → MQTT Publish (no upload)            │
└────────────────────────────────────────────────┘
                    ↓
┌────────────────────────────────────────────────┐
│              MQTT Broker (HiveMQ)              │
│          1cc4e72660cd...s1.eu.hivemq.cloud     │
└────────────────────────────────────────────────┘
                    ↓
┌────────────────────────────────────────────────┐
│            Node-RED Middleware                 │
│                                                │
│  Flow 1: button → events table                │
│  Flow 2: pir_alert → events table (3x)        │
│  Flow 2.1: pir → events table                 │
│  Flow 3: voice → events table                 │
│  Flow 4: sensor → sensor_data table           │
└────────────────────────────────────────────────┘
                    ↓
┌────────────────────────────────────────────────┐
│           Supabase Database                    │
│                                                │
│  events table:                                 │
│    - button_press records                      │
│    - motion_detected records (with/without img)│
│    - voice_note records                        │
│                                                │
│  sensor_data table:                            │
│    - temperature readings                      │
└────────────────────────────────────────────────┘
                    ↓
┌────────────────────────────────────────────────┐
│          React Frontend                        │
│                                                │
│  GET /api/events?type=button_press             │
│  GET /api/events?type=motion_detected          │
│  GET /api/events?type=voice_note               │
│  GET /api/sensors/latest?type=temperature      │
│                                                │
│  Display: Images, Alerts, Logs, Voice, Temp   │
└────────────────────────────────────────────────┘
```

---

## ✅ Verification Checklist

### Flow 1: Button Press
- [x] MQTT subscription: `doorbell/evt/button`
- [x] Prepare function: Format payload
- [x] HTTP POST: `/rest/v1/events`
- [x] Field: `image_url`, `event_type: button_press`
- [x] Debug: Response logged

### Flow 2: PIR Alert Burst
- [x] MQTT subscription: `doorbell/evt/pir_alert`
- [x] Split function: Array → 3 messages
- [x] Prepare function: Format each message
- [x] HTTP POST: `/rest/v1/events` (3 times)
- [x] Field: `image_url`, `event_type: motion_detected`
- [x] Debug: Responses logged

### Flow 2.1: PIR Motion Log
- [x] MQTT subscription: `doorbell/evt/pir`
- [x] Prepare function: Format payload (no image)
- [x] HTTP POST: `/rest/v1/events`
- [x] Field: `image_url: null`, `event_type: motion_detected`
- [x] Debug: Response logged

### Flow 3: Voice Note
- [x] MQTT subscription: `doorbell/evt/voice`
- [x] Prepare function: Format payload
- [x] HTTP POST: `/rest/v1/events`
- [x] Field: `audio_url`, `event_type: voice_note`
- [x] Debug: Response logged

### Flow 4: Sensor Data
- [x] MQTT subscription: `doorbell/sensor/temp`
- [x] Prepare function: Format payload
- [x] HTTP POST: `/rest/v1/sensor_data`
- [x] Field: `value`, `sensor_type: temperature`
- [x] Debug: Response logged

---

## 🎉 Kết Luận

**Tất cả 5 flows đã hoàn chỉnh với database insert**:

✅ Upload media files → Supabase Storage  
✅ Publish MQTT với URLs  
✅ Node-RED receives MQTT  
✅ Insert metadata vào database  
✅ Frontend fetch và display  

**Quy trình đầy đủ từ hardware → cloud → frontend đã sẵn sàng!**
