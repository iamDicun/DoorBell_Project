# ESP32 Doorbell - Backend API Specification

## Overview

This document describes all MQTT topics, JSON payloads, and HTTP endpoints used by the ESP32 smart doorbell system for integration with the Node-RED backend.

**Device Information:**

- Device ID Format: `ESP32_{MAC_ADDRESS}` (e.g., `ESP32_A1B2C3D4`)
- Firmware Version: `1.0.0`
- Timestamp Format: Seconds since device boot (unsigned long)

---

## MQTT Topics

| Topic                | Direction       | Purpose                                    |
| -------------------- | --------------- | ------------------------------------------ |
| `doorbell/status`    | ESP32 → Backend | Doorbell ring events, guest photos         |
| `doorbell/security`  | ESP32 → Backend | PIR alerts, security photos, temperature   |
| `doorbell/telemetry` | ESP32 → Backend | Environmental data (temperature, humidity) |
| `doorbell/heartbeat` | ESP32 → Backend | Device health status                       |
| `doorbell/command`   | Backend → ESP32 | Remote control commands                    |

---

## Flow 1: Doorbell Ring Event

### MQTT Notification (doorbell/status)

```json
{
  "event": "press",
  "chime": "style1",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123456
}
```

**Chime Styles Available:**

- `style1` - Default doorbell chime
- `style2` - Alternative chime sound 2
- `style3` - Alternative chime sound 3
- `style4` - Alternative chime sound 4

### Photo Upload Success (doorbell/status)

```json
{
  "event": "guest_photo",
  "status": "captured",
  "size": 15234,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123460
}
```

### HTTP Photo Upload

**Endpoint:** `POST http://192.168.137.1:3000/upload-image`

**Headers:**

```
Content-Type: image/jpeg
X-Event-Type: doorbell_press
X-Timestamp: 123460
```

**Body:** Binary JPEG data (QVGA 320x240, ~15-20KB)

**Response Expected:** 200 OK

---

## Flow 2: PIR Motion Detection & Security Alerts

### PIR Alert Levels

- **NORMAL:** 1-2 detections in 20 seconds (motion cleared)
- **MEDIUM:** 3 detections in 20 seconds (person lingering)
- **HIGH:** 4+ detections in 20 seconds (suspicious loitering)

**Detection Rules:**

- Each detection must be at least **3 seconds apart** (debounce)
- Detections are counted within a **20-second sliding window**
- Rising edge (LOW→HIGH) triggers immediate detection registration
- Alert level evaluated every 5 seconds

### ALERT_HIGH - Suspicious Activity (doorbell/security)

```json
{
  "event": "pir_alert",
  "level": "high",
  "message": "Suspicious loitering detected",
  "detections": 5,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123500
}
```

**Triggers:**

- 3 security photos (burst mode)
- Alarm activation (speaker plays alert tone)

### Security Burst Photos (doorbell/security)

```json
{
  "status": "detected",
  "count": 3,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123510
}
```

### HTTP Security Photo Uploads

**Endpoint:** `POST http://192.168.137.1:3000/upload-image`

**Headers:**

```
Content-Type: image/jpeg
X-Event-Type: pir_burst
X-Timestamp: 123510
```

**Body:** 3 separate uploads, each with binary JPEG data

---

### ALERT_MEDIUM - Person Lingering (doorbell/security)

```json
{
  "event": "pir_alert",
  "level": "medium",
  "message": "Person lingering at door",
  "detections": 3,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123600
}
```

**Triggers:**

- Single guest photo capture
- Photo upload to backend

---

### ALERT_NORMAL - Motion Cleared (doorbell/security)

```json
{
  "event": "pir_alert",
  "level": "normal",
  "message": "Motion cleared",
  "detections": 1,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123700
}
```

**Triggers:**

- Alarm deactivation (if active)

---

## Flow 3: Voice Note Recording

### Recording Start - Long Button Press (≥3 seconds)

No MQTT notification sent during recording start.

### Recording Stop & Upload (doorbell/status)

