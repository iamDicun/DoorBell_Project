# 🚀 Node-RED Backend Implementation Plan

**Project:** DoorBell Smart Home System  
**Version:** 1.0  
**Date:** December 30, 2025  
**Status:** 📋 Planning Phase

---

## 📊 Current Status Summary

### ✅ Completed (30%)
- ✅ Flow 1: Button Press Event (MQTT → DB)
- ✅ Flow 2: PIR Motion Detection (MQTT → DB)
- ✅ Flow 3: Voice Notes (MQTT → DB)
- ✅ Flow 4: Temperature Sensor (MQTT → DB)
- ✅ API: `GET /api/events` (basic)
- ✅ API: `GET /api/sensors/latest`

### ❌ Missing (70%)
- ❌ All Command APIs (4 endpoints)
- ❌ All MQTT Publishers (4 topics)
- ❌ Voice Notes API (3 endpoints)
- ❌ Sensor Stats API (2 endpoints)
- ❌ Device Settings API (2 endpoints)
- ❌ Events API enhancements (3 endpoints)

---

## 🎯 Implementation Roadmap

### Phase 1: Critical Features (Priority: 🔴 HIGH)
**Goal:** Enable two-way communication (Frontend ↔ ESP32)  
**Timeline:** Week 1

#### 1.1 Command API - Snapshot
**Endpoint:** `POST /api/commands/snapshot`

**Request:**
```json
POST /api/commands/snapshot
Content-Type: application/json

{
  "description": "Manual snapshot" // optional
}
```

**Response (200):**
```json
{
  "success": true,
  "message": "Snapshot command sent to ESP32",
  "command_id": "cmd_1735490500_snapshot",
  "timestamp": 1735490500
}
```

**MQTT Publish:**
```
Topic: doorbell/cmd/snapshot
QoS: 1
Payload: {
  "command_id": "cmd_1735490500_snapshot",
  "timestamp": 1735490500
}
```

**Node-RED Implementation:**
```
[HTTP In: POST /api/commands/snapshot]
    ↓
[Function: Generate Command ID]
    ↓
[MQTT Out: doorbell/cmd/snapshot] ──┐
    ↓                                 │
[Function: Format HTTP Response]     │
    ↓                                 │
[HTTP Response: 200]                  │
                                      │
                                      ▼
[MQTT In: doorbell/evt/snapshot]
    ↓
[Function: Parse ESP32 Response]
    ↓
[HTTP Request: INSERT to Supabase]
    ↓
[Debug: Log Success]
```

---

#### 1.2 Command API - Speak (Audio Message)
**Endpoint:** `POST /api/commands/speak`

**Request:**
```json
POST /api/commands/speak
Content-Type: application/json

{
  "audio_url": "https://xxx.supabase.co/.../message.wav", // required
  "message_type": "predefined", // optional: predefined|custom|recorded
  "message_text": "Vui lòng đợi", // optional
  "volume": 80 // optional, 0-100, default: current setting
}
```

**Validation Rules:**
- `audio_url`: Required, must be valid HTTPS URL
- `volume`: Optional, integer 0-100
- `message_type`: Optional, enum: predefined|custom|recorded

**Response (200):**
```json
{
  "success": true,
  "message": "Audio playback command sent to ESP32",
  "command_id": "cmd_1735490600_speak",
  "audio_url": "https://xxx.supabase.co/.../message.wav",
  "volume": 80,
  "timestamp": 1735490600
}
```

**Response (400) - Validation Error:**
```json
{
  "success": false,
  "error": "Validation failed",
  "code": "VALIDATION_ERROR",
  "details": {
    "field": "audio_url",
    "message": "audio_url is required"
  }
}
```

**MQTT Publish:**
```
Topic: doorbell/cmd/speak
QoS: 1
Payload: {
  "command_id": "cmd_1735490600_speak",
  "audio_url": "https://xxx.supabase.co/.../message.wav",
  "volume": 80,
  "timestamp": 1735490600
}
```

---

#### 1.3 Command API - Siren Control
**Endpoint:** `POST /api/commands/siren`

