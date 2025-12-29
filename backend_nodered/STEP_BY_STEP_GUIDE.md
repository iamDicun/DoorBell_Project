# 🛠️ Node-RED Implementation Guide - Step by Step

**Project:** DoorBell Backend  
**Date:** December 30, 2025  
**Base URL:** `http://localhost:1880/api`

---

## 📋 Prerequisites

### 1. Thông tin từ ESP32 config.h
```
MQTT Broker: 1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud
MQTT Port: 8883 (TLS)
MQTT Username: esp_doorbell
MQTT Password: Hcmus123

Supabase URL: https://xznnnklhqkccylxxzsdh.supabase.co
Supabase Anon Key: eyJhbGci...AhEE1Z8
```

### 2. MQTT Topics cần implement
**ESP32 → Node-RED (Subscribers):**
- ✅ `doorbell/evt/button` (đã có)
- ✅ `doorbell/evt/pir` (đã có)
- ✅ `doorbell/evt/voice` (đã có)
- ✅ `doorbell/sensor/temp` (đã có)
- ❌ `doorbell/evt/snapshot` (cần thêm)

**Node-RED → ESP32 (Publishers):**
- ❌ `doorbell/cmd/snapshot` (cần thêm)
- ❌ `doorbell/cmd/speak` (cần thêm)
- ❌ `doorbell/cmd/siren` (cần thêm)
- ❌ `doorbell/cmd/settings` (cần thêm)

---

## 📤 File Upload Workflow

**Quan trọng:** Frontend KHÔNG upload file qua Node-RED. Luồng đúng:

```
┌─────────────┐
│  Frontend   │
│  (React)    │
└──────┬──────┘
       │ 1. Upload file (HTTP POST multipart/form-data)
       ▼
┌─────────────────────────────┐
│  Supabase Storage           │
│  - Bucket: bell-audio       │
│  - Bucket: bell-images      │
└──────┬──────────────────────┘
       │ 2. Return public URL
       ▼
┌─────────────┐
│  Frontend   │ Store URL in state
└──────┬──────┘
       │ 3. Call Node-RED API with URL
       ▼
┌─────────────────────────────┐
│  Node-RED                   │
│  POST /api/commands/speak   │
│  Body: { audio_url: "..." } │
└──────┬──────────────────────┘
       │ 4. Publish MQTT
       ▼
┌─────────────────────────────┐
│  MQTT Topic                 │
│  doorbell/cmd/speak         │
│  Payload: { audio_url: ... }│
└──────┬──────────────────────┘
       │ 5. Subscribe & receive
       ▼
┌─────────────┐
│   ESP32     │ Download & play audio
└─────────────┘
```

### Frontend Upload Code Example

**1. Upload Audio File to Supabase Storage:**

```javascript
// Frontend: src/lib/api.js hoặc component
import { supabase } from './supabase';

export async function uploadAudioFile(audioBlob) {
  // Generate unique filename
  const timestamp = Date.now();
  const fileName = `message_${timestamp}.wav`;
  
  // Upload to Supabase Storage
  const { data, error } = await supabase.storage
    .from('bell-audio')
    .upload(fileName, audioBlob, {
      contentType: 'audio/wav',
      cacheControl: '3600',
      upsert: false
    });

  if (error) {
    throw new Error(`Upload failed: ${error.message}`);
  }

  // Get public URL
  const { data: urlData } = supabase.storage
    .from('bell-audio')
    .getPublicUrl(fileName);

  return {
    fileName: fileName,
    fileUrl: urlData.publicUrl,
    filePath: data.path
  };
}

// 2. Send command to Node-RED
export async function sendAudioMessage(audioUrl, volume = 80) {
  const response = await fetch('http://localhost:1880/api/commands/speak', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      audio_url: audioUrl,
      volume: volume,
      message_type: 'custom'
    })
  });

  if (!response.ok) {
    throw new Error('Failed to send audio command');
  }

  return await response.json();
}

// 3. Complete workflow in React component
async function handleSendAudio(audioBlob) {
  try {
    // Step 1: Upload to Supabase Storage
    setLoading(true);
    const { fileUrl } = await uploadAudioFile(audioBlob);
    
    // Step 2: Send URL to Node-RED
    const result = await sendAudioMessage(fileUrl, 80);
    
    console.log('Audio command sent:', result);
    // result = {
    //   success: true,
    //   command_id: "cmd_1735490600_speak",
    //   audio_url: "https://..."
    // }
    
    setLoading(false);
  } catch (error) {
    console.error('Error:', error);
    setLoading(false);
  }
}
```

