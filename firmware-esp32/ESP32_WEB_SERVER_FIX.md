# 🔧 ESP32 Web Server Fix - Quick Guide

**Issue:** ESP32 ping OK nhưng web server không chạy  
**Root Cause:** `doorbellSetup()` không khởi tạo WiFi & Web Server  
**Status:** ✅ Fixed

---

## 📝 Changes Made

### File: `doorbell_app.cpp`

**Added:**
1. ✅ `#include "web_server.h"` - Import web server functions
2. ✅ `initWiFi()` call in setup - Connect to WiFi
3. ✅ `setupWebServer()` call in setup - Start HTTP & WebSocket servers
4. ✅ `webSocket.loop()` in loop - Handle WebSocket connections

---

## 🚀 Flash Updated Firmware

### Step 1: Build & Upload
```powershell
cd c:\Users\minhthu\Documents\GitHub\DoorBell_Project\firmware-esp32
pio run --target upload
```

### Step 2: Monitor Serial Output
```powershell
pio device monitor
```

### Step 3: Wait for WiFi Connection Log
Look for this output:
```
========================================
WiFi Connected!
IP Address: 192.168.137.248
Camera Stream: http://192.168.137.248/stream
Web Interface: http://192.168.137.248/
========================================
[WebServer] ✓ HTTP server running on port 80
[WebServer] ✓ WebSocket server running on port 81
```

---

## ✅ Verify Fix

### Test 1: Ping ESP32
```powershell
ping 192.168.137.248
```
**Expected:** Reply from 192.168.137.248 ✅

### Test 2: Access Web Interface
Open browser: `http://192.168.137.248/`

**Expected:** HTML page hiển thị ✅

### Test 3: Access Camera Stream
Open browser: `http://192.168.137.248/stream`

**Expected:** MJPEG video stream ✅

### Test 4: Test from Frontend
```powershell
cd c:\Users\minhthu\Documents\GitHub\DoorBell_Project\frontend_react
npm run dev
# Open http://localhost:5173
```

**Expected:** Camera stream hiển thị trong Overview page ✅

---

## 🐛 Troubleshooting

### Issue: Still "Connection refused" after flash

**Check Serial Monitor:**
```
[ERR] ❌ WiFi connection failed!
```

**Solutions:**
1. Check WiFi credentials in `config.h`:
   ```cpp
   #define WIFI_SSID "Dinh"
   #define WIFI_PASS "dicuongne"
   ```
2. Verify WiFi network is active
3. Check router allows device connections
4. Try ESP32 reset button

---

### Issue: WiFi connected but stream không hoạt động

**Check Serial Monitor for:**
```
[ERR] Camera init failed
```

**Solutions:**
1. Camera module connection issue
2. Try power cycle ESP32
3. Check camera pins in `config.h`

---

### Issue: Stream lag hoặc freeze

**Adjust in `web_server.cpp`:**
```cpp
void handleStream() {
    // ...
    delay(30); // Increase to 50-100 if lag
    // ...
}
```

---

## 📊 Expected Serial Monitor Output

```
=== ESP32-S3 Smart Doorbell ===
Features: Ding-Dong, Voice Notes, Security Cam, PIR Alert

[INIT] Initializing SPIFFS...
[SPIFFS] ✓ Initialized
[SPIFFS] Total: 1024000 bytes | Used: 256000 bytes | Free: 768000 bytes

[Sensors] Pins initialized
[Camera] ✓ Initialized
[Microphone] ✓ Initialized
[Speaker] ✓ Initialized

[INIT] Starting WiFi...
Connecting WiFi...
========================================
WiFi Connected!
IP Address: 192.168.137.248
Camera Stream: http://192.168.137.248/stream
Web Interface: http://192.168.137.248/
========================================
[WiFi] ✓ Connected successfully

[INIT] Starting Web Server...
Web server started
WebSocket server started
[WebServer] ✓ HTTP server running on port 80
[WebServer] ✓ WebSocket server running on port 81

[MQTT] Connecting to broker...
[MQTT] ✓ Connected

╔════════════════════════════════════════╗
║   DOORBELL READY                       ║
║   All services initialized             ║
╚════════════════════════════════════════╝

[LOOP] Iteration: 5000 | Free heap: 180000 bytes | Uptime: 25 s
```

---

## 🎯 Success Checklist

- [ ] Flash completed successfully
- [ ] Serial Monitor shows WiFi connected
- [ ] IP Address displayed in log
- [ ] "Web server started" message appears
- [ ] Can ping ESP32 IP
- [ ] Can access http://IP/ in browser
- [ ] Can access http://IP/stream in browser
- [ ] Frontend camera component shows stream
- [ ] No lag or freeze in stream

---

## ⏱️ Estimated Time

- Build & Flash: **2-3 minutes**
- First boot & WiFi connect: **10-30 seconds**
- Verify all endpoints: **1 minute**
- **Total: ~5 minutes**

---

## 📞 Quick Commands Reference

```powershell
# Build only (check for errors)
pio run

# Build & upload
pio run --target upload

# Monitor serial
pio device monitor

# Monitor with baud rate
pio device monitor --baud 115200

# Clean build
pio run --target clean

# Full rebuild
pio run --target clean; pio run --target upload
```

---

**✅ After successful flash, ESP32 will:**
1. Connect to WiFi automatically
2. Print IP and stream URLs
3. Start HTTP server on port 80
4. Start WebSocket server on port 81
5. Accept camera stream requests
6. Respond to MQTT commands

**🎉 Ready to test camera streaming!**
