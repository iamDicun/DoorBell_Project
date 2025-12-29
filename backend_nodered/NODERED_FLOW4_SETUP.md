# Node-RED Flow 4 Setup Guide: Sensor Data Telemetry

## Overview - Luồng Dữ Liệu Cảm Biến

**Flow 4** thu thập và hiển thị dữ liệu cảm biến (nhiệt độ) real-time:

```
ESP32 Sensor → Read Temperature → MQTT: doorbell/sensor/temp
                                            ↓
                                       Node-RED Flow 4
                                            ↓
                                   Supabase Database (sensor_data table)
                                            ↓
                                       Frontend OverviewTab
                                            ↓
                                   Display: 28.5°C (real-time)
```

---

## Database Schema

### Tạo Bảng: `sensor_data`

```sql
CREATE TABLE sensor_data (
    id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    sensor_type TEXT NOT NULL,
    value NUMERIC NOT NULL,
    unit TEXT NOT NULL,
    metadata JSONB
);

-- Add index for faster queries
CREATE INDEX idx_sensor_data_type_created ON sensor_data(sensor_type, created_at DESC);

-- Enable RLS (optional)
ALTER TABLE sensor_data ENABLE ROW LEVEL SECURITY;

-- Allow public read access
CREATE POLICY "Public read access"
ON sensor_data FOR SELECT
USING (true);

-- Allow public insert (for ESP32 via Node-RED)
CREATE POLICY "Public insert access"
ON sensor_data FOR INSERT
WITH CHECK (true);
```

**Ví dụ record**:
```json
{
  "id": 1,
  "created_at": "2025-12-29T12:30:00Z",
  "sensor_type": "temperature",
  "value": 28.5,
  "unit": "C",
  "metadata": {
    "device_id": "ESP32_DOORBELL",
    "raw_timestamp": 1735470000
  }
}
```

---

## MQTT Topic

- **Topic**: `doorbell/sensor/temp`
- **QoS**: 1
- **Payload Format** (JSON):
  ```json
  {
    "value": 28.5,
    "unit": "C",
    "device_id": "ESP32_A1B2C3D4",
    "timestamp": 1735470000
  }
  ```

---

## ESP32 Code

### Config: TOPIC_SENSOR_TEMP

File: `firmware-esp32/src/config.h`

```cpp
#define TOPIC_SENSOR_TEMP "doorbell/sensor/temp"
```

### Publishing Function

File: `firmware-esp32/src/mqtt_service.cpp`

```cpp
void mqttPublishTemperature(float temperatureC) {
    char jsonBuffer[192];
    float temp = round(temperatureC * 10) / 10.0; // Round to 1 decimal
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"value\":%.1f,\"unit\":\"C\",\"device_id\":\"%s\",\"timestamp\":%lu}",
             temp,
             getDeviceId(),
             getTimestamp());
    
    mqttPublishJson(TOPIC_SENSOR_TEMP, jsonBuffer);
}
```

### Read Temperature

File: `firmware-esp32/src/doorbell_features.cpp`

```cpp
void readEnvironmentTemperature() {
    float temp = readTemperatureCelsius();
    
    if (temp > -100.0f) { // Valid reading
        Serial.printf("[FEATURE] Environment temperature: %.1f°C\n", temp);
        
        // Publish to MQTT
        mqttPublishTemperature(temp);
    } else {
        Serial.println("[FEATURE] Failed to read temperature");
    }
}
```

**Gọi định kỳ** trong `doorbell_app.cpp`:
```cpp
void loop() {
    // ... other code ...
    
    // Read temperature every 30 seconds
    static unsigned long lastTempRead = 0;
    if (millis() - lastTempRead > 30000) {
        readEnvironmentTemperature();
        lastTempRead = millis();
    }
}
```

---

## Node-RED Flow 4

### Node 1: `mqtt_sensor_in` (MQTT Input)
- **Type**: mqtt in
- **Name**: Sensor Temperature
- **Topic**: `doorbell/sensor/temp`
- **QoS**: 1
- **Datatype**: JSON

### Node 2: `prepare_sensor_insert` (Function)
- **Name**: Prepare Sensor Insert
- **Code**:
  ```javascript
  const payload = msg.payload;

  if (!payload.value || !payload.timestamp) {
      node.warn('Invalid sensor payload');
      return null;
  }

  msg.payload = {
      sensor_type: 'temperature',
      value: payload.value,
      unit: payload.unit || 'C',
      created_at: new Date(payload.timestamp * 1000).toISOString(),
      metadata: {
          device_id: payload.device_id || 'ESP32_DOORBELL',
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

### Node 3: `insert_sensor_db` (HTTP Request)
- **Type**: http request
- **Name**: Insert Sensor to DB
- **Method**: POST
- **URL**: `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/sensor_data`

### Node 4: `debug_sensor` (Debug)
- **Type**: debug
- **Name**: Sensor DB Response

---

## Frontend Integration

### Fetch Sensor Data

File: `frontend_react/src/pages/SecurityDashboard/index.jsx`

```javascript
const [temperature, setTemperature] = useState(0);
const [isLoadingSensor, setIsLoadingSensor] = useState(false);