**2. Upload Image File (for testing snapshot display):**

```javascript
export async function uploadImageFile(imageBlob) {
  const timestamp = Date.now();
  const fileName = `test_${timestamp}.jpg`;
  
  const { data, error } = await supabase.storage
    .from('bell-images')
    .upload(fileName, imageBlob, {
      contentType: 'image/jpeg',
      cacheControl: '3600',
      upsert: false
    });

  if (error) {
    throw new Error(`Upload failed: ${error.message}`);
  }

  const { data: urlData } = supabase.storage
    .from('bell-images')
    .getPublicUrl(fileName);

  return urlData.publicUrl;
}
```

### Supabase Storage Setup

**1. Create Buckets (nếu chưa có):**

Vào Supabase Dashboard → Storage → Create Bucket:

```
Bucket name: bell-audio
Public bucket: ✅ Yes (để ESP32 có thể download)
Allowed MIME types: audio/wav, audio/mpeg, audio/mp3
Max file size: 5MB

Bucket name: bell-images  
Public bucket: ✅ Yes
Allowed MIME types: image/jpeg, image/jpg, image/png
Max file size: 2MB
```

**2. Storage Policies (RLS):**

```sql
-- Allow public read access
CREATE POLICY "Public Access"
ON storage.objects FOR SELECT
USING ( bucket_id = 'bell-audio' );

-- Allow authenticated uploads
CREATE POLICY "Authenticated Upload"
ON storage.objects FOR INSERT
WITH CHECK (
  bucket_id = 'bell-audio' 
  AND auth.role() = 'authenticated'
);

-- Same for bell-images
CREATE POLICY "Public Access Images"
ON storage.objects FOR SELECT
USING ( bucket_id = 'bell-images' );

CREATE POLICY "Authenticated Upload Images"
ON storage.objects FOR INSERT
WITH CHECK (
  bucket_id = 'bell-images'
  AND auth.role() = 'authenticated'
);
```

**3. Get Storage URLs:**

```javascript
// In frontend .env
VITE_SUPABASE_URL=https://xznnnklhqkccylxxzsdh.supabase.co
VITE_SUPABASE_ANON_KEY=eyJhbGci...

// Public URL format:
// https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_123.wav
// https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/snapshot_456.jpg
```

---

## 🎯 PHASE 1: Command APIs (Ưu tiên cao nhất)

### Flow 5: POST /api/commands/snapshot

**Mục đích:** Frontend gửi lệnh để ESP32 chụp ảnh

#### Bước 1: Tạo HTTP In Node
```
Node: HTTP In
Name: POST /api/commands/snapshot
Method: POST
URL: /api/commands/snapshot
```

#### Bước 2: Tạo Function Node - Generate Command ID
```javascript
// Node: Function - Generate Snapshot Command
const commandId = 'cmd_' + Date.now() + '_snapshot';
const timestamp = Math.floor(Date.now() / 1000);

// Prepare MQTT payload
msg.mqttPayload = {
    command_id: commandId,
    timestamp: timestamp
};

// Store for HTTP response
msg.commandId = commandId;
msg.timestamp = timestamp;

// Prepare MQTT topic
msg.topic = 'doorbell/cmd/snapshot';

return msg;
```

#### Bước 3: Tạo MQTT Out Node
```
Node: MQTT Out
Name: Publish Snapshot Command
Topic: (leave empty, set by msg.topic)
QoS: 1
Retain: false
Server: [Select your MQTT broker config]
```

**Payload to ESP32:**
```json
{
  "command_id": "cmd_1735490600_snapshot",
  "timestamp": 1735490600
}
```

#### Bước 4: Tạo Function Node - Format HTTP Response
```javascript
// Node: Function - Format Snapshot Response
msg.statusCode = 200;
msg.payload = {
    success: true,
    message: "Snapshot command sent to ESP32",
    command_id: msg.commandId,
    timestamp: msg.timestamp
};

return msg;
```

#### Bước 5: Tạo HTTP Response Node
```
Node: HTTP Response
Name: Return Snapshot Response
```

#### Bước 6: Connect các nodes
```
[HTTP In] → [Generate Command ID] ──┬→ [MQTT Out] (terminal)
                                    │
                                    └→ [Format Response] → [HTTP Response]
```

**Cách connect trong Node-RED:**
1. Kéo wire từ "Generate Command ID" → "MQTT Out"
2. Kéo wire thứ 2 từ "Generate Command ID" → "Format Response"  
   *(Một node có thể có nhiều wires ra)*