```json
{
  "event": "voice_note",
  "status": "uploaded",
  "duration_ms": 5000,
  "size": 160000,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 128456
}
```

### HTTP Audio Upload

**Endpoint:** `POST http://192.168.137.1:3000/upload-audio`

**Headers:**

```
Content-Type: audio/wav
X-Event-Type: voice_note
X-Timestamp: 128456
```

**Body:** Binary WAV data (16kHz, 16-bit mono, max 10 seconds)

**Audio Format:**

- Sample Rate: 16000 Hz
- Bits per Sample: 16
- Channels: 1 (Mono)
- Format: PCM WAV with proper header

---

## Flow 4: Temperature Monitoring

### Normal Temperature Reading (doorbell/telemetry)

```json
{
  "value": 28.5,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 130000
}
```

**Published:** Every 30 seconds (configurable)

### Extreme Temperature Alert (doorbell/security)

```json
{
  "event": "extreme_temperature",
  "value": 45.0,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 130030
}
```

**Thresholds:**

- Alert if < 0°C or > 40°C

---

## Flow 5: Remote Control via MQTT

### Command Format (doorbell/command)

#### Activate Alarm

```json
{
  "action": "ON"
}
```

**Device Response:** Plays alarm tone on speaker until deactivated

---

#### Deactivate Alarm

```json
{
  "action": "OFF"
}
```

**Device Response:** Stops alarm playback

---

#### Capture Photo on Demand

```json
{
  "action": "capture"
}
```

**Device Response:**

1. Captures single photo
2. Uploads to `/upload-image` with `X-Event-Type: remote_capture`
3. Publishes success to `doorbell/status`:

```json
{
  "event": "guest_photo",
  "status": "captured",
  "size": 16384,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 135000
}
```

---

#### Play Doorbell Chimes

**Play Default Chime (Style 1):**

```json
{ "action": "play_chime_1" }
```

**Play Chime Style 2:**

```json
{ "action": "play_chime_2" }
```

**Play Chime Style 3:**

```json
{ "action": "play_chime_3" }
```

**Play Chime Style 4:**

```json
{ "action": "play_chime_4" }
```

**Device Response:** Plays corresponding MP3 file from SPIFFS and publishes event to `doorbell/status`:

```json
{
  "event": "press",
  "chime": "style2",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 135000
}
```

---

#### Play Audio Message (Generic)

```json
{
  "file": "wait"
}
```

**Device Response:** Plays specified file from SPIFFS storage (playSampleMessage function)

**Available Audio Files:**

- `ding_dong.mp3` - Default doorbell chime (Style 1)
- `ding_dong_2.mp3` - Doorbell chime Style 2
- `ding_dong_3.mp3` - Doorbell chime Style 3
- `ding_dong_4.mp3` - Doorbell chime Style 4
- `alarm.mp3` - Security alarm tone
- `please_wait.mp3` - Custom waiting message

---

## Heartbeat & Health Monitoring

### Heartbeat (doorbell/heartbeat)

Published every 60 seconds to indicate device is online.

```json
{
  "status": "online",
  "firmware_version": "1.0.0",
  "ip": "192.168.1.100",
  "reconnect_count": 0,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 140000
}
```

**Fields:**

- `status`: Always "online" when published
- `firmware_version`: Software version string
- `ip`: Current WiFi IP address
- `reconnect_count`: Number of MQTT reconnections since boot
- `device_id`: Unique device identifier
- `timestamp`: Seconds since boot

---

## Error Handling

### Upload Failures

If HTTP upload fails, device will retry up to 3 times with exponential backoff:

- Retry 1: After 1 second
- Retry 2: After 2 seconds
- Retry 3: After 4 seconds

After 3 failures, device logs error to serial but continues operation.

### MQTT Disconnection

Device automatically attempts reconnection:

- Immediate first retry
- Subsequent retries every 5 seconds
- `reconnect_count` increments on each successful reconnection
- Heartbeat resumes after reconnection

---

## Backend Implementation Notes