useEffect(() => {
  const fetchSensorData = async () => {
    setIsLoadingSensor(true);
    try {
      const { data, error } = await supabase
        .from('sensor_data')
        .select('*')
        .eq('sensor_type', 'temperature')
        .order('created_at', { ascending: false })
        .limit(1);

      if (error) {
        console.error('Error fetching sensor data:', error);
        return;
      }

      if (data && data.length > 0) {
        setTemperature(data[0].value);
      }
    } catch (error) {
      console.error('Error fetching sensor data:', error);
    } finally {
      setIsLoadingSensor(false);
    }
  };

  fetchSensorData();
  
  // Poll every 5 seconds
  const interval = setInterval(fetchSensorData, 5000);
  return () => clearInterval(interval);
}, []);
```

### Display Temperature

File: `frontend_react/src/pages/SecurityDashboard/OverviewTab.jsx`

```jsx
<div className="stat-card">
  <div className="stat-icon temperature">🌡️</div>
  <div className="stat-content">
    <div className="stat-label">Nhiệt độ</div>
    <div className="stat-value">
      {isLoadingSensor ? (
        <Loader2 size={20} className="spinner" />
      ) : (
        `${temperature.toFixed(1)}°C`
      )}
    </div>
  </div>
</div>
```

---

## Testing

### 1. Tạo Database Table

Vào **Supabase Dashboard** → **SQL Editor** → Paste và chạy:

```sql
CREATE TABLE sensor_data (
    id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    sensor_type TEXT NOT NULL,
    value NUMERIC NOT NULL,
    unit TEXT NOT NULL,
    metadata JSONB
);

CREATE INDEX idx_sensor_data_type_created ON sensor_data(sensor_type, created_at DESC);
```

### 2. Deploy Node-RED Flow 4

```bash
cd C:\Users\ADMIN\Documents\GitHub\DoorBell_Project\backend_nodered
node-red --userDir .
```

Mở http://localhost:1880 → Click **Deploy**

### 3. Test ESP32 Publishing

**Serial Monitor Output**:
```
[FEATURE] Environment temperature: 28.5°C
[MQTT] Publishing to doorbell/sensor/temp
[MQTT] Published successfully
```

**MQTT Message**:
```json
{
  "value": 28.5,
  "unit": "C",
  "device_id": "ESP32_A1B2C3D4",
  "timestamp": 1735470000
}
```

### 4. Verify Database Insert

```sql
SELECT * FROM sensor_data 
WHERE sensor_type = 'temperature'
ORDER BY created_at DESC
LIMIT 5;
```

Kết quả:
```
| id | created_at          | sensor_type | value | unit | metadata                    |
|----|---------------------|-------------|-------|------|-----------------------------|
| 1  | 2025-12-29 12:30:00 | temperature | 28.5  | C    | {"device_id": "ESP32_..."}  |
```

### 5. Test Frontend Display

1. Mở http://localhost:5173
2. Tab **"Tổng quan"** → Xem phần nhiệt độ
3. Nhiệt độ tự động cập nhật mỗi 5 giây
4. Giá trị lấy từ database (latest record)

---

## Sensor Data Retention

### Auto-delete Old Data (Optional)

Tạo function xóa data cũ > 7 ngày:

```sql
CREATE OR REPLACE FUNCTION cleanup_old_sensor_data()
RETURNS void AS $$
BEGIN
  DELETE FROM sensor_data
  WHERE created_at < NOW() - INTERVAL '7 days';
END;
$$ LANGUAGE plpgsql;

-- Schedule cron job (requires pg_cron extension)
SELECT cron.schedule(
  'cleanup-sensor-data',
  '0 2 * * *', -- Run at 2 AM daily
  'SELECT cleanup_old_sensor_data();'
);
```

---

## Advanced Features

### 1. Multiple Sensor Types

Extend để support thêm sensors:

**Humidity Sensor**:
```cpp
// ESP32
#define TOPIC_SENSOR_HUMIDITY "doorbell/sensor/humidity"

void mqttPublishHumidity(float humidityPercent) {
    char jsonBuffer[192];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"value\":%.1f,\"unit\":\"%%\",\"device_id\":\"%s\",\"timestamp\":%lu}",
             humidityPercent, getDeviceId(), getTimestamp());
    mqttPublishJson(TOPIC_SENSOR_HUMIDITY, jsonBuffer);
}
```

**Node-RED**: Duplicate Flow 4, change topic → `doorbell/sensor/humidity`

**Database**: Same table, just `sensor_type = 'humidity'`

### 2. Sensor History Chart

Frontend có thể fetch last 24h data để vẽ chart:

```javascript
const { data } = await supabase
  .from('sensor_data')
  .select('*')
  .eq('sensor_type', 'temperature')
  .gte('created_at', new Date(Date.now() - 24*60*60*1000).toISOString())
  .order('created_at', { ascending: true });

// Use Chart.js or Recharts to display
```

---

## Troubleshooting

### Issue 1: Table not found

**Error**: `relation "sensor_data" does not exist`

**Solution**: Chạy SQL tạo bảng trong Supabase SQL Editor

---

### Issue 2: No data inserted

**Symptoms**: Node-RED debug shows 200 OK but no record in DB

**Check**:
1. Verify table name: `sensor_data` (not `sensors`)
2. Check RLS policies allow insert
3. Verify payload structure matches schema

---

### Issue 3: Frontend shows 0°C

**Symptoms**: Temperature always 0

**Debug**:
```javascript
console.log('Sensor data:', data);
```

**Check**:
1. Database có records không?
2. Supabase client có connect đúng không?
3. Browser console có errors?

---

## Summary

**Flow 4** hoàn chỉnh luồng sensor telemetry:

✅ **ESP32**: Read temp every 30s → Publish MQTT `doorbell/sensor/temp`  
✅ **Node-RED**: Receive MQTT → Insert `sensor_data` table  
✅ **Frontend**: Fetch latest value every 5s → Display real-time  
✅ **Database**: Store historical data với index optimization  

**Frequency**:
- ESP32 publish: 30 seconds
- Frontend poll: 5 seconds
- Data retention: 7 days (configurable)

**Next Steps**:
1. Tạo bảng `sensor_data` trong Supabase
2. Deploy Node-RED Flow 4
3. Test ESP32 publishing
4. Verify frontend display