#### Test:
```bash
curl -X POST http://localhost:1880/api/commands/snapshot \
  -H "Content-Type: application/json"
```

**Expected Response:**
```json
{
  "success": true,
  "message": "Snapshot command sent to ESP32",
  "command_id": "cmd_1735490600_snapshot",
  "timestamp": 1735490600
}
```

---

### Flow 6: MQTT In - doorbell/evt/snapshot (Response Handler)

**Mục đích:** Nhận kết quả chụp ảnh từ ESP32 và lưu vào database

#### Bước 1: Tạo MQTT In Node
```
Node: MQTT In
Name: ESP32 Snapshot Response
Topic: doorbell/evt/snapshot
QoS: 1
Server: [Select your MQTT broker config]
```

#### Bước 2: Tạo Function Node - Parse Snapshot Response
```javascript
// Node: Function - Parse Snapshot Response
const payload = msg.payload;

// Validate payload
if (!payload.image_url || !payload.timestamp) {
    node.warn('Invalid snapshot response payload');
    return null;
}

// Prepare Supabase insert
msg.payload = {
    event_type: 'snapshot',
    image_url: payload.image_url,
    description: 'Chụp ảnh thủ công',
    created_at: new Date(payload.timestamp * 1000).toISOString(),
    metadata: {
        source: 'manual_command',
        command_id: payload.command_id || null,
        raw_timestamp: payload.timestamp
    }
};

// Prepare Supabase headers
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

return msg;
```

#### Bước 3: Tạo HTTP Request Node - Insert to Supabase
```
Node: HTTP Request
Name: Insert Snapshot to DB
Method: POST
URL: https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events
Return: a UTF-8 string
```

#### Bước 4: Tạo Debug Node
```
Node: Debug
Name: Snapshot Saved
Output: msg.payload
```

#### Bước 5: Connect
```
[MQTT In: doorbell/evt/snapshot] 
    → [Parse Response] 
    → [Insert to Supabase] 
    → [Debug]
```

---

### Flow 7: POST /api/commands/speak

**Mục đích:** Frontend gửi audio URL để ESP32 phát qua loa

#### Bước 1: HTTP In Node
```
Node: HTTP In
Name: POST /api/commands/speak
Method: POST
URL: /api/commands/speak
```

#### Bước 2: Function Node - Validate & Prepare
```javascript
// Node: Function - Validate Speak Command
const payload = msg.payload;

// Validation
if (!payload.audio_url) {
    msg.statusCode = 400;
    msg.payload = {
        success: false,
        error: "Validation failed",
        code: "VALIDATION_ERROR",
        details: {
            field: "audio_url",
            message: "audio_url is required"
        }
    };
    return [null, msg]; // Send to error output
}

// Validate volume
const volume = payload.volume || 80;
if (volume < 0 || volume > 100) {
    msg.statusCode = 400;
    msg.payload = {
        success: false,
        error: "Validation failed",
        code: "VALIDATION_ERROR",
        details: {
            field: "volume",
            message: "volume must be between 0 and 100"
        }
    };
    return [null, msg];
}

// Generate command
const commandId = 'cmd_' + Date.now() + '_speak';
const timestamp = Math.floor(Date.now() / 1000);

// MQTT payload
msg.mqttPayload = {
    command_id: commandId,
    audio_url: payload.audio_url,
    volume: volume,
    timestamp: timestamp
};

msg.commandId = commandId;
msg.audioUrl = payload.audio_url;
msg.volume = volume;
msg.timestamp = timestamp;
msg.topic = 'doorbell/cmd/speak';

return [msg, null]; // Send to success output
```

**Configure outputs:** 2 outputs (success, error)

#### Bước 3: MQTT Out Node
```
Node: MQTT Out
Name: Publish Speak Command
Topic: (set by msg.topic)
QoS: 1
```

#### Bước 4: Function Node - Save to Quick Responses
```javascript
// Node: Function - Save to Quick Responses DB
// Extract filename from URL for title
const audioUrl = msg.audioUrl;
const fileName = audioUrl.split('/').pop(); // Get "message_1767040515399.wav"
const title = fileName.replace('.wav', '').replace('message_', 'Tin nhắn #');

// Prepare insert payload
msg.payload = JSON.stringify({
    title: title,
    audio_url: audioUrl,
    is_active: true
});

msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

return msg;
```

