# Node-RED Flow 3 Setup Guide: Voice Note Recording

## Overview - Luồng Ghi Âm

**Flow 3** xử lý ghi âm khi người dùng giữ nút chuông > 3 giây:

```
ESP32 (Long Press > 3s) → Record Audio (WAV) → Upload Supabase Storage
                                                 ↓
                                            Get audio_url
                                                 ↓
                                    MQTT: doorbell/evt/voice
                                                 ↓
                                            Node-RED Flow 3
                                                 ↓
                                    Insert vào Supabase Database (events table)
                                                 ↓
                                    Frontend Tab "Hộp thư thoại"
```

---

## Thành Phần Flow 3

### 1. MQTT Topic

- **Topic**: `doorbell/evt/voice`
- **QoS**: 1
- **Payload Format** (JSON):
  ```json
  {
    "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/voice_1735470000.wav",
    "timestamp": 1735470000,
    "device_id": "ESP32_DOORBELL",
    "duration_ms": 3000
  }
  ```

### 2. Node-RED Nodes

#### Node 1: `mqtt_voice_in` (MQTT Input)
- **Type**: mqtt in
- **Name**: Voice Note Event
- **Topic**: `doorbell/evt/voice`
- **QoS**: 1
- **Datatype**: JSON
- **Purpose**: Nhận thông báo voice note từ ESP32

#### Node 2: `prepare_voice_insert` (Function)
- **Name**: Prepare Voice Insert
- **Code**:
  ```javascript
  const payload = msg.payload;

  if (!payload.audio_url || !payload.timestamp) {
      node.warn('Invalid voice note payload');
      return null;
  }

  msg.payload = {
      event_type: 'voice_note',
      audio_url: payload.audio_url,
      created_at: new Date(payload.timestamp * 1000).toISOString(),
      metadata: {
          source: 'doorbell_microphone',
          device_id: payload.device_id || 'ESP32_DOORBELL',
          duration_ms: payload.duration_ms || 0,
          raw_timestamp: payload.timestamp
      }
  };

  msg.headers = {
      'apikey': 'YOUR_SUPABASE_ANON_KEY',
      'Authorization': 'Bearer YOUR_SUPABASE_ANON_KEY',
      'Content-Type': 'application/json',
      'Prefer': 'return=representation'
  };

  return msg;
  ```
- **Purpose**: Chuẩn bị payload để insert vào database

#### Node 3: `insert_voice_db` (HTTP Request)
- **Type**: http request
- **Name**: Insert Voice to DB
- **Method**: POST
- **URL**: `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events`
- **Purpose**: Insert bản ghi vào bảng `events` với `event_type = 'voice_note'`

#### Node 4: `debug_voice` (Debug)
- **Type**: debug
- **Name**: Voice DB Response
- **Purpose**: Hiển thị response từ Supabase để debug

---

## Database Schema

### Bảng: `events`

```sql
CREATE TABLE events (
    id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    event_type TEXT NOT NULL,
    image_url TEXT,
    audio_url TEXT,  -- URL của file ghi âm
    metadata JSONB,
    description TEXT
);
```

**Ví dụ record**:
```json
{
  "id": 15,
  "created_at": "2025-12-29T12:30:00Z",
  "event_type": "voice_note",
  "image_url": null,
  "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/voice_1735470000.wav",
  "metadata": {
    "source": "doorbell_microphone",
    "device_id": "ESP32_DOORBELL",
    "duration_ms": 3000,
    "raw_timestamp": 1735470000
  },
  "description": "Voice note recorded"
}
```

---

## ESP32 Code Logic

### Trigger: Long Press Button > 3 seconds

File: `firmware-esp32/src/doorbell_features.cpp`

