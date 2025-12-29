# 🔄 Complete Workflow Summary

**Project:** DoorBell System  
**Last Updated:** December 30, 2025

---

## 📋 Architecture Overview

```
┌──────────────┐
│   Frontend   │ React App (Port 5173)
│   (React)    │
└──────┬───────┘
       │
       ├─────────────────┐
       │                 │
       ▼                 ▼
┌──────────────┐  ┌──────────────┐
│   Supabase   │  │  Node-RED    │
│   Storage    │  │   Gateway    │
│              │  │ (Port 1880)  │
└──────────────┘  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │  MQTT Broker │
                  │   (HiveMQ)   │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────┐
                  │    ESP32     │
                  │   Doorbell   │
                  └──────────────┘
```

---

## ✅ WORKFLOW 1: Send Audio Message (Frontend → ESP32)

### **Frontend Implementation (COMPLETED ✅)**

**File:** `frontend_react/src/pages/SecurityDashboard/index.jsx`

```javascript
const handleSendAudioMessage = async (messageType, content) => {
  try {
    // Step 1: Upload audio blob to Supabase Storage
    const timestamp = Date.now();
    const fileName = `message_${timestamp}.wav`;
    
    const { data, error } = await supabase.storage
      .from('bell-audio')
      .upload(fileName, content.blob, {
        contentType: 'audio/wav',
        cacheControl: '3600',
        upsert: false
      });

    if (error) throw error;

    // Step 2: Get public URL
    const { data: { publicUrl } } = supabase.storage
      .from('bell-audio')
      .getPublicUrl(fileName);

    // Step 3: Send URL to Node-RED API
    const response = await axios.post('http://localhost:1880/api/commands/speak', {
      audio_url: publicUrl,
      volume: volume,
      message_type: 'custom'
    });

    if (response.data.success) {
      alert(`✅ Đã gửi tin nhắn audio: "${content.message}"`);
    }
  } catch (error) {
    console.error('Error:', error);
    alert('❌ Lỗi: ' + error.message);
  }
};
```

**Components:**
- ✅ `AudioMessageControl.jsx` - Record audio với MediaRecorder API
- ✅ `OverviewTab.jsx` - Pass handler down
- ✅ `index.jsx` - Implement upload workflow

---

### **Node-RED Implementation (TODO ⏳)**

**Flow 7: POST /api/commands/speak**

```
Frontend           Node-RED                ESP32
   │                  │                      │
   │ 1. Upload audio  │                      │
   │─────────────────►│                      │
   │    (Supabase)    │                      │
   │                  │                      │
   │ 2. Get URL       │                      │
   │◄─────────────────│                      │
   │                  │                      │
   │ 3. POST /api/commands/speak              │
   │    { audio_url: "https://..." }         │
   │─────────────────►│                      │
   │                  │                      │
   │                  │ 4. MQTT Publish      │
   │                  │   doorbell/cmd/speak │
   │                  │─────────────────────►│
   │                  │   { audio_url, volume } │
   │                  │                      │
   │ 5. HTTP 200      │                      │
   │◄─────────────────│                      │
   │                  │                      │
   │                  │              6. Download & Play
   │                  │                      │
```

**Required Nodes:**
1. ✅ HTTP In: `POST /api/commands/speak`
2. ✅ Function: Validate input (audio_url required)
3. ✅ MQTT Out: Publish to `doorbell/cmd/speak`
4. ✅ Function: Format HTTP response
5. ✅ HTTP Response

**Key Points:**
- ⚠️ Node-RED KHÔNG nhận file upload
- ⚠️ Node-RED chỉ nhận URL đã upload
- ⚠️ MQTT Out không có output port (terminal node)
- ✅ Cần configure 2 outputs trong Validate function

---

## ✅ WORKFLOW 2: Take Snapshot (Frontend → ESP32 → Frontend)

### **Frontend → ESP32 (Command)**

