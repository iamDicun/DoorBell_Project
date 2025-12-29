# ESP32 Doorbell - Backend API Specification

## Overview

This document describes all MQTT topics, JSON payloads, and Supabase Storage integration used by the ESP32 smart doorbell system for integration with the Node-RED backend and React frontend.

**Architecture Change (December 2025):**
- ESP32 uploads images/audio **directly to Supabase Storage**
- ESP32 receives public URLs from Supabase after upload
- ESP32 publishes these URLs via MQTT to Node-RED/Frontend
- Node-RED and Frontend access media files directly from Supabase URLs
- For audio playback: Frontend/Node-RED uploads to Supabase, sends URL via MQTT, ESP32 downloads and plays

**Device Information:**

- Device ID Format: `ESP32_{MAC_ADDRESS}` (e.g., `ESP32_A1B2C3D4`)
- Firmware Version: `1.0.0`
- Timestamp Format: Seconds since device boot (unsigned long)
- Supabase Project: `{PROJECT_REF}.supabase.co`
- Storage Bucket: `doorbell-media`

---

## Supabase Storage Configuration

### Storage Buckets
- **Bucket Name**: `doorbell-media`
- **Public Access**: Enabled for read operations
- **Folder Structure**:
  ```
  doorbell-media/
  ├── images/
  │   └── ESP32_{MAC}/
  │       ├── doorbell/      (doorbell press photos)
  │       ├── security/      (PIR alert photos)
  │       └── remote/        (remote capture)
  ├── audio/
  │   └── ESP32_{MAC}/
  │       ├── voicenotes/    (user recordings)
  │       └── messages/      (system messages)
  ```

### Upload Endpoint
```
POST https://{PROJECT_REF}.supabase.co/storage/v1/object/{bucket}/{path}
```

**Headers:**
```
Authorization: Bearer {SUPABASE_ANON_KEY}
Content-Type: image/jpeg  (or audio/wav)
```

**Response (Success):**
```json
{
  "Key": "doorbell-media/images/ESP32_A1B2C3D4/doorbell/123456.jpg"
}
```

**Public URL Format:**
```
https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/doorbell/123456.jpg
```

---

## MQTT Topics

| Topic                    | Direction       | Purpose                                    |
| ------------------------ | --------------- | ------------------------------------------ |
| `doorbell/evt/button`    | ESP32 → Backend | Doorbell press events with Supabase URLs  |
| `doorbell/evt/pir`       | ESP32 → Backend | PIR motion alerts with Supabase URLs       |
| `doorbell/evt/voice`     | ESP32 → Backend | Voice note recordings with Supabase URLs   |
| `doorbell/evt/snapshot`  | ESP32 → Backend | Manual snapshot capture with Supabase URLs |
| `doorbell/sensor/temp`   | ESP32 → Backend | Temperature readings (5-minute interval)   |
| `doorbell/heartbeat`     | ESP32 → Backend | Device health status                       |
| `doorbell/cmd/speak`     | Backend → ESP32 | Play audio from Supabase URL               |
| `doorbell/cmd/camera`    | Backend → ESP32 | Remote capture command                     |
| `doorbell/cmd/siren`     | Backend → ESP32 | Alarm/siren control (ON/OFF)               |
| `doorbell/cmd/volume`    | Backend → ESP32 | Volume control                             |

---

## Flow 1: Doorbell Ring Event & Guest Photo

**Trigger:** Guest presses button (short press)

### Step 1: ESP32 Local Processing

1. **Interrupt detected** → Play "Ding-dong" sound via speaker
2. **Camera captures** single photo (QVGA 320x240, ~15-20KB)
3. **Upload to Supabase:**
   ```
   POST https://{PROJECT_REF}.supabase.co/storage/v1/object/doorbell-media/images/ESP32_A1B2C3D4/doorbell/123456.jpg
   ```
4. **ESP32 constructs URL:**
   ```
   https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/doorbell/123456.jpg
   ```

