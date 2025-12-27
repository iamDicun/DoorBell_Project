# ESP32 Doorbell - Tài liệu API cho Backend (Tiếng Việt)

## Tổng quan

Tài liệu này mô tả các topic MQTT, định dạng payload JSON và các endpoint HTTP dùng bởi hệ thống chuông cửa ESP32 để tích hợp với backend Node-RED.

**Thông tin thiết bị:**

- Định dạng `device_id`: `ESP32_{MAC_ADDRESS}` (ví dụ: `ESP32_A1B2C3D4`)
- Phiên bản firmware: `1.0.0`
- Dạng timestamp: Số giây kể từ lúc thiết bị khởi động (unsigned long)

---

## Các topic MQTT chính

| Topic                | Hướng           | Mục đích                                     |
| -------------------- | --------------- | -------------------------------------------- |
| `doorbell/status`    | ESP32 → Backend | Sự kiện chuông, kết quả upload ảnh/âm thanh  |
| `doorbell/security`  | ESP32 → Backend | Cảnh báo PIR, ảnh an ninh, cảnh báo nhiệt độ |
| `doorbell/telemetry` | ESP32 → Backend | Dữ liệu cảm biến (nhiệt độ, motion tổng hợp) |
| `doorbell/heartbeat` | ESP32 → Backend | Thông tin trạng thái/khỏe thiết bị           |
| `doorbell/command`   | Backend → ESP32 | Các lệnh điều khiển từ xa                    |

---

## Luồng 1: Nhấn chuông (Doorbell Ring)

### Thông báo MQTT (`doorbell/status`)

```json
{
  "event": "press",
  "chime": "style1",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123456
}
```

**Các kiểu chuông (chime):**

- `style1` (mặc định)
- `style2`
- `style3`
- `style4`

### Upload ảnh khách (sau chụp)

**Endpoint:** `POST http://192.168.137.1:3000/upload-image`

**Headers:**

```
Content-Type: image/jpeg
X-Event-Type: doorbell_press
X-Timestamp: 123460
```

**Body:** Dữ liệu nhị phân JPEG (QVGA 320x240, ~15-20KB)

**Phản hồi mong đợi:** 200 OK

Sau khi upload thành công, thiết bị sẽ publish một payload ví dụ đến `doorbell/status`:

```json
{
  "event": "guest_photo",
  "status": "captured",
  "size": 15234,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 123460
}
```

---

## Luồng 2: Phát hiện chuyển động (PIR) & Cảnh báo an ninh

### Các mức cảnh báo PIR

- **NORMAL:** 1-2 detections trong 20 giây (motion cleared)
- **MEDIUM:** 3 detections trong 20 giây (người đứng lâu)
- **HIGH:** 4+ detections trong 20 giây (loitering / khả nghi)

**Quy tắc ghi nhận detections:**

- Mỗi detection phải cách nhau tối thiểu **3 giây** (debounce)
- Detections được ghi trong cửa sổ trượt **20 giây**
- Mỗi lần rising edge (LOW→HIGH) của PIR sẽ được đăng ký ngay (không phụ thuộc vào sampling)
- Mức cảnh báo được đánh giá mỗi **5 giây**

### Ví dụ payload ALERT_HIGH (`doorbell/security`)

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

**Hành động khi HIGH:**

- Chụp 3 ảnh burst (upload tới `/upload-image` với header `X-Event-Type: pir_burst`)
- Kích hoạt còi/âm báo

### Ví dụ payload ALERT_MEDIUM (`doorbell/security`)

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

**Hành động khi MEDIUM:**

- Chụp 1 ảnh khách và upload

### Ví dụ payload ALERT_NORMAL (`doorbell/security`)

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

---

## Luồng 3: Ghi âm voice note (Long press)

### Ghi và upload sau khi dừng ghi

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

### HTTP Upload audio

**Endpoint:** `POST http://192.168.137.1:3000/upload-audio`

**Headers:**

```
Content-Type: audio/wav
X-Event-Type: voice_note
X-Timestamp: 128456
```

**Body:** Dữ liệu WAV (16 kHz, 16-bit, mono)

---

## Luồng 4: Nhiệt độ môi trường

### Đọc nhiệt độ định kỳ (`doorbell/telemetry`)

```json
{
  "value": 28.5,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 130000
}
```

**Cảnh báo nhiệt độ cực trị (`doorbell/security`):**

```json
{
  "event": "extreme_temperature",
  "value": 45.0,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 130030
}
```

---

## Luồng 5: Điều khiển từ xa qua MQTT (`doorbell/command`)