```
Frontend           Node-RED                ESP32
   │                  │                      │
   │ POST /api/commands/snapshot             │
   │─────────────────►│                      │
   │                  │                      │
   │                  │ MQTT Publish         │
   │                  │ doorbell/cmd/snapshot │
   │                  │─────────────────────►│
   │                  │ { command_id, timestamp } │
   │ HTTP 200         │                      │
   │◄─────────────────│                      │
   │                  │              Capture image
   │                  │              Upload to Storage
   │                  │              Get public URL
```

### **ESP32 → Frontend (Response)**

```
ESP32              Node-RED            Supabase DB
   │                  │                      │
   │ MQTT Publish     │                      │
   │ doorbell/evt/snapshot                   │
   │─────────────────►│                      │
   │ { command_id, image_url, timestamp }    │
   │                  │                      │
   │                  │ Insert to events     │
   │                  │─────────────────────►│
   │                  │                      │
   │                  │                      │
   
Frontend polls:
GET /api/events → Nhận image_url mới
```

**Required Flows:**
- ✅ Flow 5: POST /api/commands/snapshot → MQTT publish
- ⏳ Flow 6: MQTT In doorbell/evt/snapshot → Save to DB

---

## ✅ WORKFLOW 3: Toggle Alarm (Frontend ↔ ESP32)

```
Frontend           Node-RED            ESP32
   │                  │                  │
   │ POST /api/commands/siren             │
   │    { action: "on", duration: 5 }    │
   │─────────────────►│                  │
   │                  │                  │
   │                  │ Update DB        │
   │                  │ (alarm_enabled)  │
   │                  │                  │
   │                  │ MQTT Publish     │
   │                  │ doorbell/cmd/siren │
   │                  │─────────────────►│
   │                  │ { action, duration } │
   │ HTTP 200         │                  │
   │◄─────────────────│                  │
   │                  │          Play siren
```

**Side Effects:**
- Updates `device_settings.alarm_enabled` in database
- Publishes MQTT command to ESP32

---

## ✅ WORKFLOW 4: Update Settings (Frontend → ESP32)

```
Frontend           Node-RED            ESP32
   │                  │                  │
   │ PATCH /api/settings                 │
   │ { speaker_volume: 75, ... }         │
   │─────────────────►│                  │
   │                  │                  │
   │                  │ Update DB        │
   │                  │ (device_settings)│
   │                  │                  │
   │                  │ MQTT Publish     │
   │                  │ doorbell/cmd/settings │
   │                  │─────────────────►│
   │                  │ { speaker_volume, ... } │
   │ HTTP 200         │                  │
   │◄─────────────────│                  │
   │ { success, data }│          Sync settings
```

**Critical:** Settings MUST be synced via MQTT after DB update!

---

## 📝 Node-RED Connection Patterns

### Pattern 1: Simple API (No MQTT)
```
[HTTP In] → [Query DB] → [Format Response] → [HTTP Response]
```

**Example:** GET /api/events, GET /api/settings

---

### Pattern 2: Command with MQTT (Terminal)
```
[HTTP In] → [Validate] → [Generate Command] ──┬→ [MQTT Out]
                                              │
                                              └→ [Format Response] → [HTTP Response]
```

**Key:** MQTT Out is terminal, need 2 wires from previous node

**Example:** POST /api/commands/snapshot

---

### Pattern 3: Command with Validation (2 Outputs)
```
[HTTP In] → [Validate] ──(output 1)──┬→ [MQTT Out]
            │            │            │
            │            │            └→ [Format Success] → [HTTP Response]
            │            │
            │            └──(output 2)──→ [HTTP Response Error]
```

**Key:** Function node has 2 outputs configured

**Example:** POST /api/commands/speak, POST /api/commands/siren

---

### Pattern 4: Update + MQTT Sync
```
[HTTP In] → [Validate] → [Update DB] → [Prepare MQTT] ──┬→ [MQTT Out]
                                                         │
                                                         └→ [Format] → [HTTP Response]
```

**Key:** DB update first, then MQTT sync

**Example:** PATCH /api/settings

---

## 🧪 Testing Checklist

### Frontend Tests
```bash
# 1. Test Audio Recording
- Open dashboard
- Press and hold message button
- Release to stop recording
- Click "Phát file ghi âm này"
- Check browser console for upload URL
- Check Node-RED debug

# 2. Test Snapshot
- Click "Chụp ảnh" button
- Wait 2-3 seconds
- Check events tab for new image
```

