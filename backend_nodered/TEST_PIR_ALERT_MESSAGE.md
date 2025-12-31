# Test Message cho Topic: doorbell/evt/pir_alert

## HiveMQ Cloud Connection Info
- **Broker**: `1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud`
- **Port**: `8883` (TLS) hoặc `8884` (WSS)
- **Username**: `esp_doorbell`
- **Password**: `Hcmus123`

---

## Test Message 1: PIR Alert với 3 ảnh burst

**Topic**: `doorbell/evt/pir_alert`

**Payload (JSON)**:
```json
{
  "image_urls": [
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735660800_pir_1.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735660800_pir_2.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735660800_pir_3.jpg"
  ],
  "count": 3,
  "timestamp": 1735660800,
  "level": "high",
  "description": "High alert burst images"
}
```

**Kết quả mong đợi trong database (table events)**:
- 3 records được tạo
- Mỗi record có:
  - `event_type`: "pir_motion"
  - `image_url`: URL tương ứng
  - `description`: "High alert burst images"
  - `metadata.level`: "high"
  - `metadata.burst_sequence`: 1, 2, 3
  - `metadata.total_burst`: 3
  - `created_at`: "2024-12-31T10:00:00.000Z"

---

## Test Message 2: PIR Alert với 2 ảnh (trường hợp 1 ảnh fail)

**Topic**: `doorbell/evt/pir_alert`

**Payload (JSON)**:
```json
{
  "image_urls": [
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735661000_pir_1.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735661000_pir_2.jpg"
  ],
  "count": 2,
  "timestamp": 1735661000,
  "level": "high",
  "description": "Partial burst capture (1 image failed)"
}
```

**Kết quả mong đợi**: 2 records được tạo với metadata.total_burst = 2

---

## Test Message 3: PIR Alert với custom description

**Topic**: `doorbell/evt/pir_alert`

**Payload (JSON)**:
```json
{
  "image_urls": [
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735661200_pir_1.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735661200_pir_2.jpg",
    "https://xznnnklhqkccylxxzsdh.supabase.co/storage/v1/object/public/bell-images/1735661200_pir_3.jpg"
  ],
  "count": 3,
  "timestamp": 1735661200,
  "level": "critical",
  "description": "Suspicious movement detected near entrance"
}
```

**Kết quả mong đợi**: 
- 3 records với description: "Suspicious movement detected near entrance"
- metadata.level: "critical"

---

## Cách test trên HiveMQ Cloud Web Client

1. Truy cập: https://console.hivemq.cloud/
2. Login vào cluster của bạn
3. Vào tab **Web Client**
4. Connect với:
   - Username: `esp_doorbell`
   - Password: `Hcmus123`
5. Publish message:
   - Topic: `doorbell/evt/pir_alert`
   - QoS: 1
   - Payload: Copy một trong các JSON test ở trên

---

## Verify trong database

Sau khi publish message, kiểm tra trong Supabase:

```sql
-- Xem các records vừa được tạo
SELECT 
  id,
  event_type,
  image_url,
  description,
  metadata->>'level' as level,
  metadata->>'burst_sequence' as burst_seq,
  metadata->>'total_burst' as total,
  created_at
FROM events
WHERE event_type = 'pir_motion'
ORDER BY created_at DESC
LIMIT 10;
```

---

## Troubleshooting

### Nếu không thấy data trong DB:
1. Kiểm tra Node-RED đang chạy: `node-red --userDir C:\Users\ADMIN\Documents\GitHub\DoorBell_Project\backend_nodered`
2. Kiểm tra Node-RED Debug log xem có nhận được message không
3. Kiểm tra MQTT broker connection trong Node-RED
4. Verify Supabase API key còn valid không

### Nếu thiếu level hoặc description:
- Function "Split Burst Images" sẽ dùng default: `level: "high"`, `description: "High alert burst images"`
- Đảm bảo Node-RED đã reload flow mới sau khi cập nhật flows.json
