# 🚀 Deployment Instructions

## Sau khi cập nhật flows.json, làm theo các bước sau:

### Option 1: Restart Node-RED (Recommended)

#### Nếu chạy trong terminal:
1. Press `Ctrl+C` để stop Node-RED
2. Chạy lại: `node-red` hoặc `npm start`

#### Nếu chạy như Windows Service:
```powershell
Restart-Service node-red
```

---

### Option 2: Import flows qua Node-RED UI

1. Mở trình duyệt: `http://localhost:1880`
2. Click menu (≡) ở góc trên bên phải
3. Chọn **Import** → **Clipboard**
4. Copy toàn bộ nội dung file `flows.json`
5. Paste vào textbox
6. Click **Import**
7. Click nút **Deploy** (màu đỏ ở góc trên bên phải)

---

### Option 3: Reload Flows từ file

1. Mở Node-RED UI: `http://localhost:1880`
2. Click menu (≡) → **Settings** → **View all**
3. Tìm section **Projects**
4. Click **Reload flows from disk**
5. Click nút **Deploy**

---

## Verify Deployment Thành Công

### Test GET /api/quick-responses:
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
      "title": "busy",
      "audio_url": "...",
      "is_active": true
    },
    ...
  ]
}
```

### Nếu vẫn lỗi "Cannot GET /api/quick-responses":
1. Check Node-RED logs có error không
2. Verify flows.json syntax đúng (valid JSON)
3. Restart Node-RED một lần nữa

---

## Frontend Testing

1. Navigate to frontend directory:
```powershell
cd c:\Users\minhthu\Documents\GitHub\DoorBell_Project\frontend_react
```

2. Start frontend dev server:
```powershell
npm run dev
```

3. Open browser: `http://localhost:5173` (hoặc port Vite đang dùng)

4. Navigate to **Overview** page → **Audio Message Control**

5. Test workflow:
   - Click vào một quick response (ví dụ "Vui lòng đợi")
   - Nếu chưa có audio → Nút "Ghi âm mới" xuất hiện
   - Click "Ghi âm mới" → Record audio → Click "Lưu ghi âm này"
   - Sau khi upload → Nút "Phát tin nhắn này qua loa" được enable
   - Click "Phát tin nhắn..." → Check MQTT message published

---

## Debugging Tips

### Check Node-RED logs:
```powershell
# If running in terminal
# Logs appear in the terminal window

# If running as service
Get-EventLog -LogName Application -Source node-red -Newest 20
```

### Check browser console (F12):
- Look for API fetch errors
- Check network tab for 404/500 errors

### Check MQTT messages:
- Use **MQTT Explorer** to subscribe to `doorbell/cmd/speak`
- Verify message payload contains correct audio_url

---

## Common Issues

### ❌ "Cannot GET /api/quick-responses"
**Solution:** Node-RED chưa load flows mới. Restart Node-RED.

### ❌ "Network Error" in frontend
**Solution:** Check Node-RED đang chạy tại `http://localhost:1880` và CORS enabled.

### ❌ "Không thể tải danh sách tin nhắn"
**Solution:** Check Supabase API key và database có records trong `quick_responses` table.

### ❌ Audio upload failed
**Solution:** Check Supabase Storage bucket `bell-audio` exists và có public access.

---

## Success Indicators ✅

- [ ] GET /api/quick-responses returns 200 với data
- [ ] Frontend hiển thị 4 quick responses
- [ ] Click vào response → Form ghi âm xuất hiện
- [ ] Ghi âm → Upload thành công → Database updated
- [ ] Click "Phát tin nhắn" → MQTT message published
- [ ] ESP32 nhận MQTT và download + play audio

---

## Next Steps

Sau khi verify các APIs hoạt động:

1. ✅ Test with real audio files từ microphone
2. ✅ Test ESP32 nhận MQTT commands
3. ✅ Test audio playback trên ESP32 speaker
4. 🔄 Implement error handling cho network failures
5. 🔄 Add loading states trong UI
6. 🔄 Add success/error toast notifications

---

**📝 Note:** Luôn restart Node-RED sau khi edit flows.json manually!