#### Bước 5: HTTP Request Node - Insert to Supabase
```
Node: HTTP Request
Name: Insert Quick Response
Method: POST
URL: https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/quick_responses
Return: a UTF-8 string
```

#### Bước 6: Function Node - Format Success Response
```javascript
// Node: Function - Format Speak Response
msg.statusCode = 200;
msg.payload = {
    success: true,
    message: "Audio playback command sent to ESP32",
    command_id: msg.commandId,
    audio_url: msg.audioUrl,
    volume: msg.volume,
    timestamp: msg.timestamp
};

return msg;
```

#### Bước 7: HTTP Response (Success)
```
Node: HTTP Response
Name: Return Success
```

#### Bước 8: HTTP Response (Error)
```
Node: HTTP Response
Name: Return Error
```

#### Connect:
```
[HTTP In] → [Validate] ──(output 1)──┬→ [MQTT Out] (terminal)
                        │            │
                        │            └→ [Save to Quick Responses] 
                        │                    → [HTTP Request Insert DB]
                        │                    → [Format Success] 
                        │                    → [HTTP Response Success]
                        │
                        └──(output 2)──→ [HTTP Response Error]
```

**⚠️ Lưu ý quan trọng:**
- MQTT Out là terminal node (không có output port)
- Validate output 1 kéo 2 dây: một → MQTT Out, một → Save to Quick Responses
- Save to DB chain: Save Function → HTTP Request → Format Response → HTTP Response
- Output 2 của Validate đi thẳng HTTP Response Error

**Workflow hoàn chỉnh:**
1. Frontend upload audio blob → Supabase Storage
2. Frontend lấy public URL từ Storage (e.g., `https://.../bell-audio/message_1767040515399.wav`)
3. Frontend POST `/api/commands/speak` với `audio_url`
4. Node-RED validate input
5. **Node-RED publish MQTT** với `audio_url` (song song với step 6)
6. **Node-RED save to `quick_responses` table** với title + audio_url
7. ESP32 download audio từ URL và play

**Database Schema: `quick_responses`**
```sql
CREATE TABLE public.quick_responses (
  id bigint GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
  created_at timestamp with time zone DEFAULT timezone('utc'::text, now()),
  title text NOT NULL,
  audio_url text NOT NULL,
  is_active boolean DEFAULT true
);
```

**Example Record:**
```json
{
  "id": 1,
  "title": "Tin nhắn #1767040515399",
  "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_1767040515399.wav",
  "is_active": true,
  "created_at": "2025-12-30T03:15:15.399Z"
}
```

#### Test:
```bash
# Test với URL thật từ Supabase Storage
curl -X POST http://localhost:1880/api/commands/speak \
  -H "Content-Type: application/json" \
  -d '{
    "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_1735566000000.wav",
    "volume": 80
  }'

# Expected Response:
# {
#   "success": true,
#   "message": "Audio playback command sent to ESP32",
#   "command_id": "cmd_1735566001234_speak",
#   "audio_url": "https://...",
#   "volume": 80,
#   "timestamp": 1735566001
# }
```

**Test MQTT với MQTT Explorer:**
```
Topic: doorbell/cmd/speak
Payload:
{
  "command_id": "cmd_1735566001234_speak",
  "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message.wav",
  "volume": 80,
  "timestamp": 1735566001
}
```

---

### Flow 8: POST /api/commands/siren

**Mục đích:** Bật/tắt còi báo động

#### Bước 1: HTTP In Node
```
Node: HTTP In
Name: POST /api/commands/siren
Method: POST
URL: /api/commands/siren
```

#### Bước 2: Function Node - Validate Siren Command
```javascript
// Node: Function - Validate Siren Command
const payload = msg.payload;

// Validate action
if (!payload.action || (payload.action !== 'on' && payload.action !== 'off')) {
    msg.statusCode = 400;
    msg.payload = {
        success: false,
        error: "Validation failed",
        code: "VALIDATION_ERROR",
        details: {
            field: "action",
            message: "action must be 'on' or 'off'"
        }
    };
    return [null, msg]; // Error output
}

// Validate duration
let duration = payload.duration || 5;
if (duration < 1 || duration > 30) {
    duration = 5; // Default to 5 seconds
}

// Generate command
const commandId = 'cmd_' + Date.now() + '_siren';
const timestamp = Math.floor(Date.now() / 1000);

// MQTT payload
msg.mqttPayload = {
    command_id: commandId,
    action: payload.action,
    duration: duration,
    timestamp: timestamp
};

msg.commandId = commandId;
msg.sirenAction = payload.action;
msg.duration = duration;
msg.timestamp = timestamp;
msg.topic = 'doorbell/cmd/siren';

return [msg, null]; // Success output
```

