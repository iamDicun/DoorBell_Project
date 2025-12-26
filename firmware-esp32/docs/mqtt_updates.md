# MQTT Service Updates - HiveMQ Integration

## Tổng quan thay đổi

Đã chỉnh sửa mqtt_service để gửi dữ liệu lên HiveMQ Cloud với các tính năng:
- **PIR Sensor**: Phát hiện chuyển động (motion detection)
- **Temperature Sensor**: Đọc nhiệt độ từ thermistor và tính ra độ Celsius (°C)
- **Distance Sensor**: Đo khoảng cách bằng IR sensor (cm)

## MQTT Topics

### 1. Motion Detection
**Topic**: `doorbell/sensors/motion`
**Format**:
```json
{
  "motion": true,
  "timestamp": 12345678
}
```
- `motion`: `true` = có chuyển động, `false` = không có chuyển động
- Publish ngay khi trạng thái PIR thay đổi

### 2. Temperature Sensor
**Topic**: `doorbell/sensors/temperature`
**Format**:
```json
{
  "temperature": 25.3,
  "unit": "C",
  "timestamp": 12345678
}
```
- `temperature`: Nhiệt độ tính bằng độ C (Celsius), làm tròn 1 chữ số thập phân
- Publish mỗi 10 giây

### 3. Distance Sensor
**Topic**: `doorbell/sensors/distance`
**Format**:
```json
{
  "distance": 45.2,
  "unit": "cm",
  "timestamp": 12345678
}
```
- `distance`: Khoảng cách đo được (cm), làm tròn 1 chữ số thập phân
- Publish mỗi 10 giây

### 4. Telemetry (Tổng hợp)
**Topic**: `doorbell/telemetry`
**Format**:
```json
{
  "motion": true,
  "temperature": 25.3,
  "distance": 45.2,
  "wifi_rssi": -67,
  "timestamp": 12345678
}
```
- Gộp tất cả dữ liệu sensor + WiFi RSSI
- Publish mỗi 10 giây

## Files đã thay đổi

### 1. config.h
- Thêm các MQTT topic mới:
  - `MQTT_TOPIC_MOTION`
  - `MQTT_TOPIC_TEMPERATURE`
  - `MQTT_TOPIC_DISTANCE`

### 2. mqtt_service.h / mqtt_service.cpp
- Thêm các hàm publish mới:
  - `mqttPublishMotion(bool motionDetected)`
  - `mqttPublishTemperature(float temperatureC)`
  - `mqttPublishDistance(float distanceCm)`
  - `mqttPublishTelemetry(bool motion, float temperature, float distance)`
- Sử dụng ArduinoJson để format JSON

### 3. sensor_utils.h / sensor_utils.cpp (MỚI)
- `readPIRSensor()`: Đọc PIR sensor (HIGH = có chuyển động)
- `readTemperatureCelsius()`: Tính nhiệt độ từ thermistor sử dụng Steinhart-Hart equation
- `readDistanceCm()`: Đọc IR distance sensor và chuyển đổi sang cm

### 4. doorbell_app.cpp
- Khởi tạo các pin sensor (PIR, Thermistor, IR)
- Loop kiểm tra PIR mỗi 500ms, publish khi có thay đổi
- Loop publish telemetry mỗi 10 giây

## Cấu hình Sensor trong config.h

```cpp
#define PIR_PIN             48   // Motion detector
#define THERMISTOR_PIN      14   // ADC input for NTC
#define IR_SENSOR_PIN       48   // IR distance sensor

// Thermistor calibration (10k NTC, Beta 3950)
#define THERMISTOR_SERIES_OHMS      10000.0f
#define THERMISTOR_NOMINAL_OHMS     10000.0f
#define THERMISTOR_NOMINAL_TEMP_C   25.0f
#define THERMISTOR_BETA_COEFFICIENT 3950.0f

// IR Distance calibration
#define IR_ADC_RAW_NEAR     3200.0f
#define IR_ADC_RAW_FAR      200.0f
#define IR_DISTANCE_NEAR_CM 8.0f
#define IR_DISTANCE_FAR_CM  80.0f
```

## HiveMQ Cloud Configuration

```cpp
#define MQTT_BROKER    "aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud"
#define MQTT_PORT_TLS  8883
#define MQTT_USERNAME  "doorbell"
#define MQTT_PASSWORD  "Hcmus123"
```

## Cách hoạt động

1. **Motion Detection (PIR)**:
   - Kiểm tra mỗi 500ms
   - Publish ngay khi phát hiện thay đổi trạng thái
   - HIGH = có người/vật thể chuyển động
   - LOW = không có chuyển động

2. **Temperature (Thermistor)**:
   - Đọc ADC từ thermistor (NTC 10kΩ)
   - Tính điện trở từ voltage divider
   - Áp dụng Steinhart-Hart equation để tính nhiệt độ Celsius
   - Công thức: 1/T = 1/T₀ + (1/B) × ln(R/R₀)

3. **Distance (IR Sensor)**:
   - Đọc ADC từ IR distance sensor
   - Linear interpolation giữa các điểm calibration
   - Chuyển đổi sang cm

## Testing với MQTT Explorer

Có thể test bằng MQTT Explorer hoặc mosquitto_sub:

```bash
mosquitto_sub -h aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud \
  -p 8883 -u doorbell -P Hcmus123 \
  -t "doorbell/#" --capath /etc/ssl/certs/
```

Hoặc subscribe các topic riêng lẻ:
- `doorbell/sensors/motion`
- `doorbell/sensors/temperature`
- `doorbell/sensors/distance`
- `doorbell/telemetry`
