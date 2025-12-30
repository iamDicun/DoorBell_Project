# Test Quick Responses API

## Prerequisites
1. Node-RED đang chạy tại `http://localhost:1880`
2. Database đã có 4 records trong `quick_responses` table với title: `wait`, `notHome`, `busy`, `package`

---

## Test 1: GET /api/quick-responses

### PowerShell:
```powershell
Invoke-RestMethod -Uri "http://localhost:1880/api/quick-responses" -Method GET
```

### Expected Response:
```json
{
  "success": true,
  "data": [
    {
      "id": 1,
      "created_at": "2025-12-29T20:47:16.496017+00:00",
      "title": "busy",
      "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_1767041230983.wav",
      "is_active": true
    },
    {
      "id": 2,
      "created_at": "2025-12-29T20:47:16.496017+00:00",
      "title": "package",
      "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_1767041230983.wav",
      "is_active": true
    },
    {
      "id": 3,
      "created_at": "2025-12-29T20:47:16.496017+00:00",
      "title": "wait",
      "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_1767041230983.wav",
      "is_active": true
    },
    {
      "id": 4,
      "created_at": "2025-12-29T20:47:16.496017+00:00",
      "title": "notHome",
      "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/message_1767041230983.wav",
      "is_active": true
    }
  ]
}
```

---

## Test 2: PATCH /api/quick-responses/:title

### Test với title = "wait"

#### PowerShell:
```powershell
$body = @{
    audio_url = "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735566789123.wav"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:1880/api/quick-responses/wait" `
  -Method PATCH `
  -ContentType "application/json" `
  -Body $body
```

### Expected Response:
```json
{
  "success": true,
  "message": "Quick response updated successfully",
  "data": {
    "id": 3,
    "created_at": "2025-12-30T04:26:29.123+00:00",
    "title": "wait",
    "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735566789123.wav",
    "is_active": true
  }
}
```

---

## Test 3: POST /api/commands/speak (Updated - No DB Insert)

### PowerShell:
```powershell
$body = @{
    audio_url = "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735566789123.wav"
    volume = 80
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:1880/api/commands/speak" `
  -Method POST `
  -ContentType "application/json" `
  -Body $body
```

### Expected Response:
```json
{
  "success": true,
  "message": "Audio playback command sent to ESP32",
  "command_id": "cmd_1735566800123_speak",
  "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735566789123.wav",
  "volume": 80,
  "timestamp": 1735566800
}
```

### Verify MQTT (using MQTT Explorer):
- **Topic:** `doorbell/cmd/speak`
- **Payload:**
```json
{
  "command_id": "cmd_1735566800123_speak",
  "audio_url": "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735566789123.wav",
  "volume": 80,
  "timestamp": 1735566800
}
```

---

## Test 4: Error Handling

### Test Invalid Title:
```powershell
$body = @{
    audio_url = "https://test.com/audio.wav"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:1880/api/quick-responses/invalid_title" `
  -Method PATCH `
  -ContentType "application/json" `
  -Body $body
```

### Expected Response:
```json
{
  "success": false,
  "error": "Invalid title",
  "code": "VALIDATION_ERROR",
  "details": {
    "field": "title",
    "message": "title must be one of: wait, notHome, busy, package"
  }
}
```

### Test Missing audio_url:
```powershell
$body = @{} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:1880/api/quick-responses/wait" `
  -Method PATCH `
  -ContentType "application/json" `
  -Body $body
```

### Expected Response:
```json
{
  "success": false,
  "error": "Validation failed",
  "code": "VALIDATION_ERROR",
  "details": {
    "field": "audio_url",
    "message": "audio_url is required and must be a string"
  }
}
```

---

## Complete Workflow Test

### Step 1: Check current quick responses
```powershell
Invoke-RestMethod -Uri "http://localhost:1880/api/quick-responses"
```

### Step 2: Frontend uploads new audio to Supabase Storage
*(This happens in frontend code - see AudioMessageControl.jsx)*

Example result: `https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735567000000.wav`

### Step 3: Update quick response with new audio URL
```powershell
$body = @{
    audio_url = "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735567000000.wav"
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:1880/api/quick-responses/wait" `
  -Method PATCH `
  -ContentType "application/json" `
  -Body $body
```

### Step 4: Send audio command to ESP32
```powershell
$body = @{
    audio_url = "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-audio/wait_1735567000000.wav"
    volume = 80
} | ConvertTo-Json

Invoke-RestMethod -Uri "http://localhost:1880/api/commands/speak" `
  -Method POST `
  -ContentType "application/json" `
  -Body $body
```

### Step 5: Verify in Supabase
Query `quick_responses` table:
```sql
SELECT * FROM quick_responses WHERE title = 'wait';
```

Should show updated `audio_url` and `created_at` timestamp.

---

## Expected Behavior Summary

✅ **GET /api/quick-responses:** Returns all active quick responses with audio URLs  
✅ **PATCH /api/quick-responses/:title:** Updates audio_url for specific title  
✅ **POST /api/commands/speak:** Publishes MQTT command WITHOUT inserting to DB  
✅ **Frontend:** Fetches quick responses, shows status, requires recording if empty, allows update & send  
✅ **Database:** Only quick_responses table is updated (no duplicate inserts)  
✅ **ESP32:** Receives MQTT command and downloads audio from URL

---

## Troubleshooting

### Issue: "Quick response not found" when PATCH
**Solution:** Check database has record with exact title match (case-sensitive)

### Issue: Frontend shows "Chưa ghi âm" even after update
**Solution:** Click refresh or reload page. Check audio_url is not empty string in database.

### Issue: ESP32 doesn't play audio
**Solution:** 
1. Check MQTT message was published (use MQTT Explorer)
2. Verify audio_url is publicly accessible
3. Check ESP32 logs for download errors

### Issue: Node-RED flows not working after edit
**Solution:** 
1. Click "Deploy" button in Node-RED
2. Check Node-RED debug panel for errors
3. Verify Supabase API key and URL are correct