#### Bước 3: MQTT Out Node
```
Node: MQTT Out
Name: Publish Siren Command
Topic: (set by msg.topic)
QoS: 1
```

#### Bước 4: Function Node - Update Alarm Setting
```javascript
// Node: Function - Prepare DB Update
// Update alarm_enabled in device_settings
const alarmEnabled = (msg.sirenAction === 'on');

// Prepare payload for DB update
msg.payload = {
    alarm_enabled: alarmEnabled,
    updated_at: new Date().toISOString()
};

// Prepare headers for Supabase
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

return msg;
```

#### Bước 5: HTTP Request Node
```
Node: HTTP Request
Name: Update Device Settings
Method: PATCH
URL: (leave empty - set by msg.url)
Return: a UTF-8 string
```

**⚠️ Important Configuration:**
- Method: Select "PATCH" from dropdown
- URL: Leave empty (will be set by msg.url from previous function)
- Return: "a UTF-8 string"
- Nếu vẫn lỗi "Invalid URL", thử set URL cố định: `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/device_settings?id=eq.1`

#### Bước 6: Function Node - Format Success Response
```javascript
// Node: Function - Format Siren Response
msg.statusCode = 200;
msg.payload = {
    success: true,
    message: "Siren command sent to ESP32",
    command_id: msg.commandId,
    action: msg.sirenAction,
    duration: msg.duration,
    timestamp: msg.timestamp
};

return msg;
```

#### Bước 7: HTTP Response Nodes
```
Success: HTTP Response
Error: HTTP Response
```

#### Connect:
```
[HTTP In] → [Validate] ──(output 1)──┬→ [MQTT Out] (terminal)
                        │            │
                        │            └→ [Update DB Prepare] 
                        │                    → [HTTP Request Update DB] 
                        │                    → [Format Response] 
                        │                    → [HTTP Response Success]
                        │
                        └──(output 2)──→ [HTTP Response Error]
```

**⚠️ Lưu ý:**
- MQTT Out là terminal node (không có output)
- Validate output 1 kéo 2 dây: một → MQTT Out, một → Update DB Prepare
- Update DB chain: Prepare → HTTP Request → Format → Response
- Validate output 2 → HTTP Response Error

#### Test:

**Windows PowerShell:**
```powershell
# Turn siren on
Invoke-RestMethod -Uri "http://localhost:1880/api/commands/siren" `
  -Method POST `
  -ContentType "application/json" `
  -Body '{"action":"on","duration":5}'

# Turn siren off
Invoke-RestMethod -Uri "http://localhost:1880/api/commands/siren" `
  -Method POST `
  -ContentType "application/json" `
  -Body '{"action":"off"}'
```

**Linux/Mac (curl):**
```bash
# Turn siren on
curl -X POST http://localhost:1880/api/commands/siren \
  -H "Content-Type: application/json" \
  -d '{"action":"on","duration":5}'

# Turn siren off
curl -X POST http://localhost:1880/api/commands/siren \
  -H "Content-Type: application/json" \
  -d '{"action":"off"}'
```

**Expected Response:**
```json
{
  "success": true,
  "message": "Siren command sent to ESP32",
  "command_id": "cmd_1735566002345_siren",
  "action": "on",
  "duration": 5,
  "timestamp": 1735566002
}
```

**Verify MQTT + DB:**
1. Check MQTT topic `doorbell/cmd/siren` có message
2. Check database `device_settings.alarm_enabled` = true/false

---

## 🎯 PHASE 2: Settings API

### Flow 9: GET /api/settings

#### Bước 1: HTTP In Node
```
Node: HTTP In
Name: GET /api/settings
Method: GET
URL: /api/settings
```

#### Bước 2: Function Node - Build Query
```javascript
// Node: Function - Build Settings Query
msg.url = 'https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/device_settings?id=eq.1';
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8'
};

return msg;
```

#### Bước 3: HTTP Request Node
```
Node: HTTP Request
Name: Query Settings from Supabase
Method: GET
URL: (set by msg.url)
```