**Request:**
```json
POST /api/commands/siren
Content-Type: application/json

{
  "action": "on", // required: "on" or "off"
  "duration": 5 // optional, seconds (default: 5, max: 30)
}
```

**Validation Rules:**
- `action`: Required, must be "on" or "off"
- `duration`: Optional, integer 1-30 seconds

**Response (200):**
```json
{
  "success": true,
  "message": "Siren command sent to ESP32",
  "command_id": "cmd_1735490700_siren",
  "action": "on",
  "duration": 5,
  "timestamp": 1735490700
}
```

**MQTT Publish:**
```
Topic: doorbell/cmd/siren
QoS: 1
Payload: {
  "command_id": "cmd_1735490700_siren",
  "action": "on",
  "duration": 5,
  "timestamp": 1735490700
}
```

**Side Effect:**
- If action = "on": Update `device_settings.alarm_enabled = true`
- If action = "off": Update `device_settings.alarm_enabled = false`

---

#### 1.4 Device Settings API
**Endpoint:** `GET /api/settings`

**Request:**
```bash
GET /api/settings
```

**Response (200):**
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

**Node-RED Query:**
```sql
SELECT * FROM device_settings WHERE id = 1
```

---

**Endpoint:** `PATCH /api/settings`

**Request:**
```json
PATCH /api/settings
Content-Type: application/json

{
  "alarm_enabled": true, // optional
  "speaker_volume": 70, // optional, 0-100
  "do_not_disturb": false, // optional
  "pir_enabled": true, // optional
  "notifications_enabled": true // optional
}
```

**Validation Rules:**
- All fields optional (partial update)
- `speaker_volume`: integer 0-100
- Boolean fields: true/false

**Response (200):**
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
    "last_sync": "2025-12-29T15:35:00.000Z"
  }
}
```

**MQTT Publish (After DB Update):**
```
Topic: doorbell/cmd/settings
QoS: 1
Retain: false
Payload: {
  "speaker_volume": 70,
  "pir_enabled": true,
  "alarm_enabled": true,
  "notifications_enabled": true,
  "timestamp": 1735490900
}
```

**Node-RED Flow:**
```
[HTTP In: PATCH /api/settings]
    ↓
[Function: Validate Input]
    ↓
[HTTP Request: UPDATE device_settings in Supabase]
    ↓
[Function: Prepare MQTT Payload]
    ↓
[MQTT Out: doorbell/cmd/settings]
    ↓
[Function: Format HTTP Response]
    ↓
[HTTP Response: 200]
```

---

### Phase 2: Core Features (Priority: 🟡 MEDIUM)
**Timeline:** Week 2

#### 2.1 Events API - Complete Implementation

**Endpoint:** `GET /api/events` (Enhancement)

**Current State:** ✅ Basic implementation exists  
**Needed:** Add missing query params

**Request:**
```bash
GET /api/events?limit=20&offset=0&type=button_press&unread=true
```

**Query Parameters:**
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| limit | integer | 20 | Records per page (max: 100) |
| offset | integer | 0 | Pagination offset |
| type | string | null | Filter: button_press, pir_motion, snapshot, voice_note |
| unread | boolean | false | Only unread events |

**Response (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 123,
      "created_at": "2025-12-29T14:30:00.000Z",
      "event_type": "button_press",
      "image_url": "https://xxx.supabase.co/.../button.jpg",
      "images": null,
      "description": "Có khách nhấn chuông",
      "is_read": false,
      "audio_url": null,
      "metadata": {
        "source": "doorbell_button",
        "raw_timestamp": 1735488600
      }
    }
  ],
  "total": 125,
  "limit": 20,
  "offset": 0
}
```

**Supabase Query:**
```sql
SELECT * FROM events
WHERE (event_type = $type OR $type IS NULL)
  AND (is_read = false OR $unread = false)
ORDER BY created_at DESC
LIMIT $limit OFFSET $offset
```

---

**Endpoint:** `GET /api/events/:id`

**Request:**
```bash
GET /api/events/123
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "id": 123,
    "created_at": "2025-12-29T14:30:00.000Z",
    "event_type": "button_press",
    "image_url": "https://xxx.supabase.co/.../button.jpg",
    "images": null,
    "description": "Có khách nhấn chuông",
    "is_read": false,
    "audio_url": null,
    "metadata": null
  }
}
```