### Step 2: Signal via MQTT (doorbell/evt/button)

**After successful upload (HTTP 200 OK):**

```json
{
  "event": "press",
  "image_url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/doorbell/123456.jpg",
  "image_size": 15234,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709123456
}
```

### Step 3: Backend Processing (Node-RED)

1. **Subscribe** to `doorbell/evt/button`
2. **Extract** `image_url` from payload
3. **Save to Database** (Events table):
   ```sql
   INSERT INTO events (device_id, event_type, image_url, timestamp)
   VALUES ('ESP32_A1B2C3D4', 'doorbell_press', 'https://...', 1709123456)
   ```
4. **Push notification** to Web DaAnti-Theft

**Trigger:** PIR sensor detects motion

### PIR Alert Levels

- **NORMAL:** 1-2 detections in 20 seconds (motion cleared)
- **MEDIUM:** 3 detections in 20 seconds (person lingering)
- **HIGH:** 4+ detections in 20 seconds (suspicious loitering)

**Detection Rules:**

- Each detection must be at least **3 seconds apart** (debounce)
- Detections are counted within a **20-second sliding window**
- Rising edge (LOW→HIGH) triggers immediate detection registration
- Alert level evaluated every 5 seconds

### Step 1: ESP32 Local Processing

1. **Check anti-noise logic** → Confirm human presence
2. **Burst Capture:** Take 3 consecutive photos
3. **Upload to Supabase:** Upload File 1, File 2, File 3 sequentially
   ```
   POST .../doorbell-media/images/ESP32_A1B2C3D4/security/123500_1.jpg
   POST .../doorbell-media/images/ESP32_A1B2C3D4/security/123500_2.jpg
   POST .../doorbell-media/images/ESP32_A1B2C3D4/security/123500_3.jpg
   ```

### Step 2: Signal via MQTT (doorbell/evt/pir)

**Payload:**

```json
{
  "status": "detected",
  "level": "high",
  "message": "Suspicious loitering detected",
  "detections": 5,
  "images": [
    "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/security/123500_1.jpg",
    "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/security/123500_2.jpg",
    "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/security/123500_3.jpg"
  ],
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709123500
}
```

### Step 3: Backend Processing (Node-RED)

1. **Check alarm switch state** (ON/OFF in dashboard)
2. **Save image URLs** to Database (SecurityEvents table)
3. **Send email alert** with 3 image links embedded:
   ```html
   <img src="https://.../motion_1.jpg">
   <img src="https://.../motion_2.jpg">
   <img src="https://.../motion_3.jpg">
   ```
4. **If alarm = ON:**
   - Publish MQTT to `doorbell/cmd/siren`
   - Payload: `{"action": "ON"}`

### Step 4: ESP32 Receives Command

- **Subscribe** to `doorbell/cmd/siren`
- **Receives:** `{"action": "ON"}`
- **Action:** Activate speaker siren/alarm sound

---

### ALERT_MEDIUM - Person Lingering

**Payload (doorbell/evt/pir):**

```json
{
  "status": "detected",
  "level": "medium",
  "message": "Person lingering at door",
  "detections": 3,
  "images": [
    "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/security/123600.jpg"
  ],
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709123600
}
```

**Triggers:**
- Single photo capture and upload
- No siren activation (medium alert only)

---

### ALERT_NORMAL Mailbox (Voice Note)

**Trigger:** Guest long-presses button > 3 seconds

### Step 1: ESP32 Recording

1. **Detect long press** (button held > 3 seconds)
2. **Record audio** via INMP441 microphone
3. **Create WAV file** (16kHz, 16-bit mono, max 10 seconds)
4. **Upload to Supabase:**
   ```
   POST https://{PROJECT_REF}.supabase.co/storage/v1/object/doorbell-media/audio/ESP32_A1B2C3D4/voicenotes/128456.wav
   ```

**Audio Format:**
- Sample Rate: 16000 Hz
- Bits per Sample: 16
- Channels: 1 (Mono)
- Format: PCM WAV with proper header

