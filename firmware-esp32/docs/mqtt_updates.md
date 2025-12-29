# MQTT Service Updates - HiveMQ Integration with Supabase Storage

## Tổng quan thay đổi (December 2025)

**Kiến trúc mới:**
- ESP32 upload trực tiếp lên **Supabase Storage** cho tất cả media (images, audio)
- ESP32 nhận lại **public URL** từ Supabase sau khi upload thành công
- ESP32 gửi URL qua **MQTT** đến Node-RED/Frontend
- Node-RED và Frontend truy cập media trực tiếp từ Supabase URLs
- **Ngược lại**: Frontend/Node-RED upload lên Supabase, gửi URL qua MQTT, ESP32 tải về và phát

**Lợi ích:**
- Không cần Node-RED lưu trữ file
- Media được phân phối qua Supabase CDN
- Frontend truy cập trực tiếp từ cloud
- Bidirectional media flow (ESP32 ⇄ Supabase ⇄ Clients)

## MQTT Topics

### 1. Motion Detection with Supabase URLs
**Topic**: `doorbell/sensors/motion` hoặc `doorbell/security`
**Format**:
```json
{
  "motion": true,
  "timestamp": 12345678,
  "image_url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/security/12345678.jpg"
}
```
- `motion`: `true` = có chuyển động, `false` = không có chuyển động
- `image_url`: URL ảnh từ Supabase (nếu có chụp ảnh)
- Publish ngay khi trạng thái PIR thay đổi

**Flow:**
1. ESP32 phát hiện chuyển động
2. ESP32 chụp ảnh → upload lên Supabase
3. Supabase trả về public URL
4. ESP32 publish MQTT với URL

### 2. Temperature Sensor
**Topic**: `doorbell/sensors/temperature`
**Format**:
```json
{
  "temperature": 25.3,
  "unit": "C",
  "timestamp": 12345678
}
```
- `temperature`: Nhiệt độ tính bằng độ C (Celsius), làm tròn 1 chữ số thập phân
- Publish mỗi 10 giây

### 3. Distance Sensor
**Topic**: `doorbell/sensors/distance`
**Format**:
```json
{
  "distance": 45.2,
  "unit": "cm",
  "timestamp": 12345678
}
```
- `distance`: Khoảng cách đo được (cm), làm tròn 1 chữ số thập phân
- Publish mỗi 10 giây

### 4. Telemetry with Media URLs (Tổng hợp)
**Topic**: `doorbell/telemetry`
**Format**:
```json
{
  "motion": true,
  "temperature": 25.3,
  "distance": 45.2,
  "wifi_rssi": -67,
  "timestamp": 12345678,
  "supabase_uploads": 42,
  "supabase_downloads": 15
}
```
- Gộp tất cả dữ liệu sensor + WiFi RSSI + Supabase stats
- Publish mỗi 10 giây

### 5. Doorbell Press with Image URL
**Topic**: `doorbell/status`
**Format**:
```json
{
  "event": "press",
  "chime": "style1",
  "image_url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/images/ESP32_A1B2C3D4/doorbell/12345678.jpg",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 12345678
}
```

**Flow:**
1. User nhấn nút chuông
2. ESP32 phát chime + chụp ảnh
3. Upload ảnh lên Supabase
4. Publish MQTT với image URL
5. Frontend/Node-RED hiển thị ảnh từ URL

### 6. Voice Note with Audio URL
**Topic**: `doorbell/status`
**Format**:
```json
{
  "event": "voice_note",
  "status": "uploaded",
  "duration_ms": 5000,
  "audio_url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/ESP32_A1B2C3D4/voicenotes/12345678.wav",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 12345678
}
```

**Flow:**
1. User giữ nút ≥3s để ghi âm
2. ESP32 record → upload lên Supabase
3. Supabase trả về audio URL
4. Publish MQTT với audio URL
5. Frontend/Node-RED play audio từ URL

### 7. Remote Audio Playback Command
**Topic**: `doorbell/command` (Backend → ESP32)
**Format**:
```json
{
  "action": "play_audio",
  "url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/alert.mp3"
}
```

**Flow:**
1. Frontend/Node-RED upload audio lên Supabase
2. Nhận được public URL
3. Gửi command via MQTT với URL
4. ESP32 tải audio từ URL
5. ESP32 phát qua speaker
6. ESP32 confirm via MQTT:

```json
{
  "event": "audio_playback",
  "status": "completed",
  "url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/alert.mp3",
  "timestamp": 12345690
}
```

## Files đã thay đổi

### 1. config.h
- Thêm Supabase configuration:
  - `SUPABASE_URL`
  - `SUPABASE_ANON_KEY`
  - `STORAGE_BUCKET`
- Thêm các MQTT topic mới:
  - `MQTT_TOPIC_MOTION`
  - `MQTT_TOPIC_TEMPERATURE`
  - `MQTT_TOPIC_DISTANCE`
  - `MQTT_TOPIC_STATUS` (with URLs)
  - `MQTT_TOPIC_COMMAND` (for URL-based commands)

### 2. mqtt_service.h / mqtt_service.cpp
- Thêm các hàm publish với URLs:
  - `mqttPublishMotion(bool motionDetected, const char* imageUrl)`
  - `mqttPublishStatus(const char* event, const char* imageUrl)`
  - `mqttPublishVoiceNote(const char* audioUrl, uint32_t duration)`
  - `mqttPublishAudioPlaybackStatus(const char* url, const char* status)`
- Parse command MQTT để xử lý URLs:
  - `handlePlayAudioCommand(const char* url)`
- Sử dụng ArduinoJson để format JSON

### 3. supabase_client.h / supabase_client.cpp (MỚI)
- `uploadImageToSupabase(uint8_t* imageData, size_t size, const char* path)`: Upload ảnh, trả về URL
- `uploadAudioToSupabase(uint8_t* audioData, size_t size, const char* path)`: Upload audio, trả về URL
- `downloadAudioFromSupabase(const char* url, uint8_t** buffer, size_t* size)`: Tải audio từ URL
- Xử lý HTTPS requests với Supabase REST API
- Parse JSON response để lấy public URL

### 4. upload_client.h / upload_client.cpp
- **Đổi từ Node-RED endpoint sang Supabase Storage API**
- Thay `POST http://192.168.137.1:3000/upload-image` 
- Thành `POST https://{PROJECT_REF}.supabase.co/storage/v1/object/{bucket}/{path}`
- Headers: `Authorization: Bearer {SUPABASE_ANON_KEY}`
- Return public URL thay vì chỉ status code

### 5. audio_service.h / audio_service.cpp
- Thêm `downloadAndPlayAudio(const char* url)`: Tải từ Supabase URL và phát
- Buffer management: download → PSRAM → play → free
- Error handling cho network issues

### 6. sensor_utils.h / sensor_utils.cpp
- `readPIRSensor()`: Đọc PIR sensor (HIGH = có chuyển động)
- `readTemperatureCelsius()`: Tính nhiệt độ từ thermistor sử dụng Steinhart-Hart equation
- `readDistanceCm()`: Đọc IR distance sensor và chuyển đổi sang cm

### 7. doorbell_app.cpp
- Khởi tạo Supabase client
- Loop kiểm tra PIR → capture → upload Supabase → MQTT with URL
- Handle MQTT commands với URLs
- Button press → capture → upload → MQTT with URL
- Long press → record → upload → MQTT with audio URL

## Cấu hình trong config.h

```cpp
// Supabase Configuration
#define SUPABASE_URL          "https://{PROJECT_REF}.supabase.co"
#define SUPABASE_ANON_KEY     "your-anon-key-here"
#define STORAGE_BUCKET        "doorbell-media"

// Upload paths
#define IMAGE_PATH_DOORBELL   "images/%s/doorbell/%lu.jpg"
#define IMAGE_PATH_SECURITY   "images/%s/security/%lu.jpg"
#define AUDIO_PATH_VOICENOTE  "audio/%s/voicenotes/%lu.wav"
#define AUDIO_PATH_MESSAGE    "audio/messages/%s.mp3"

// Sensor Configuration
#define PIR_PIN             48   // Motion detector
#define THERMISTOR_PIN      14   // ADC input for NTC
#define IR_SENSOR_PIN       48   // IR distance sensor

// Thermistor calibration (10k NTC, Beta 3950)
#define THERMISTOR_SERIES_OHMS      10000.0f
#define THERMISTOR_NOMINAL_OHMS     10000.0f
#define THERMISTOR_NOMINAL_TEMP_C   25.0f
#define THERMISTOR_BETA_COEFFICIENT 3950.0f

// IR Distance calibration
#define IR_ADC_RAW_NEAR     3200.0f
#define IR_ADC_RAW_FAR      200.0f
#define IR_DISTANCE_NEAR_CM 8.0f
#define IR_DISTANCE_FAR_CM  80.0f
```