### Correlating MQTT Events with HTTP Uploads

1. **Event Type Correlation:** Use `X-Event-Type` header to identify upload context:

   - `doorbell_press` → User rang doorbell
   - `pir_burst` → Security alert (expect 3 photos)
   - `voice_note` → Voice message from user
   - `remote_capture` → Photo requested via MQTT command

2. **Timing Correlation:** Use `X-Timestamp` header to match MQTT events with uploads:

   - MQTT event timestamp and HTTP upload timestamp should be within 1-2 seconds
   - Use this to link notification with corresponding file

3. **Device Identification:** `device_id` field enables multi-device deployments:
   - Use to route events/files to correct dashboard/user
   - Track per-device statistics and history

### Recommended Database Schema

```javascript
// Events Collection
{
  device_id: "ESP32_A1B2C3D4",
  event_type: "doorbell_press",
  timestamp: 123456,
  metadata: {
    // Event-specific data
  },
  associated_files: [
    "uploads/ESP32_A1B2C3D4_123460.jpg"
  ]
}

// Files Collection
{
  device_id: "ESP32_A1B2C3D4",
  file_type: "image/jpeg",
  event_type: "doorbell_press",
  timestamp: 123460,
  size: 15234,
  path: "uploads/ESP32_A1B2C3D4_123460.jpg"
}
```

### Security Considerations

1. **Authentication:** Current implementation uses HiveMQ Cloud with username/password
2. **Encryption:** MQTT over TLS (port 8883)
3. **HTTP Security:** Currently unencrypted - consider HTTPS for production
4. **File Validation:** Backend should validate:
   - Content-Type matches actual file format
   - File size within reasonable limits (< 100KB for photos, < 1MB for audio)
   - JPEG/WAV header validation to prevent malicious uploads

---

## Testing Checklist

- [ ] Doorbell press triggers MQTT event + photo upload within 2 seconds
- [ ] All 4 chime styles (play_chime_1/2/3/4) work via MQTT command
- [ ] PIR HIGH alert triggers 3 photo uploads + alarm (4+ detections in 20s)
- [ ] PIR MEDIUM alert triggers 1 photo (3 detections in 20s)
- [ ] PIR detections debounced (minimum 3s between each)
- [ ] Long button press (3+ seconds) records and uploads voice note
- [ ] Temperature readings appear every 30 seconds
- [ ] Remote capture command works within 5 seconds
- [ ] Alarm ON/OFF commands toggle speaker
- [ ] Heartbeat received every 30 seconds (was 60s, now 30s per code)
- [ ] X-Event-Type and X-Timestamp headers present on all uploads
- [ ] MQTT reconnection increments reconnect_count
- [ ] All payloads include device_id and timestamp
- [ ] Chime field appears in doorbell/status payloads

---

## Quick Reference: MQTT Commands

### Remote Control Commands (doorbell/command)

| Command       | Payload                               | Description                  |
| ------------- | ------------------------------------- | ---------------------------- |
| Alarm ON      | `{"action":"ON"}`                     | Activate security alarm      |
| Alarm OFF     | `{"action":"OFF"}`                    | Deactivate security alarm    |
| Capture Photo | `{"action":"capture"}`                | Take single photo remotely   |
| Chime Style 1 | `{"action":"play_chime_1"}`           | Play default doorbell sound  |
| Chime Style 2 | `{"action":"play_chime_2"}`           | Play alternative chime 2     |
| Chime Style 3 | `{"action":"play_chime_3"}`           | Play alternative chime 3     |
| Chime Style 4 | `{"action":"play_chime_4"}`           | Play alternative chime 4     |
| Play File     | `{"file":"wait"}`                     | Play custom audio message    |
| Set Volume    | `{"action":"set_volume","value":0.7}` | Set speaker volume (0.0-1.0) |

---

## Contact & Support

**Firmware Version:** 1.0.0  
**Last Updated:** December 2025  
**Platform:** ESP32-S3 with OV2640 Camera, INMP441 Mic, MAX98357 Speaker
