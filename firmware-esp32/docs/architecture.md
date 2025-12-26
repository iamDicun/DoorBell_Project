# ESP32-S3 Headless Doorbell Architecture

## Connectivity & Messaging
- **Wi-Fi**: Bring-up via stored SSID/PASS (see `config.h`). Ensure auto-reconnect and captive portal timeout fallback.
- **MQTT over TLS**: Use `WiFiClientSecure` + HiveMQ Cloud endpoint `52bad230d09945aa900f187e730b7934.s1.eu.hivemq.cloud`. Primary port 8883, WebSocket 8884 if needed. Topics:
  - `doorbell/status` – doorbell state events (`{"event":"ring"}` etc.).
  - `doorbell/security` – motion alerts (`{"alert":"motion_detected"}`).
  - `doorbell/command` – remote control JSON commands.
  - `doorbell/telemetry` – temperature readings.
  - `doorbell/heartbeat` – keep-alive payload (uptime, RSSI).
- **HTTP(S) client**: Upload recordings/images to backend endpoints provided by server config.

## Hardware Inputs
- **Button (GPIO 39)**: Short press → play chime + MQTT `ring`. Long press (>3s) → record voice clip, POST WAV.
- **PIR (GPIO 14)**: ISR posts event to queue, publish motion alert, capture burst images, upload.
- **Thermal/NTC (GPIO 40)**: ADC read every 5 minutes, convert using Beta curve, publish telemetry.

## Audio Subsystem
- **Speaker (I2S1 pins 3/21/47)**: Supports canned chime, MQTT-triggered file playback, streamed audio from WebSocket.
- **Microphone (I2S0 pins 42/41/2)**: Handles recordings, outbound streaming, voice notes.
- **Two-way Audio WS**: Binary PCM frames in/out. Use ring buffers to decouple network jitter from I2S tasks.

## Video Subsystem
- **Camera**: Use ESP32-S3 CAM driver with PSRAM buffers. Provide MJPEG HTTP or WS endpoint (`/camera` or `ws://.../camera`). Burst mode saves JPEGs to PSRAM/SPIFFS before upload.

## Event Flow
1. ISRs push events into FreeRTOS queues.
2. Event manager task decides action (local playback/record, MQTT publish, capture, upload).
3. Upload client task handles HTTP POST so sensor ISR remains lightweight.

## Remote Command Handling (MQTT `doorbell/command`)
- `{"action":"play_sound","file":"alert.mp3"}` – locate in SPIFFS and play.
- `{"action":"set_volume","value":80}` – update speaker gain + persist to NVS.
- `{"action":"set_alarm","state":true}` – toggle internal siren logic.
- `{"action":"set_config","sensor":"pir","enabled":false}` – enable/disable interrupts dynamically.

## Storage Strategy
- Prefer PSRAM for transient buffers (audio/video). SPIFFS holds configs, sound assets, pending uploads under `/tmp` until POST succeeds.
- After upload success, delete local file and free memory.

## Telemetry & Health
- Schedule timer to publish temperature + supply voltage to `doorbell/telemetry`.
- Heartbeat every 30s includes firmware version, heap, Wi-Fi RSSI, MQTT status.

## Tasks & Queues (proposed)
- `TaskConnectivity` – Wi-Fi, MQTT reconnect logic.
- `TaskSensor` – button/PIR handling, queueing actions.
- `TaskAudioOut` – chime playback + WS inbound audio.
- `TaskAudioIn` – microphone capture, recordings, outbound stream.
- `TaskCamera` – streaming + burst capture.
- `TaskUploader` – HTTP uploads for recordings/images.
- Shared queues for events and audio buffers; mutex around SPIFFS operations.