## HiveMQ Cloud Configuration

```cpp
#define MQTT_BROKER    "aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud"
#define MQTT_PORT_TLS  8883
#define MQTT_USERNAME  "doorbell"
#define MQTT_PASSWORD  "Hcmus123"
```

## Cách hoạt động

### 1. ESP32 → Supabase → MQTT → Clients

**A. Motion Detection (PIR) with Photo:**
- Kiểm tra mỗi 500ms
- HIGH = phát hiện chuyển động:
  1. Chụp ảnh từ camera
  2. Upload lên Supabase Storage
  3. Nhận public URL
  4. Publish MQTT với URL
  5. Node-RED/Frontend hiển thị từ URL

**B. Doorbell Press with Photo:**
- User nhấn nút
- Phát chime sound
- Chụp ảnh → Upload Supabase → MQTT với URL

**C. Voice Recording:**
- Long press (≥3s) để ghi âm
- Record audio (max 10s)
- Upload WAV lên Supabase
- Nhận audio URL
- Publish MQTT với URL
- Frontend/Node-RED play từ URL

### 2. Clients → Supabase → MQTT → ESP32

**Frontend/Node-RED muốn ESP32 phát audio:**

1. **Upload audio lên Supabase:**
   ```javascript
   // Frontend/Node-RED
   const { data } = await supabase.storage
     .from('doorbell-media')
     .upload('audio/messages/alert.mp3', audioFile)
   
   const url = data.publicUrl
   ```

2. **Gửi command qua MQTT:**
   ```json
   {
     "action": "play_audio",
     "url": "https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/alert.mp3"
   }
   ```

3. **ESP32 xử lý:**
   - Nhận MQTT command
   - Download audio từ URL vào PSRAM
   - Play qua I2S speaker
   - Free memory sau khi phát xong
   - Confirm qua MQTT

### 3. Temperature & Distance (Unchanged)

**Temperature (Thermistor):**
- Đọc ADC từ thermistor (NTC 10kΩ)
- Tính điện trở từ voltage divider
- Áp dụng Steinhart-Hart equation để tính nhiệt độ Celsius
- Công thức: 1/T = 1/T₀ + (1/B) × ln(R/R₀)
- Publish mỗi 30s

**Distance (IR Sensor):**
- Đọc ADC từ IR distance sensor
- Linear interpolation giữa các điểm calibration
- Chuyển đổi sang cm
- Publish mỗi 10s

## Supabase Storage Setup

### Tạo Bucket
```sql
-- Via Supabase Dashboard hoặc SQL
INSERT INTO storage.buckets (id, name, public)
VALUES ('doorbell-media', 'doorbell-media', true);
```

### RLS Policies
```sql
-- Public read access
CREATE POLICY "Public read access"
ON storage.objects FOR SELECT
USING (bucket_id = 'doorbell-media');

-- Authenticated uploads (ESP32 uses anon key with this policy)
CREATE POLICY "Authenticated upload"
ON storage.objects FOR INSERT
WITH CHECK (bucket_id = 'doorbell-media');

-- Authenticated delete (for cleanup)
CREATE POLICY "Authenticated delete"
ON storage.objects FOR DELETE
USING (bucket_id = 'doorbell-media');
```

### Folder Structure
```
doorbell-media/
├── images/
│   └── ESP32_A1B2C3D4/
│       ├── doorbell/
│       │   ├── 123456.jpg
│       │   └── 123789.jpg
│       ├── security/
│       │   ├── 123500_1.jpg
│       │   ├── 123500_2.jpg
│       │   └── 123500_3.jpg
│       └── remote/
│           └── 135000.jpg
└── audio/
    ├── ESP32_A1B2C3D4/
    │   └── voicenotes/
    │       ├── 128456.wav
    │       └── 128789.wav
    └── messages/
        ├── alert.mp3
        ├── welcome.mp3
        └── custom_message.wav
```

## Testing với MQTT Explorer

Có thể test bằng MQTT Explorer hoặc mosquitto_sub:

```bash
mosquitto_sub -h aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud \
  -p 8883 -u doorbell -P Hcmus123 \
  -t "doorbell/#" --capath /etc/ssl/certs/
```