#### Bước 4: Function Node - Format Response
```javascript
// Node: Function - Format Settings Response
let data = msg.payload;
if (typeof data === 'string') {
    try {
        data = JSON.parse(data);
    } catch (e) {
        data = [];
    }
}

if (Array.isArray(data) && data.length > 0) {
    msg.statusCode = 200;
    msg.payload = {
        success: true,
        data: data[0]
    };
} else {
    msg.statusCode = 404;
    msg.payload = {
        success: false,
        error: "Settings not found",
        code: "NOT_FOUND"
    };
}

return msg;
```

#### Bước 5: HTTP Response
```
Node: HTTP Response
Name: Return Settings
```

#### Connect:
```
[HTTP In] → [Build Query] → [Query DB] → [Format Response] → [HTTP Response]
```

---

### Flow 10: PATCH /api/settings

**Quan trọng:** Sau khi update DB, phải publish MQTT để ESP32 sync settings

#### Bước 1: HTTP In Node
```
Node: HTTP In
Name: PATCH /api/settings
Method: PATCH
URL: /api/settings
```

#### Bước 2: Function Node - Validate & Prepare Update
```javascript
// Node: Function - Validate Settings Update
const payload = msg.payload;

// Validate volume
if (payload.speaker_volume !== undefined) {
    if (payload.speaker_volume < 0 || payload.speaker_volume > 100) {
        msg.statusCode = 400;
        msg.payload = {
            success: false,
            error: "Validation failed",
            code: "VALIDATION_ERROR",
            details: {
                field: "speaker_volume",
                message: "speaker_volume must be between 0 and 100"
            }
        };
        return [null, msg]; // Error output
    }
}

// Build update object (only include provided fields)
const updateData = {
    updated_at: new Date().toISOString()
};

if (payload.alarm_enabled !== undefined) updateData.alarm_enabled = payload.alarm_enabled;
if (payload.speaker_volume !== undefined) updateData.speaker_volume = payload.speaker_volume;
if (payload.do_not_disturb !== undefined) updateData.do_not_disturb = payload.do_not_disturb;
if (payload.pir_enabled !== undefined) updateData.pir_enabled = payload.pir_enabled;
if (payload.notifications_enabled !== undefined) updateData.notifications_enabled = payload.notifications_enabled;

msg.url = 'https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/device_settings?id=eq.1';
msg.method = 'PATCH';
msg.payload = updateData;
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

return [msg, null];
```

#### Bước 3: HTTP Request Node
```
Node: HTTP Request
Name: Update Settings in DB
Method: (set by msg.method)
URL: (set by msg.url)
```

#### Bước 4: Function Node - Prepare MQTT Sync
```javascript
// Node: Function - Prepare Settings MQTT
let data = msg.payload;
if (typeof data === 'string') {
    try {
        data = JSON.parse(data);
    } catch (e) {
        data = [];
    }
}

// Get the updated settings
const settings = Array.isArray(data) ? data[0] : data;

// Prepare MQTT payload for ESP32
msg.mqttPayload = {
    speaker_volume: settings.speaker_volume,
    pir_enabled: settings.pir_enabled,
    alarm_enabled: settings.alarm_enabled,
    notifications_enabled: settings.notifications_enabled,
    do_not_disturb: settings.do_not_disturb,
    timestamp: Math.floor(Date.now() / 1000)
};

msg.topic = 'doorbell/cmd/settings';
msg.settingsData = settings; // Store for HTTP response

return msg;
```

#### Bước 5: MQTT Out Node
```
Node: MQTT Out
Name: Sync Settings to ESP32
Topic: (set by msg.topic)
QoS: 1
Retain: false
```

#### Bước 6: Function Node - Format HTTP Response
```javascript
// Node: Function - Format Settings Response
msg.statusCode = 200;
msg.payload = {
    success: true,
    message: "Settings updated",
    data: msg.settingsData
};

return msg;
```

#### Bước 7: HTTP Response
```
Success: HTTP Response
Error: HTTP Response
```

#### Connect:
```
[HTTP In] → [Validate] → [Update DB] → [Prepare MQTT] → [MQTT Out]
                ↓                                          ↓
           [Error Response]                    [Format Response] → [HTTP Response]
```

#### Test:
```bash
curl -X PATCH http://localhost:1880/api/settings \
  -H "Content-Type: application/json" \
  -d '{
    "speaker_volume": 75,
    "alarm_enabled": true,
    "pir_enabled": true
  }'
```

---

## 🎯 PHASE 3: Events API Enhancements

### Flow 11: Enhance GET /api/events

**Current:** Basic implementation exists  
**Need:** Add `offset` and `unread` query params