```cpp
void startVoiceNoteRecording() {
    Serial.println("[FEATURE] Starting voice note recording");
    
    // Start recording for VOICE_NOTE_DURATION (default 10s)
    startRecording(VOICE_NOTE_DURATION);
    
    // Wait for recording to complete
    delay(100);
    
    if (isRecordingReady() && getRecordedBytes() > 0) {
        // Create WAV file with header
        uint8_t wavHeader[44];
        uint32_t dataSize = getRecordedBytes();
        // ... (WAV header creation) ...
        
        // Upload to Supabase Storage
        bool uploaded = uploadWithRetry(ENDPOINT_VOICE_NOTE, wavBuffer, 44 + dataSize, "audio/wav", 3,
                                      "voice_note", getTimestamp());
        
        if (uploaded) {
            // Get URL from upload response
            const char* audioUrl = getLastUploadedUrl();
            
            // Publish to MQTT
            char msg[512];
            snprintf(msg, sizeof(msg), 
                "{\"audio_url\":\"%s\",\"timestamp\":%lu,\"device_id\":\"ESP32_DOORBELL\",\"duration_ms\":%lu}", 
                audioUrl, getTimestamp(), (unsigned long)(dataSize * 1000 / (MIC_SAMPLE_RATE * 2)));
            mqttPublishJson(TOPIC_EVT_VOICE, msg);
            Serial.printf("[FEATURE] Published voice note to MQTT: %s\n", audioUrl);
        }
    }
}
```

### Config Topic

File: `firmware-esp32/src/config.h`

```cpp
#define TOPIC_EVT_VOICE "doorbell/evt/voice"
```

---

## Supabase Storage Configuration

### Bucket: `bell-audio`

1. **Tạo bucket** (nếu chưa có):
   - Vào Supabase Dashboard → Storage
   - Click "New Bucket"
   - Name: `bell-audio`
   - ✅ Public bucket (allow public access for audio playback)

2. **Set Policies**:
   ```sql
   -- Allow public read access
   CREATE POLICY "Public read access"
   ON storage.objects FOR SELECT
   USING (bucket_id = 'bell-audio');

   -- Allow authenticated insert (ESP32 uses anon key with RLS disabled)
   CREATE POLICY "Allow insert for all"
   ON storage.objects FOR INSERT
   WITH CHECK (bucket_id = 'bell-audio');
   ```

3. **File naming format**: `voice_{timestamp}.wav`
   - Example: `voice_1735470000.wav`

---

## Frontend Integration

### Tab "Hộp thư thoại" (Voice Messages)

File: `frontend_react/src/pages/SecurityDashboard/VoiceMessagesTab.jsx`

**Fetch voice notes từ Supabase**:
```javascript
const fetchVoiceMessages = async () => {
  try {
    const { data, error } = await supabase
      .from('events')
      .select('*')
      .eq('event_type', 'voice_note')
      .order('created_at', { ascending: false })
      .limit(20);
    
    if (error) throw error;
    
    const formatted = data.map(msg => ({
      id: msg.id,
      audio_url: msg.audio_url,
      timestamp: new Date(msg.created_at).toLocaleString('vi-VN'),
      duration_ms: msg.metadata?.duration_ms || 0,
      device_id: msg.metadata?.device_id || 'Unknown'
    }));
    
    setVoiceMessages(formatted);
  } catch (err) {
    console.error('Error fetching voice messages:', err);
  }
};
```

**Hiển thị audio player**:
```jsx
{voiceMessages.map(msg => (
  <div key={msg.id} className="voice-message-card">
    <audio controls src={msg.audio_url}>
      Your browser does not support audio playback.
    </audio>
    <p>{msg.timestamp}</p>
    <p>Duration: {(msg.duration_ms / 1000).toFixed(1)}s</p>
  </div>
))}
```

---

## Testing Flow 3

### 1. Test ESP32 Upload

Kiểm tra ESP32 có upload file WAV lên Supabase:

```bash
# Check serial monitor output
[FEATURE] Starting voice note recording
[FEATURE] Uploading voice note...
[SUPABASE] Uploading to: https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/bell-audio/voice_1735470000.wav
[SUPABASE] Response code: 200
[SUPABASE] Public URL: https://...supabase.co/storage/v1/object/public/bell-audio/voice_1735470000.wav
[FEATURE] Voice note uploaded successfully
[FEATURE] Published voice note to MQTT: https://...
```

### 2. Test MQTT Message

Sử dụng MQTT client để kiểm tra:

```bash
# Subscribe to voice topic
mqtt sub -h 1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud -p 8883 \
  -u "doorbell_user" -pw "your_password" \
  -t "doorbell/evt/voice" --tls-version tlsv1.2
```