### Node-RED Tests
```bash
# 1. Test speak command
curl -X POST http://localhost:1880/api/commands/speak \
  -H "Content-Type: application/json" \
  -d '{
    "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/test.wav",
    "volume": 80
  }'

# Expected: 
# - HTTP 200 with command_id
# - MQTT message published
# - Check MQTT Explorer

# 2. Test snapshot command
curl -X POST http://localhost:1880/api/commands/snapshot

# Expected:
# - HTTP 200 with command_id
# - MQTT message published

# 3. Test settings sync
curl -X PATCH http://localhost:1880/api/settings \
  -H "Content-Type: application/json" \
  -d '{"speaker_volume": 75}'

# Expected:
# - HTTP 200 with updated settings
# - Database updated
# - MQTT message published
```

### MQTT Tests
```bash
# Subscribe to all topics
mosquitto_sub -h 1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud \
  -p 8883 -t "doorbell/#" \
  -u esp_doorbell -P Hcmus123 \
  --cafile ca.crt

# Should see:
# doorbell/cmd/speak
# doorbell/cmd/snapshot
# doorbell/cmd/siren
# doorbell/cmd/settings
```

---

## ⚠️ Common Mistakes to Avoid

### 1. ❌ MQTT Out Connection
**Wrong:**
```
[Function] → [MQTT Out] → [Next Node]  // ❌ MQTT Out has no output!
```

**Correct:**
```
[Function] ──┬→ [MQTT Out]
            │
            └→ [Next Node]
```

---

### 2. ❌ File Upload in Node-RED
**Wrong:**
```
Frontend → (multipart/form-data) → Node-RED → MQTT  // ❌ Don't do this!
```

**Correct:**
```
Frontend → Supabase Storage → Get URL → Node-RED → MQTT
```

---

### 3. ❌ Missing MQTT Sync
**Wrong:**
```
PATCH /api/settings → Update DB → Return response  // ❌ ESP32 not synced!
```

**Correct:**
```
PATCH /api/settings → Update DB → Publish MQTT → Return response
```

---

### 4. ❌ Function Output Configuration
**Wrong:**
```javascript
// Function with 2 return statements but Outputs = 1
return [msg, null];  // ❌ Second output lost!
```

**Correct:**
```javascript
// Configure node: Properties → Outputs: 2
return [msg, null];  // ✅ Output 1 goes to first wire, output 2 to second wire
```

---

## 📊 Implementation Status

### Frontend
- ✅ Upload audio to Supabase Storage
- ✅ Get public URL
- ✅ Send URL to Node-RED API
- ✅ AudioMessageControl component
- ✅ Handler in SecurityDashboard

### Node-RED (TO DO)
- ⏳ Flow 5: POST /api/commands/snapshot
- ⏳ Flow 6: MQTT In doorbell/evt/snapshot
- ⏳ Flow 7: POST /api/commands/speak
- ⏳ Flow 8: POST /api/commands/siren
- ⏳ Flow 9: GET /api/settings
- ⏳ Flow 10: PATCH /api/settings

### ESP32 (Assumed Working)
- ✅ Subscribe doorbell/cmd/* topics
- ✅ Download audio from URL
- ✅ Play audio
- ✅ Capture snapshot
- ✅ Upload image to Storage
- ✅ Publish response events

---

## 🚀 Next Steps

1. **Implement Node-RED Flows (Priority Order)**
   - Flow 7: POST /api/commands/speak (Most critical)
   - Flow 5 + 6: Snapshot command + response
   - Flow 8: Siren control
   - Flow 10: Settings sync

2. **Test End-to-End**
   - Record audio in browser → Play on ESP32
   - Take snapshot → See image in dashboard
   - Toggle alarm → Hear siren
   - Change volume → ESP32 volume changes

3. **Add Error Handling**
   - Frontend: Show loading states
   - Node-RED: Catch all errors
   - ESP32: Handle network failures

---

**Last Updated:** December 30, 2025  
**Maintained By:** DoorBell Project Team