**Response (404):**
```json
{
  "success": false,
  "error": "Event not found",
  "code": "NOT_FOUND"
}
```

---

**Endpoint:** `PATCH /api/events/:id/read`

**Request:**
```json
PATCH /api/events/123/read
Content-Type: application/json

{
  "is_read": true // optional, default: true
}
```

**Response (200):**
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

**Supabase Update:**
```sql
UPDATE events
SET is_read = $is_read, updated_at = NOW()
WHERE id = $id
RETURNING id, is_read, updated_at
```

---

**Endpoint:** `DELETE /api/events/:id`

**Request:**
```bash
DELETE /api/events/123
```

**Response (200):**
```json
{
  "success": true,
  "message": "Event deleted",
  "deleted_id": 123
}
```

**Response (404):**
```json
{
  "success": false,
  "error": "Event not found",
  "code": "NOT_FOUND"
}
```

---

#### 2.2 Voice Notes API (New)

**Endpoint:** `GET /api/voice-notes`

**Request:**
```bash
GET /api/voice-notes?limit=20&offset=0&unlistened=true
```

**Query Parameters:**
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| limit | integer | 20 | Records per page |
| offset | integer | 0 | Pagination |
| unlistened | boolean | false | Only unlistened notes |

**Response (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 45,
      "created_at": "2025-12-29T14:30:00.000Z",
      "audio_url": "https://xxx.supabase.co/.../voice.wav",
      "event_type": "voice_note",
      "description": "Voice note recorded",
      "is_read": false,
      "metadata": {
        "source": "doorbell_microphone",
        "duration_ms": 15000
      }
    }
  ],
  "total": 46,
  "limit": 20,
  "offset": 0
}
```

**Supabase Query:**
```sql
SELECT * FROM events
WHERE event_type = 'voice_note'
  AND (is_read = false OR $unlistened = false)
ORDER BY created_at DESC
LIMIT $limit OFFSET $offset
```

---

**Endpoint:** `PATCH /api/voice-notes/:id/listened`

**Request:**
```json
PATCH /api/voice-notes/45/listened
Content-Type: application/json

{
  "is_listened": true // maps to is_read field
}
```

**Response (200):**
```json
{
  "success": true,
  "message": "Voice note marked as listened",
  "data": {
    "id": 45,
    "is_listened": true,
    "updated_at": "2025-12-29T15:30:00.000Z"
  }
}
```

---

**Endpoint:** `DELETE /api/voice-notes/:id`

**Request:**
```bash
DELETE /api/voice-notes/45
```

**Response (200):**
```json
{
  "success": true,
  "message": "Voice note deleted",
  "deleted_id": 45
}
```

---

#### 2.3 Sensor Logs API

**Endpoint:** `GET /api/sensor-logs`

**Request:**
```bash
GET /api/sensor-logs?hours=24&limit=100&type=temperature
```

**Query Parameters:**
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| hours | integer | 24 | Last X hours |
| limit | integer | 100 | Max records |
| type | string | null | temperature (future: humidity) |

**Response (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 1234,
      "created_at": "2025-12-29T15:30:00.000Z",
      "sensor_type": "temperature",
      "temperature": 28.5,
      "humidity": null,
      "metadata": {
        "device_id": "ESP32_DOORBELL",
        "unit": "C"
      }
    },
    {
      "id": 1235,
      "created_at": "2025-12-29T15:35:00.000Z",
      "sensor_type": "temperature",
      "temperature": 28.7,
      "humidity": null,
      "metadata": null
    }
  ],
  "total": 288,
  "hours": 24
}
```

**Supabase Query:**
```sql
SELECT * FROM sensor_logs
WHERE created_at > NOW() - INTERVAL '$hours hours'
  AND (sensor_type = $type OR $type IS NULL)
ORDER BY created_at DESC
LIMIT $limit
```

---

**Endpoint:** `GET /api/sensor-logs/stats`

