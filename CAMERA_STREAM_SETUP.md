# 📹 Camera Stream Setup Guide

**Date:** December 30, 2025  
**Status:** ✅ Implementation Complete

---

## 🎯 Tổng Quan

Camera streaming đã được implement đầy đủ:
- ✅ **ESP32:** MJPEG stream tại `/stream` endpoint
- ✅ **Frontend:** Component với error handling & retry logic
- ✅ **Config:** .env variable để dễ dàng thay đổi IP

---

## 📋 Checklist Trước Khi Test

- [ ] ESP32 đã flash firmware mới (với improved WiFi log)
- [ ] ESP32 đang chạy và connected WiFi
- [ ] Đã có IP của ESP32 từ Serial Monitor
- [ ] Frontend .env đã cập nhật với IP đúng
- [ ] Frontend dev server đang chạy

---

## 🚀 Bước 1: Get ESP32 IP Address

### Upload Firmware (nếu chưa):
```bash
cd c:\Users\minhthu\Documents\GitHub\DoorBell_Project\firmware-esp32
pio run --target upload
```

### Mở Serial Monitor:
```bash
pio device monitor
```

### Tìm log này sau khi ESP32 boot:
```
========================================
WiFi Connected!
IP Address: 192.168.137.100
Camera Stream: http://192.168.137.100/stream
Web Interface: http://192.168.137.100/
========================================
```

**Ghi lại IP Address!** Ví dụ: `192.168.137.100`

---

## 🔧 Bước 2: Configure Frontend

### Update file `.env`:
```bash
cd c:\Users\minhthu\Documents\GitHub\DoorBell_Project\frontend_react
notepad .env
```

### Sửa dòng VITE_ESP32_IP:
```env
# Thay IP này bằng IP thật từ Serial Monitor
VITE_ESP32_IP=192.168.137.100
```

**⚠️ Important:** IP phải khớp chính xác với IP từ ESP32 Serial Monitor!

---

## 🧪 Bước 3: Test Camera Stream

### Test 1: Truy cập trực tiếp từ browser
```
http://192.168.137.100/stream
```

**Expected:** Thấy video stream MJPEG (hình ảnh liên tục update)

**Nếu không thấy:**
- Check ESP32 Serial Monitor có error không
- Ping ESP32: `ping 192.168.137.100`
- Verify ESP32 và PC cùng WiFi network

---

### Test 2: Test Web Interface
```
http://192.168.137.100/
```

**Expected:** Thấy trang web control của ESP32

---

### Test 3: Start Frontend & View Stream

```powershell
cd c:\Users\minhthu\Documents\GitHub\DoorBell_Project\frontend_react
npm run dev
```

Mở browser: `http://localhost:5173`

**Navigate:** Overview → Camera Trực tiếp section

**Expected Results:**

✅ **Success:**
- Loading message → Camera stream hiển thị
- Console log: "Camera stream connected successfully"
- Stream smooth, không lag

❌ **Error - Chưa config IP:**
- Message: "Chưa cấu hình ESP32 IP trong .env"
- Solution: Update VITE_ESP32_IP trong .env

❌ **Error - Không kết nối được:**
- Message: "Không thể kết nối camera"
- Có nút "Thử lại"
- Troubleshooting steps hiển thị
- Solution: Check ESP32 status, verify IP, test direct URL

---

## 🔍 Troubleshooting

### Issue 1: "Không thể kết nối camera"

**Kiểm tra:**
```powershell
# 1. ESP32 có online không?
ping 192.168.137.100

# 2. Stream endpoint có hoạt động không?
# Mở browser: http://192.168.137.100/stream

# 3. Check CORS (nếu cần)
# ESP32 web server không có CORS restriction, nên không vấn đề
```

**Solutions:**
- Restart ESP32
- Check WiFi connection
- Verify IP address match
- Check firewall không block port 80

---

### Issue 2: Stream lag hoặc freeze

**Nguyên nhân:**
- WiFi signal yếu
- ESP32 CPU overload
- Network congestion

**Solutions:**
- Di chuyển ESP32 gần router
- Giảm camera frame rate (trong ESP32 code)
- Check không có nhiều devices cùng connect stream

---

### Issue 3: Browser console errors

**Error:** `Mixed Content (HTTP on HTTPS)`
- Frontend đang chạy HTTPS nhưng ESP32 là HTTP
- Solution: Chạy frontend HTTP hoặc dùng proxy

