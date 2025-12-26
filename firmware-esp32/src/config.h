#ifndef CONFIG_H
#define CONFIG_H

// --- WIFI CONFIGURATION ---
#define WIFI_SSID           "Dinh"
#define WIFI_PASS           "dicuongne"

// --- MQTT (HiveMQ Cloud) ---
#define MQTT_BROKER         "aaf300d67e7447499464e9b37bd11547.s1.eu.hivemq.cloud"
#define MQTT_PORT_TLS       8883
#define MQTT_PORT_WSS       8884
#define MQTT_USERNAME       "doorbell"
#define MQTT_PASSWORD       "Hcmus123"
#define MQTT_TOPIC_STATUS       "doorbell/status"
#define MQTT_TOPIC_SECURITY     "doorbell/security"
#define MQTT_TOPIC_COMMAND      "doorbell/command"
#define MQTT_TOPIC_TELEMETRY    "doorbell/telemetry"
#define MQTT_TOPIC_HEARTBEAT    "doorbell/heartbeat"
#define MQTT_TOPIC_MOTION       "doorbell/sensors/motion"
#define MQTT_TOPIC_TEMPERATURE  "doorbell/sensors/temperature"
#define MQTT_TOPIC_DISTANCE     "doorbell/sensors/distance" // bỏ

// --- Backend Upload Endpoints (HTTP POST) ---
#define BACKEND_BASE_URL    "https://backend.example.com"
#define ENDPOINT_VOICE_NOTE "/api/doorbell/voice"
#define ENDPOINT_BURST_IMG  "/api/doorbell/motion"

// --- SENSOR / CONTROL PINS ---
#define BUTTON_PIN          39  // User button, short/long press actions
#define PIR_PIN             48  // Motion detector
#define THERMISTOR_PIN      14  // ADC input for NTC
#define IR_SENSOR_PIN       PIR_PIN  // Analog IR distance sensor (defaults to PIR pin)


// --- IR DISTANCE CALIBRATION (tune for your sensor) ---
#define IR_ADC_RAW_NEAR     3200.0f   // Raw ADC when object is nearest
#define IR_ADC_RAW_FAR       200.0f   // Raw ADC when object is farthest
#define IR_DISTANCE_NEAR_CM    8.0f   // Distance in cm matching IR_ADC_RAW_NEAR
#define IR_DISTANCE_FAR_CM    80.0f   // Distance in cm matching IR_ADC_RAW_FAR
#define BUTTON_LONG_MS      3000

// --- AUDIO DEFAULTS ---
#define DEFAULT_SPK_VOLUME  0.5f

// --- MIC I2S CONFIGURATION (I2S_NUM_0) ---
#define MIC_I2S_PORT    I2S_NUM_0
#define MIC_SCK_PIN     42
#define MIC_WS_PIN      41
#define MIC_SD_PIN      2
#define MIC_SAMPLE_RATE 16000

// --- SPEAKER I2S CONFIGURATION (I2S_NUM_1) ---
#define SPK_I2S_PORT    I2S_NUM_1
#define SPK_BCK_PIN     3
#define SPK_WS_PIN      21
#define SPK_DATA_PIN    47
#define SPK_SAMPLE_RATE 44100

// --- RECORDING CONFIGURATION ---
#define RECORD_DURATION_SEC   10
#define BYTES_PER_SAMPLE      2
#define MIC_BUFFER_SIZE       (MIC_SAMPLE_RATE * BYTES_PER_SAMPLE * RECORD_DURATION_SEC)

// --- UPLOAD CONFIGURATION ---
#define MAX_UPLOAD_SIZE       (2 * 1024 * 1024)  // 2MB max upload

// --- THERMISTOR CALIBRATION (defaults for 10k NTC, Beta 3950) ---
#define THERMISTOR_SERIES_OHMS        10000.0f
#define THERMISTOR_NOMINAL_OHMS       10000.0f
#define THERMISTOR_NOMINAL_TEMP_C     25.0f
#define THERMISTOR_BETA_COEFFICIENT   3950.0f

// --- CAMERA PINOUT (ESP32-S3-CAM) ---
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D7      16
#define CAM_PIN_D6      17
#define CAM_PIN_D5      18
#define CAM_PIN_D4      12
#define CAM_PIN_D3      10
#define CAM_PIN_D2      8
#define CAM_PIN_D1      9
#define CAM_PIN_D0      11
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

// --- WEB SERVER ---
#define WEB_PORT        80
#define WS_PORT         81

#endif