**Subscribe các topic để xem URLs:**
- `doorbell/status` - Xem doorbell press + image URLs, voice note + audio URLs
- `doorbell/security` - Xem PIR alerts + image URLs (single or burst)
- `doorbell/sensors/motion` - Motion detection với image URLs
- `doorbell/telemetry` - Temperature, distance, Supabase stats

**Publish test command với Supabase URL:**
```bash
# Test play audio from Supabase
mosquitto_pub -h aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud \
  -p 8883 -u doorbell -P Hcmus123 \
  -t "doorbell/command" \
  -m '{"action":"play_audio","url":"https://{PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/alert.mp3"}'

# Test remote capture
mosquitto_pub -h aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud \
  -p 8883 -u doorbell -P Hcmus123 \
  -t "doorbell/command" \
  -m '{"action":"capture"}'
```

## Testing Flow End-to-End

### 1. Test ESP32 Upload → MQTT → Client Access

```bash
# Terminal 1: Subscribe to see URLs
mosquitto_sub -h ... -t "doorbell/status"

# Terminal 2: Trigger doorbell press on ESP32
# Expected output in Terminal 1:
{
  "event": "press",
  "chime": "style1",
  "image_url": "https://...supabase.co/storage/.../123456.jpg",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123456
}

# Terminal 3: Access image URL
curl "https://...supabase.co/storage/.../123456.jpg" -o test.jpg
# Should download the actual doorbell press photo
```

### 2. Test Client Upload → MQTT → ESP32 Play

```javascript
// Frontend/Node-RED: Upload audio
const { data } = await supabase.storage
  .from('doorbell-media')
  .upload('audio/messages/test.mp3', audioFile)

const publicUrl = `https://${PROJECT_REF}.supabase.co/storage/v1/object/public/doorbell-media/audio/messages/test.mp3`

// Send to ESP32 via MQTT
mqttClient.publish('doorbell/command', JSON.stringify({
  action: 'play_audio',
  url: publicUrl
}))

// Listen for confirmation
mqttClient.subscribe('doorbell/status')
// Expected: {"event":"audio_playback","status":"completed","url":"..."}
```

## Architecture Diagram

```
┌─────────────┐                    ┌──────────────────┐
│   ESP32     │                    │   Supabase       │
│             │                    │   Storage        │
│  - Camera   │──── HTTPS PUT ────▶│                  │
│  - Mic      │◀─── URL Return ────│  - Images        │
│  - Speaker  │                    │  - Audio         │
└──────┬──────┘                    └──────────────────┘
       │                                     ▲
       │ MQTT (URLs)                        │ HTTPS GET
       │                                     │
       ▼                                     │
┌──────────────┐                    ┌───────┴──────────┐
│   HiveMQ     │◀──── Subscribe ────│  Frontend/       │
│   Cloud      │                    │  Node-RED        │
│              │───── Command ─────▶│                  │
└──────────────┘     (with URLs)    └──────────────────┘
```

**Data Flow:**
1. ESP32 captures → uploads to Supabase → gets URL
2. ESP32 publishes URL via MQTT to HiveMQ
3. Clients subscribe to MQTT → receive URLs
4. Clients access media directly from Supabase URLs
5. Clients upload to Supabase → send URL via MQTT
6. ESP32 downloads from URL → plays audio

## Troubleshooting

### ESP32 không upload được lên Supabase
- Kiểm tra `SUPABASE_URL` và `SUPABASE_ANON_KEY` trong config.h
- Verify Supabase bucket name: `doorbell-media`
- Check RLS policies: public read, authenticated insert
- Monitor Serial output cho HTTPS response codes

### ESP32 không download được audio từ URL
- Verify URL format chính xác
- Check Supabase file có public access không
- Ensure ESP32 có đủ PSRAM để buffer audio
- Monitor heap memory trước/sau download

### Node-RED/Frontend không truy cập được URLs
- Verify URLs có prefix `public` trong path
- Check CORS settings trong Supabase (nếu cần)
- Test URLs trực tiếp trong browser
- Ensure bucket có public read policy

### MQTT messages không có URLs
- Check upload_client trả về URL chính xác
- Verify JSON parsing trong mqtt_service
- Monitor Serial cho upload response
- Check ArduinoJson buffer size đủ lớn
- `doorbell/sensors/temperature`
- `doorbell/sensors/distance`
- `doorbell/telemetry`