### Step 2: Signal via MQTT (doorbell/evt/voice)

**Payload:**

```json
{
  "event": "new_voice_note",
  "audio_url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/ESP32_A1B2C3D4/voicenotes/128456.wav",
  "duration": 10,
  "size": 160000,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709128456
}
```

### Step 3: Backend Processing (Node-RED)

1. **Receive URL** from MQTT payload
2. **Save to Database** (VoiceNotes table):
   ```sqlEnvironmental Monitoring (Temperature)

**Trigger:** Periodic timer (every 5 minutes)

### ESP32 Processing

1. **Read temperature sensor** (NTC thermistor on GPIO 40)
2. **Calculate temperature** using Steinhart-Hart equation
3. **No Supabase upload** (lightweight data only)

### Signal via MQTT (doorbell/sensor/temp)

**Payload:**

```json
{
  "value": 28.5,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709130000
}
```

**Published:** Every 5 minutes (configurable)

### Backend Processing (Node-RED)

1. **Subscribe** to `doorbell/sensor/temp`
2. **Display** on Web Dashboard (real-time chart/gauge)
3. **Optional:** Store in time-series database for historical analysis

### Extreme Temperature Alert

**If temperature < 0°C or > 40°C:**

```json
{
  "event": "extreme_temperature",
  "value": 45.0,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709130030
}
```

**Backend Action:** Send alert notification to user
Node-RED and Frontend can play the audio directly from `audio_url`.

---

## Flow 4: Temperature Monitoring

### Normal Temperature Reading (doorbell/telemetry)

```json
{
  "value": 28.5,
  "unit": "C",
  "device_iTwo-Way Communication & Remote Control (Web → ESP32)

This flow demonstrates bidirectional communication where Frontend/Node-RED also uploads to Supabase first.

---

### Function 1: Play Voice Message (Text-to-Speech or Pre-recorded)

**Scenario:** User wants ESP32 to play "Please wait" message

#### Step 1: Frontend/Node-RED Processing

1. **User selects** message: "Vui lòng đợi" (Please wait)
2. **If text input:**
   - Convert to audio using TTS API (e.g., Google TTS, Azure Speech)
   - Generates .mp3 or .wav file
3. **Upload to Supabase:**
   ```javascript
   const { data } = await supabase.storage
     .from('doorbell-media')
     .upload('audio/messages/wait.wav', audioFile)
   ```
4. **Get public URL:**
   ```
   https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/wait.wav
   ```

#### Step 2: Signal via MQTT (doorbell/cmd/speak)

**Node-RED publishes:**

```json
{
  "command": "play_url",
  "url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/wait.wav"
}
```

#### Step 3: ESP32 Processing

1. **Subscribe** to `doorbell/cmd/speak`
2. **Receive URL** from MQTT payload
3. **Download audio stream:**
   - Use `HTTPClient` with GET request
   - Stream audio data to I2S buffer
   - Play simultaneously (no full download needed)
4. **Playback** via MAX98357 speaker

**Implementation:**
```cpp
// ESP32 pseudo-code
void handleSpeakCommand(String url) {
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    WiFiClient *stream = http.getStreamPtr();
    while (http.connected()) {
      // Read chunk from stream
      size_t size = stream->available();
      uint8_t buffer[128];
      if (size) {
        size_t c = stream->readBytes(buffer, min(size, sizeof(buffer)));
        // Push to I2S buffer
        i2s_write(I2S_NUM_0, buffer, c, &bytes_written, portMAX_DELAY);
      }
    }
  }
  http.end();
}
```

---

### Function 2: Manual Snapshot (Remote Camera Trigger)

**Scenario:** User wants to capture photo remotely

#### Step 1: Frontend Action

1. **User clicks** "Snapshot" button on dashboard
2. **Frontend/Node-RED publishes** MQTT

#### Step 2: Signal via MQTT (doorbell/cmd/camera)

**Payload:**

```json
{
  "action": "capture"
}
```

#### Step 3: ESP32 Processing

1. **Subscribe** to `doorbell/cmd/camera`
2. **Receive command** `{"action": "capture"}`
3. **Capture photo** via OV2640 camera
4. **Upload to Supabase:**
   ```
   POST .../doorbell-media/images/ESP32_A1B2C3D4/remote/135000.jpg
   ```
5. **Get public URL**

#### Step 4: ESP32 Response via MQTT (doorbell/evt/snapshot)

**Publish to notify completion:**

```json
{
  "event": "snapshot",
  "status": "captured",
  "image_url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/remote/135000.jpg",
  "size": 16384,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1709135000
}
```

#### Step 5: Frontend Display

1. **Subscribe** to `doorbell/evt/snapshot`
2. **Receive** image URL
3. **Display** using `<img src="{image_url}">`

---

### Function 3: Siren/Alarm Control

**Activate Alarm:**

```json
// Topic: doorbell/cmd/siren
{
  "action": "ON"
}
```

**Deactivate Alarm:**

```json
// Topic: doorbell/cmd/siren
{
  "action": "OFF"
}
```

**ESP32 Response:** Plays/stops alarm tone on speaker

---

### Function 4: Volume Control

```json
// Topic: doorbell/cmd/volume
{
  "action": "set_volume",
  "value": 0.7
}
```

**ESP32 Action:** Adjusts MAX98357 gain (0.0-1.0)# Play Audio Message from Supabase URL

**NEW: Frontend/Node-RED uploads audio to Supabase, then sends URL to ESP32**

```json
{
  "action": "play_audio",
  "url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/alert.mp3"
}
```

**Device Response:**
1. Downloads audio file from Supabase URL to PSRAM buffer
2. Plays audio through speaker
3. Frees memory after playback
4. Publishes confirmation to `doorbell/status`:

```json
{
  "event": "audio_playback",
  "status": "completed",
  "url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/alert.mp3",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 135010
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

**Device Response:** Plays corresponding chime from SPIFFS (embedded) and publishes event to `doorbell/status`:

```json
{
  "event": "press",
  "chime": "style2",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 135000
}
```

**Implementation Details:**

- Chimes are embedded in firmware SPIFFS for offline operation
- Uses ESP8266Audio library with MP3 decoder
- If file not found on SPIFFS, falls back to test tone (Mario melody)
- Maximum file size: 2MB (limited by available RAM)

**SPIFFS File Upload Required:**

Before chime sounds work, MP3 files must be uploaded to device SPIFFS:

1. Place MP3 files in project `data/` folder:

   ```
   data/ding_dong.mp3
   data/ding_dong_2.mp3
   data/ding_dong_3.mp3
   data/ding_dong_4.mp3
   ```

2. Upload to device using PlatformIO:

   ```bash
   platformio run --target uploadfs
   ```

3. Verify files in Serial Monitor during boot:
   ```
   [SPIFFS] Checking audio files:
     ✓ /ding_dong.mp3 (15234 bytes)
     ✓ /ding_dong_2.mp3 (18456 bytes)
   ```

---

#### Play Custom Audio from SPIFFS (Legacy)

```json
{
  "file": "wait"
}
```

**Device Response:** Plays specified file from SPIFFS storage (embedded files only)

**Note:** For dynamic audio content, use `play_audio` with Supabase URL instead.

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
**Flow 1: Doorbell Press**
- [ ] Button press triggers MQTT `doorbell/evt/button` within 2 seconds
- [ ] Image uploaded to Supabase successfully
- [ ] `image_url` in MQTT payload is valid and accessible
- [ ] Frontend can display image from URL
- [ ] "Ding-dong" sound plays on ESP32

**Flow 2: PIR Motion**
- [ ] PIR HIGH alert uploads 3 burst photos to Supabase
- [ ] MQTT `doorbell/evt/pir` contains `images` array with 3 URLs
- [ ] Node-RED checks alarm switch state
- [ ] Email sent with embedded image links (if enabled)
- [ ] MQTT `doorbell/cmd/siren` published when alarm ON
- [ ] ESP32 activates siren sound on command
- [ ] PIR detections debounced (minimum 3s between each)

**Flow 3: Voice Note**
- [ ] Long press (>3s) starts recording
- [ ] WAV file uploaded to Supabase successfully
- [ ] MQTT `doorbell/evt/voice` contains valid `audio_url`
- [ ] Frontend can play audio from URL
- [ ] Database stores voice note entry

**Flow 4: Temperature**
- [ ] Temperature readings published every 5 minutes
- [ ] MQTT `doorbell/sensor/temp` received by Node-RED
- [ ] Dashboard displays current temperature
- [ ] Extreme temperature alert triggered for < 0°C or > 40°C

**Flow 5: Two-Way Control**
- [ ] Frontend uploads audio to Supabase successfully
- [ ] MQTT `doorbell/cmd/speak` sent with Supabase URL
- [ ] ESP32 downloads and plays audio from URL
- [ ] Manual snapshot via `doorbell/cmd/camera` works
- [ ] ESP32 responds with `doorbell/evt/snapshot` containing image URL
- [ ] Siren control via `doorbell/cmd/siren` works (ON/OFF)
- [ ] Volume control via `doorbell/cmd/volume` adjusts speaker

**General**
- [ ] All MQTT payloads include `device_id` and `timestamp`
- [ ] Supabase URLs are publicly accessible
- [ ] MQTT reconnection works after network loss
- [ ] ESP32 frees PSRAM after uploads/downloads
- [ ] Heartbeat received every 30 seconds----------------------- | ------------------------------- |
| `doorbell/evt/button`   | Doorbell press    | `image_url`             | Guest pressed button + photo    |
| `doorbell/evt/pir`      | Motion detection  | `images[]`              | PIR alert + 1-3 photos          |
| `doorbell/evt/voice`    | Voice recording   | `audio_url`             | Guest voice note                |
| `doorbell/evt/snapshot` | Manual capture    | `image_url`             | Remote snapshot response        |
| `doorbell/sensor/temp`  | Temperature       | `value`, `unit`         | Environmental monitoring        |
| `doorbell/heartbeat`    | Health status     | `status`, `uptime`      | Device keepalive                |

### Backend → ESP32 (Commands)

| Topic                 | Command           | Payload                         | Description                  |
| --------------------- | ----------------- | ------------------------------- | ---------------------------- |
| `doorbell/cmd/speak`  | Play audio        | `{"command":"play_url","url"}` | Stream audio from Supabase   |
| `doorbell/cmd/camera` | Capture photo     | `{"action":"capture"}`          | Trigger manual snapshot      |
| `doorbell/cmd/siren`  | Alarm control     | `{"action":"ON/OFF"}`           | Activate/deactivate siren    |
| `doorbell/cmd/volume` | Volume control    | `{"action":"set_volume","val"}` | Adjust speaker volume 0.0-1.0
4. ESP32 downloads from URL
5. ESP32 plays audio

### Supabase Storage Setup

```sql
-- Create storage bucket
INSERT INTO storage.buckets (id, name, public)
VALUES ('doorbell-media', 'doorbell-media', true);

-- Set up RLS policies for public read
CREATE POLICY "Public read access"
ON storage.objects FOR SELECT
USING (bucket_id = 'doorbell-media');

-- Allow authenticated uploads
CREATE POLICY "Authenticated upload"
ON storage.objects FOR INSERT
WITH CHECK (bucket_id = 'doorbell-media' AND auth.role() = 'authenticated');
```

### Correlating MQTT Events with Supabase URLs

1. **URL Correlation:** MQTT payload contains `image_url` or `audio_url` field
2. **Timing Correlation:** Use `timestamp` field to track event sequence
3. **Device Identification:** `device_id` field enables multi-device deployments

### Recommended Database Schema

```javascript
// Events Collection (PostgreSQL/Supabase)
{
  id: uuid,
  device_id: "ESP32_A1B2C3D4",
  event_type: "doorbell_press",
  timestamp: 123456,
  media_urls: [
    "https://{PROJECT_REF}.supabase.co/storage/.../123456.jpg"
  ],
  metadata: jsonb,
  created_at: timestamp
}

// No separate files table needed - URLs are in events
```

### ESP32 Configuration

```cpp
// config.h
#define SUPABASE_URL "https://{PROJECT_REF}.supabase.co"
#define SUPABASE_ANON_KEY "your-anon-key"
#define STORAGE_BUCKET "doorbell-media"

// Upload paths
#define IMAGE_PATH_DOORBELL "images/%s/doorbell/%lu.jpg"
#define IMAGE_PATH_SECURITY "images/%s/security/%lu.jpg"
#define AUDIO_PATH_VOICENOTE "audio/%s/voicenotes/%lu.wav"
```

### Security Considerations

1. **Authentication:** Use Supabase anon key with RLS policies
2. **Encryption:** HTTPS for all Supabase communication
3. **Row Level Security (RLS):** Configure Supabase policies for access control
4. **File Validation:** Backend should validate media URLs before sending to ESP32
5. **Rate Limiting:** Implement rate limits on Supabase API to prevent abuse

---

## Testing Checklist

- [ ] Doorbell press uploads image to Supabase and sends URL via MQTT within 2 seconds
- [ ] All 4 chime styles (play_chime_1/2/3/4) work via MQTT command
- [ ] PIR HIGH alert uploads 3 photos to Supabase + sends URLs array via MQTT + triggers alarm
- [ ] PIR MEDIUM alert uploads 1 photo to Supabase + sends URL via MQTT
- [ ] PIR detections debounced (minimum 3s between each)
- [ ] Long button press records, uploads to Supabase, and sends audio URL via MQTT
- [ ] Temperature readings appear every 30 seconds
- [ ] Remote capture uploads to Supabase and sends URL within 5 seconds
- [ ] Alarm ON/OFF commands toggle speaker
- [ ] Frontend can upload audio to Supabase and send URL via MQTT for ESP32 playback
- [ ] ESP32 successfully downloads audio from Supabase URL and plays
- [ ] Heartbeat received every 30 seconds
- [ ] All MQTT payloads include `device_id`, `timestamp`, and media URLs
- [ ] Supabase URLs are publicly accessible
- [ ] Node-RED can access images/audio from Supabase URLs
- [ ] MQTT reconnection increments reconnect_count
- [ ] ESP32 frees PSRAM after upload to Supabase

---

## Quick Reference: MQTT Commands

### Remote Control Commands (doorbell/command)

| Command       | Payload                               | Description                  |
| ------------- | ------------------------------------- | ---------------------------- |
| Alarm ON      | `{"action":"ON"}`                     | Activate security alarm      |
| Alarm OFF     | `{"action":"OFF"}`                    | Deactivate security alarm    |
| Capture Photo | `{"action":"capture"}`                | Take photo → Supabase → URL  |
| Chime Style 1 | `{"action":"play_chime_1"}`           | Play default doorbell sound  |
| Chime Style 2 | `{"action":"play_chime_2"}`           | Play alternative chime 2     |
| Chime Style 3 | `{"action":"play_chime_3"}`           | Play alternative chime 3     |
| Chime Style 4 | `{"action":"play_chime_4"}`           | Play alternative chime 4     |
| Play Audio URL| `{"action":"play_audio","url":"..."}` | Download & play from Supabase|
| Set Volume    | `{"action":"set_volume","value":0.7}` | Set speaker volume (0.0-1.0) |

---

## Contact & Support

**Firmware Version:** 1.0.0  
**Last Updated:** December 2025  
**Platform:** ESP32-S3 with OV2640 Camera, INMP441 Mic, MAX98357 Speaker  
**Storage Backend:** Supabase Storage (Direct Upload Architecture)