**Request:**
```bash
GET /api/sensor-logs/stats?days=7&interval=hour
```

**Query Parameters:**
| Param | Type | Default | Description |
|-------|------|---------|-------------|
| days | integer | 7 | Last X days |
| interval | string | hour | hour or day |

**Response (200):**
```json
{
  "success": true,
  "data": [
    {
      "timestamp": "2025-12-29T15:00:00.000Z",
      "avg_temperature": 28.6,
      "min_temperature": 27.5,
      "max_temperature": 29.8,
      "avg_humidity": null,
      "count": 12
    }
  ],
  "interval": "hour",
  "days": 7
}
```

**Supabase Query:**
```sql
SELECT
  date_trunc('$interval', created_at) as timestamp,
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

**Endpoint:** `GET /api/sensor-logs/latest` (Enhancement)

**Current State:** ✅ Exists  
**Needed:** Add support for all sensor types

**Request:**
```bash
GET /api/sensor-logs/latest?type=temperature
```

**Response (200):**
```json
{
  "success": true,
  "data": {
    "id": 1234,
    "temperature": 28.5,
    "humidity": null,
    "sensor_type": "temperature",
    "created_at": "2025-12-29T15:30:00.000Z"
  }
}
```

**Response (404):**
```json
{
  "success": false,
  "data": null,
  "message": "No sensor data found"
}
```

---

### Phase 3: MQTT Response Handlers (Priority: 🟡 MEDIUM)
**Timeline:** Week 2

#### 3.1 Snapshot Response Handler
**MQTT Topic:** `doorbell/evt/snapshot`

**ESP32 Payload:**
```json
{
  "command_id": "cmd_1735490600_snapshot",
  "image_url": "https://xxx.supabase.co/.../snapshot.jpg",
  "timestamp": 1735490602,
  "status": "success" // or "error"
}
```

**Node-RED Processing:**
```
[MQTT In: doorbell/evt/snapshot]
    ↓
[Function: Validate Payload]
    ↓
[HTTP Request: INSERT into events]
    INSERT INTO events (
      event_type, image_url, description, metadata
    ) VALUES (
      'snapshot',
      payload.image_url,
      'Chụp ảnh thủ công',
      { command_id: payload.command_id }
    )
    ↓
[Debug: Log Success]
```

---

### Phase 4: Error Handling & Validation (Priority: 🟢 LOW)
**Timeline:** Week 3

#### 4.1 Standard Error Response Format

**All errors follow this format:**
```json
{
  "success": false,
  "error": "Error message",
  "code": "ERROR_CODE",
  "details": {}
}
```

#### 4.2 Error Codes

| HTTP Status | Code | Description |
|-------------|------|-------------|
| 400 | VALIDATION_ERROR | Invalid request body/params |
| 404 | NOT_FOUND | Resource not found |
| 500 | DB_ERROR | Database error |
| 500 | MQTT_ERROR | MQTT publish failed |
| 500 | INTERNAL_ERROR | Generic server error |

#### 4.3 Validation Error Example

**Request:**
```json
POST /api/commands/speak
{
  "volume": 150
}
```

**Response (400):**
```json
{
  "success": false,
  "error": "Validation failed",
  "code": "VALIDATION_ERROR",
  "details": {
    "field": "audio_url",
    "message": "audio_url is required"
  }
}
```

#### 4.4 Catch-All Error Handler

**Node-RED Implementation:**
```
[Catch Node: Scope = All]
    ↓
[Function: Format Error]
    msg.statusCode = msg.statusCode || 500;
    msg.payload = {
      success: false,
      error: msg.error?.message || 'Internal server error',
      code: msg.errorCode || 'INTERNAL_ERROR',
      details: msg.error?.details || {}
    };
    return msg;
    ↓
[HTTP Response]
```

---

## 📋 Implementation Checklist

### Phase 1: Critical Features (Week 1)
```
Commands API:
[ ] POST /api/commands/snapshot
[ ] POST /api/commands/speak
[ ] POST /api/commands/siren

Settings API:
[ ] GET /api/settings
[ ] PATCH /api/settings (with MQTT notify)