#### Update existing "Build Query" Function Node:
```javascript
// Node: Function - Build Query (UPDATED)
const limit = parseInt(msg.req.query.limit) || 50;
const offset = parseInt(msg.req.query.offset) || 0; // ADD THIS
const eventType = msg.req.query.type || null;
const unread = msg.req.query.unread === 'true'; // ADD THIS

let url = 'https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events?order=created_at.desc&limit=' + limit;

// Add offset
url += '&offset=' + offset;

// Filter by type
if (eventType) {
    url += '&event_type=eq.' + eventType;
}

// Filter by unread
if (unread) {
    url += '&is_read=eq.false';
}

msg.url = url;
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8'
};

// Store for response
msg.limit = limit;
msg.offset = offset;

return msg;
```

#### Update "Format Response" to include offset:
```javascript
// Node: Function - Format Response (UPDATED)
let data = msg.payload;
if (typeof data === 'string') {
    try {
        data = JSON.parse(data);
    } catch (e) {
        data = [];
    }
}

msg.payload = {
    success: true,
    data: data,
    count: Array.isArray(data) ? data.length : 0,
    limit: msg.limit,
    offset: msg.offset // ADD THIS
};
msg.statusCode = 200;
return msg;
```

---

### Flow 12: GET /api/events/:id

#### Bước 1: HTTP In Node
```
Node: HTTP In
Name: GET /api/events/:id
Method: GET
URL: /api/events/:id
```

#### Bước 2: Function Node - Extract ID & Build Query
```javascript
// Node: Function - Get Event by ID
const eventId = msg.req.params.id;

if (!eventId) {
    msg.statusCode = 400;
    msg.payload = {
        success: false,
        error: "Event ID is required",
        code: "VALIDATION_ERROR"
    };
    return [null, msg];
}

msg.url = 'https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events?id=eq.' + eventId;
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8'
};

return [msg, null];
```

#### Bước 3: HTTP Request
```
Node: HTTP Request
Name: Query Event
Method: GET
URL: (set by msg.url)
```

#### Bước 4: Function Node - Format Response
```javascript
// Node: Function - Format Event Response
let data = msg.payload;
if (typeof data === 'string') {
    try {
        data = JSON.parse(data);
    } catch (e) {
        data = [];
    }
}

if (Array.isArray(data) && data.length > 0) {
    msg.statusCode = 200;
    msg.payload = {
        success: true,
        data: data[0]
    };
} else {
    msg.statusCode = 404;
    msg.payload = {
        success: false,
        error: "Event not found",
        code: "NOT_FOUND"
    };
}

return msg;
```

#### Connect:
```
[HTTP In] → [Extract ID] → [Query DB] → [Format] → [HTTP Response]
                         ↓ (error)
                    [HTTP Response Error]
```

---

### Flow 13: PATCH /api/events/:id/read

#### Bước 1: HTTP In
```
Node: HTTP In
Name: PATCH /api/events/:id/read
Method: PATCH
URL: /api/events/:id/read
```

#### Bước 2: Function Node - Prepare Update
```javascript
// Node: Function - Mark Event Read
const eventId = msg.req.params.id;
const isRead = msg.payload.is_read !== undefined ? msg.payload.is_read : true;

if (!eventId) {
    msg.statusCode = 400;
    msg.payload = {
        success: false,
        error: "Event ID is required",
        code: "VALIDATION_ERROR"
    };
    return [null, msg];
}

msg.url = 'https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events?id=eq.' + eventId;
msg.method = 'PATCH';
msg.payload = {
    is_read: isRead,
    updated_at: new Date().toISOString()
};
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

msg.eventId = eventId;
msg.isRead = isRead;

return [msg, null];
```

#### Bước 3: HTTP Request
```
Node: HTTP Request
Name: Update Event
Method: (set by msg.method)
URL: (set by msg.url)
```

#### Bước 4: Function Node - Format Response
```javascript
// Node: Function - Format Update Response
let data = msg.payload;
if (typeof data === 'string') {
    try {
        data = JSON.parse(data);
    } catch (e) {
        data = [];
    }
}

if (Array.isArray(data) && data.length > 0) {
    msg.statusCode = 200;
    msg.payload = {
        success: true,
        message: "Event marked as read",
        data: {
            id: parseInt(msg.eventId),
            is_read: msg.isRead,
            updated_at: data[0].updated_at
        }
    };
} else {
    msg.statusCode = 404;
    msg.payload = {
        success: false,
        error: "Event not found",
        code: "NOT_FOUND"
    };
}

return msg;
```

---

## 📝 Quick Reference: Supabase Headers