Khi ESP32 publish, bạn sẽ thấy:
```json
{
  "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/voice_1735470000.wav",
  "timestamp": 1735470000,
  "device_id": "ESP32_DOORBELL",
  "duration_ms": 3000
}
```

### 3. Test Node-RED Flow

1. **Mở Node-RED**: http://localhost:1880
2. **Import Flow**: Copy nội dung từ `flows.json` tab "Flow 3"
3. **Deploy**: Click nút "Deploy" (đỏ, góc trên bên phải)
4. **Xem Debug**: Mở tab Debug bên phải
5. **Trigger**: Giữ nút chuông trên ESP32 > 3 giây
6. **Kết quả**: Xem debug node hiển thị response từ Supabase

### 4. Test Database Insert

Kiểm tra record được insert:

```sql
-- Query latest voice notes
SELECT * FROM events 
WHERE event_type = 'voice_note'
ORDER BY created_at DESC
LIMIT 5;
```

Kết quả mong đợi:
```
| id | created_at          | event_type | audio_url                                    | metadata                          |
|----|---------------------|------------|----------------------------------------------|-----------------------------------|
| 15 | 2025-12-29 12:30:00 | voice_note | https://.../bell-audio/voice_1735470000.wav  | {"device_id": "ESP32_DOORBELL"}   |
```

### 5. Test Frontend Display

1. **Chạy frontend**: `npm run dev` trong folder `frontend_react`
2. **Mở browser**: http://localhost:5173
3. **Vào tab "Hộp thư thoại"**
4. **Xem danh sách voice notes**: Hiển thị audio player cho mỗi bản ghi
5. **Test playback**: Click play button trên audio player

---

## Troubleshooting

### Issue 1: Audio không upload lên Supabase

**Triệu chứng**:
```
[SUPABASE] Response code: 400
[SUPABASE] Error response: {"error":"Invalid bucket"}
```

**Giải pháp**:
1. Kiểm tra bucket `bell-audio` đã tạo trong Supabase
2. Kiểm tra policy cho phép insert vào bucket
3. Verify `SUPABASE_URL` và `SUPABASE_ANON_KEY` trong `config.h`

### Issue 2: MQTT message không đến Node-RED

**Triệu chứng**: Debug node trong Node-RED không hiển thị message

**Giải pháp**:
1. Kiểm tra MQTT broker connection trong Node-RED
2. Verify topic name: `doorbell/evt/voice` (không dấu cách, lowercase)
3. Kiểm tra ESP32 serial monitor xem có publish không
4. Test với MQTT client external (MQTT Explorer, mqtt.js)

### Issue 3: Database insert failed

**Triệu chứng**:
```json
{
  "error": "null value in column \"description\" violates not-null constraint"
}
```

**Giải pháp**:
Option 1: Update prepare_voice_insert function thêm field `description`:
```javascript
msg.payload = {
    event_type: 'voice_note',
    audio_url: payload.audio_url,
    description: 'Voice note recorded',  // ADD THIS
    created_at: new Date(payload.timestamp * 1000).toISOString(),
    metadata: { ... }
};
```

Option 2: Modify database schema:
```sql
ALTER TABLE events ALTER COLUMN description DROP NOT NULL;
```

### Issue 4: Audio không play trên frontend

**Triệu chứng**: Audio player hiển thị nhưng không play được

**Giải pháp**:
1. Kiểm tra bucket `bell-audio` đã set public
2. Test URL trực tiếp trong browser
3. Kiểm tra CORS policy trong Supabase
4. Verify audio file format (WAV, 16kHz, mono)

---

## Summary

**Flow 3** hoàn thiện luồng voice note recording:

✅ **ESP32**: Long press > 3s → Record WAV → Upload Supabase → Publish MQTT  
✅ **Node-RED**: Receive `doorbell/evt/voice` → Insert database  
✅ **Frontend**: Tab "Hộp thư thoại" → Display audio player  
✅ **Database**: Store audio_url with metadata  

**Next Steps**:
1. Deploy flows.json vào Node-RED
2. Flash ESP32 với code mới
3. Test end-to-end bằng cách giữ nút chuông
4. Verify audio playback trên frontend