MQTT Publishers:
[ ] doorbell/cmd/snapshot
[ ] doorbell/cmd/speak
[ ] doorbell/cmd/siren
[ ] doorbell/cmd/settings

MQTT Subscribers:
[ ] doorbell/evt/snapshot
```

### Phase 2: Core Features (Week 2)
```
Events API:
[ ] GET /api/events (add offset, unread params)
[ ] GET /api/events/:id
[ ] PATCH /api/events/:id/read
[ ] DELETE /api/events/:id

Voice Notes API:
[ ] GET /api/voice-notes
[ ] PATCH /api/voice-notes/:id/listened
[ ] DELETE /api/voice-notes/:id

Sensor Logs API:
[ ] GET /api/sensor-logs
[ ] GET /api/sensor-logs/stats
[ ] GET /api/sensor-logs/latest (enhancement)
```

### Phase 3: Error Handling (Week 3)
```
[ ] Add validation for all endpoints
[ ] Add catch-all error handler
[ ] Add MQTT publish error handling
[ ] Add database error handling
[ ] Add logging for all errors
```

---

## 🔧 Testing Plan

### 1. Unit Tests (cURL)

**Test Snapshot Command:**
```bash
curl -X POST http://localhost:1880/api/commands/snapshot \
  -H "Content-Type: application/json" \
  -d '{"description": "Test"}'
```

**Expected Response:**
```json
{
  "success": true,
  "message": "Snapshot command sent to ESP32",
  "command_id": "cmd_..._snapshot"
}
```

**Test Settings Update:**
```bash
curl -X PATCH http://localhost:1880/api/settings \
  -H "Content-Type: application/json" \
  -d '{"speaker_volume": 75, "alarm_enabled": true}'
```

**Test Get Events with Filters:**
```bash
curl "http://localhost:1880/api/events?limit=10&unread=true&type=button_press"
```

### 2. MQTT Integration Tests

**Monitor MQTT Topic:**
```bash
# Subscribe to all doorbell topics
mosquitto_sub -h your-broker.com -p 8883 \
  -t "doorbell/#" -u username -P password --cafile ca.crt
```

**Publish Test Event:**
```bash
# Simulate ESP32 snapshot response
mosquitto_pub -h your-broker.com -p 8883 \
  -t "doorbell/evt/snapshot" -u username -P password \
  -m '{"command_id":"cmd_123_snapshot","image_url":"https://test.jpg","timestamp":1735490000}'
```

### 3. Integration Tests

**Flow:**
1. Frontend sends snapshot command
2. Node-RED publishes MQTT to ESP32
3. ESP32 responds with image URL
4. Node-RED saves to database
5. Frontend polls and receives new event

---

## 📊 Success Metrics

### Week 1 Goals:
- ✅ All 4 command endpoints working
- ✅ All 4 MQTT publishers working
- ✅ Settings API complete with MQTT sync
- ✅ Manual testing successful

### Week 2 Goals:
- ✅ All Events API endpoints complete
- ✅ Voice Notes API complete
- ✅ Sensor Logs API complete
- ✅ cURL tests passing

### Week 3 Goals:
- ✅ Error handling implemented
- ✅ Validation on all endpoints
- ✅ Integration tests passing
- ✅ Ready for production

---

## 🚀 Deployment Notes

### Environment Variables
```javascript
// ~/.node-red/settings.js
module.exports = {
  functionGlobalContext: {
    SUPABASE_URL: process.env.SUPABASE_URL,
    SUPABASE_ANON_KEY: process.env.SUPABASE_ANON_KEY,
    MQTT_BROKER: process.env.MQTT_BROKER,
    MQTT_USERNAME: process.env.MQTT_USERNAME,
    MQTT_PASSWORD: process.env.MQTT_PASSWORD
  }
}
```

### Production Checklist
```
[ ] HTTPS enabled
[ ] Authentication added (JWT or API key)
[ ] Rate limiting configured
[ ] CORS configured for frontend domain
[ ] Logging enabled
[ ] Monitoring setup
[ ] Backup strategy in place
```

---

**Last Updated:** December 30, 2025  
**Next Review:** January 6, 2026