**Copy-paste này cho mọi Supabase HTTP Request:**

```javascript
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};
```

---

## 🧪 Testing Commands

### Test All Command APIs:

**Windows PowerShell:**
```powershell
# Snapshot
Invoke-RestMethod -Uri "http://localhost:1880/api/commands/snapshot" -Method POST

# Speak
Invoke-RestMethod -Uri "http://localhost:1880/api/commands/speak" `
  -Method POST `
  -ContentType "application/json" `
  -Body '{"audio_url":"https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/test.wav","volume":80}'

# Siren On
Invoke-RestMethod -Uri "http://localhost:1880/api/commands/siren" `
  -Method POST `
  -ContentType "application/json" `
  -Body '{"action":"on","duration":5}'

# Siren Off
Invoke-RestMethod -Uri "http://localhost:1880/api/commands/siren" `
  -Method POST `
  -ContentType "application/json" `
  -Body '{"action":"off"}'
```

**Linux/Mac (curl):**
```bash
# Snapshot
curl -X POST http://localhost:1880/api/commands/snapshot

# Speak
curl -X POST http://localhost:1880/api/commands/speak \
  -H "Content-Type: application/json" \
  -d '{"audio_url":"https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/test.wav","volume":80}'

# Siren On
curl -X POST http://localhost:1880/api/commands/siren \
  -H "Content-Type: application/json" \
  -d '{"action":"on","duration":5}'

# Siren Off
curl -X POST http://localhost:1880/api/commands/siren \
  -H "Content-Type: application/json" \
  -d '{"action":"off"}'
```

---

### Test Settings API:

**Windows PowerShell:**
```powershell
# Get Settings
Invoke-RestMethod -Uri "http://localhost:1880/api/settings"

# Update Settings
Invoke-RestMethod -Uri "http://localhost:1880/api/settings" `
  -Method PATCH `
  -ContentType "application/json" `
  -Body '{"speaker_volume":75,"alarm_enabled":true,"pir_enabled":true,"notifications_enabled":true}'
```

**Linux/Mac (curl):**
```bash
# Get Settings
curl http://localhost:1880/api/settings

# Update Settings
curl -X PATCH http://localhost:1880/api/settings \
  -H "Content-Type: application/json" \
  -d '{"speaker_volume":75,"alarm_enabled":true,"pir_enabled":true,"notifications_enabled":true}'
```

---

### Test Events API:

**Windows PowerShell:**
```powershell
# Get all events
Invoke-RestMethod -Uri "http://localhost:1880/api/events?limit=10"

# Get unread events
Invoke-RestMethod -Uri "http://localhost:1880/api/events?unread=true"

# Get specific event
Invoke-RestMethod -Uri "http://localhost:1880/api/events/123"

# Mark as read
Invoke-RestMethod -Uri "http://localhost:1880/api/events/123/read" `
  -Method PATCH `
  -ContentType "application/json" `
  -Body '{"is_read":true}'
```

**Linux/Mac (curl):**
```bash
# Get all events
curl "http://localhost:1880/api/events?limit=10"

# Get unread events
curl "http://localhost:1880/api/events?unread=true"

# Get specific event
curl "http://localhost:1880/api/events/123"

# Mark as read
curl -X PATCH "http://localhost:1880/api/events/123/read" \
  -H "Content-Type: application/json" \
  -d '{"is_read":true}'
```

---

## ✅ Final Checklist

```
Phase 1 - Commands (Critical):
[ ] Flow 5: POST /api/commands/snapshot + MQTT publish
[ ] Flow 6: MQTT In doorbell/evt/snapshot
[ ] Flow 7: POST /api/commands/speak + MQTT publish
[ ] Flow 8: POST /api/commands/siren + MQTT publish + DB update

Phase 2 - Settings:
[ ] Flow 9: GET /api/settings
[ ] Flow 10: PATCH /api/settings + MQTT sync

Phase 3 - Events Enhancement:
[ ] Flow 11: Enhance GET /api/events (offset, unread)
[ ] Flow 12: GET /api/events/:id
[ ] Flow 13: PATCH /api/events/:id/read

Testing:
[ ] All cURL tests pass
[ ] MQTT messages published correctly
[ ] ESP32 receives commands
[ ] Database updates work
[ ] Error handling works
```

---

**🎉 Hoàn thành Phase 1-3 là đủ cho MVP production-ready!**

**Next:** Phase 4 (Voice Notes API, Sensor Stats) - Optional enhancements