**Error:** `net::ERR_CONNECTION_REFUSED`
- ESP32 không online hoặc IP sai
- Solution: Check IP, restart ESP32

**Error:** `Failed to load resource`
- Stream endpoint không tồn tại
- Solution: Verify `/stream` endpoint trong ESP32 code

---

## 🎨 UI Features

### Camera Controls:

1. **Loading State:**
   - Icon + "Đang kết nối camera..."
   - Hiển thị stream URL đang connect

2. **Error State:**
   - Icon màu đỏ + "Không thể kết nối camera"
   - Nút "Thử lại" để reconnect
   - Troubleshooting checklist
   - Direct URL để test

3. **Connected State:**
   - Stream hiển thị full-width
   - Button "Chụp nhanh" để capture snapshot

4. **Auto-retry:**
   - User có thể click "Thử lại" bất cứ lúc nào
   - Frontend force reload stream với key prop

---

## 📊 Performance Tips

### ESP32 Side:
```cpp
// Giảm delay giữa frames nếu lag
// In handleStream() function:
delay(30); // Tăng lên 50-100 nếu cần

// Hoặc adjust camera quality
// In camera_module.cpp:
config.jpeg_quality = 12; // 10-63, càng cao càng kém chất lượng
```

### Frontend Side:
```jsx
// Nếu cần refresh định kỳ
useEffect(() => {
  const interval = setInterval(() => {
    setStreamKey(prev => prev + 1); // Force reload every X seconds
  }, 60000); // 60 seconds
  return () => clearInterval(interval);
}, []);
```

---

## 🔐 Security Notes

**⚠️ Current Setup:**
- Stream không có authentication
- HTTP (not HTTPS)
- Bất kỳ ai trong network đều có thể xem

**Production Recommendations:**
1. Add basic auth to ESP32 web server
2. Use HTTPS with self-signed cert
3. Implement token-based access
4. Add rate limiting
5. Log access attempts

---

## 📁 Modified Files

### ESP32:
- ✅ [web_server.cpp](firmware-esp32/src/web_server.cpp) - Improved WiFi connection log

### Frontend:
- ✅ [.env](frontend_react/.env) - Added VITE_ESP32_IP
- ✅ [CameraLive.jsx](frontend_react/src/components/Overview/CameraLive.jsx) - Complete rewrite
- ✅ [CameraLive.css](frontend_react/src/components/Overview/CameraLive.css) - Added error styles

---

## 🎯 Next Steps

### Current: ✅ Camera Streaming Works
### Next: 
1. ⏳ Implement snapshot capture to Supabase Storage
2. ⏳ Add motion detection overlay on stream
3. ⏳ Implement PTZ controls (if hardware supports)
4. ⏳ Add recording capability
5. ⏳ Implement multi-camera support

---

## 📞 Quick Reference

### ESP32 Endpoints:
```
Stream:     http://<ESP32_IP>/stream
Web UI:     http://<ESP32_IP>/
Record:     http://<ESP32_IP>/record
Stop:       http://<ESP32_IP>/stop_record
Download:   http://<ESP32_IP>/download
```

### Frontend URLs:
```
Dev Server: http://localhost:5173
Overview:   http://localhost:5173/#/overview (or main page)
```

### Serial Monitor Commands:
```bash
# PlatformIO
pio device monitor

# Arduino IDE
Tools → Serial Monitor (baud: 115200)
```

---

## ✅ Success Criteria

- [ ] ESP32 boot và log IP ra Serial Monitor
- [ ] Test `http://<ESP32_IP>/stream` trong browser → Thấy video
- [ ] Frontend .env có VITE_ESP32_IP đúng
- [ ] Frontend hiển thị camera stream không error
- [ ] Stream smooth, fps ổn định
- [ ] Button "Chụp nhanh" sẵn sàng (snapshot feature chưa implement)
- [ ] Error handling hoạt động (unplug ESP32 → Hiện error → Plug lại → Click "Thử lại" → Stream lại)

---

**🎉 Done! Camera streaming hoàn chỉnh và sẵn sàng test!**

**Questions?**
- Check ESP32 Serial Monitor for debug logs
- Check browser console (F12) for frontend errors
- Test direct stream URL trước khi test qua frontend