### Các lệnh cơ bản

#### Kích hoạt còi (Alarm ON)

```json
{ "action": "ON" }
```

#### Tắt còi (Alarm OFF)

```json
{ "action": "OFF" }
```

#### Chụp ảnh từ xa

```json
{ "action": "capture" }
```

**Phản hồi thiết bị:** chụp ảnh, upload tới `/upload-image` với header `X-Event-Type: remote_capture`, sau đó publish kết quả lên `doorbell/status`.

#### Phát chuông qua lệnh MQTT (4 kiểu)

- Style 1 (mặc định): `{ "action": "play_chime_1" }`
- Style 2: `{ "action": "play_chime_2" }`
- Style 3: `{ "action": "play_chime_3" }`
- Style 4: `{ "action": "play_chime_4" }`

**Device Response example:**

```json
{
  "event": "press",
  "chime": "style2",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 135000
}
```

#### Phát file audio có sẵn

```json
{ "file": "wait" }
```

**Available audio files (trên SPIFFS):**

- `ding_dong.mp3` (style1)
- `ding_dong_2.mp3` (style2)
- `ding_dong_3.mp3` (style3)
- `ding_dong_4.mp3` (style4)
- `alarm.mp3`
- `please_wait.mp3`

#### Đặt âm lượng

```json
{ "action": "set_volume", "value": 0.7 }
```

---

## Heartbeat & Giám sát sức khỏe

### Heartbeat (`doorbell/heartbeat`)

Gửi định kỳ (mỗi 30s) để báo thiết bị đang online và các chỉ số:

```json
{
  "status": "online",
  "heap": 207000,
  "rssi": -24,
  "uptime": 120,
  "firmware_version": "1.0.0",
  "ip": "192.168.1.100",
  "reconnect_count": 0,
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 140000
}
```

**Gợi ý backend:**

- Xem thiết bị offline nếu không nhận heartbeat trong 3× khoảng thời gian (ví dụ 90s)
- Dùng `reconnect_count` để phát hiện kết nối không ổn định

---

## HTTP Uploads & Headers

**Ảnh:** `POST /upload-image` với header:

```
Content-Type: image/jpeg
X-Event-Type: doorbell_press | pir_burst | remote_capture
X-Timestamp: <timestamp>
```

**Audio:** `POST /upload-audio` với header:

```
Content-Type: audio/wav
X-Event-Type: voice_note
X-Timestamp: <timestamp>
```

Sử dụng `X-Event-Type` và `X-Timestamp` để liên kết các upload file với các sự kiện MQTT tương ứng.

---

## Xử lý lỗi

- Nếu upload HTTP thất bại, thiết bị thử lại tối đa 3 lần với backoff lũy tiến.
- Nếu MQTT disconnection, thiết bị tự reconnect với khoảng cách 5s giữa các lần thử.

---

## Bảng tham khảo nhanh: Các topic cần subscribe

- `doorbell/status` — sự kiện chuông, upload file thành công
- `doorbell/security` — cảnh báo PIR & an ninh
- `doorbell/telemetry` — cảm biến (nhiệt độ, motion)
- `doorbell/heartbeat` — trạng thái thiết bị
- `doorbell/command` — lệnh từ backend (ON/OFF, capture, play_chime_x)

---

## Checklist kiểm thử

- [ ] Nhấn chuông nhận MQTT event + upload ảnh trong 2s
- [ ] Tất cả 4 chime hoạt động qua lệnh MQTT
- [ ] PIR HIGH kích hoạt 3 ảnh burst + còi
- [ ] PIR MEDIUM kích 1 ảnh
- [ ] PIR debounce (3s) hoạt động
- [ ] Ghi âm voice note upload thành công
- [ ] Nhiệt độ gửi định kỳ
- [ ] Remote capture hoạt động
- [ ] Heartbeat mỗi 30s
- [ ] Headers `X-Event-Type` và `X-Timestamp` có trên tất cả uploads
- [ ] Tất cả payload có `device_id` và `timestamp`

---

## Ghi chú về bảo mật

- MQTT sử dụng TLS (broker HiveMQ Cloud). HTTP hiện chưa mã hóa — cân nhắc HTTPS cho môi trường production.
- Backend nên kiểm tra header `Content-Type`, kích thước file và kiểm tra header file (JPEG/WAV) để phòng chống upload độc hại.

---

## Liên hệ & Hỗ trợ

- Firmware: 1.0.0
- Cập nhật: December 2025
- Board: ESP32-S3, Camera OV2640, Mic INMP441, Speaker MAX98357

---
