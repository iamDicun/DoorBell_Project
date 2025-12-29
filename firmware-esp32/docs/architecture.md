# ESP32-S3 Headless Doorbell Architecture

## Connectivity & Messaging
- **Wi-Fi**: Bring-up via stored SSID/PASS (see `config.h`). Ensure auto-reconnect and captive portal timeout fallback.
- **MQTT over TLS**: Use `WiFiClientSecure` + HiveMQ Cloud endpoint `52bad230d09945aa900f187e730b7934.s1.eu.hivemq.cloud`. Primary port 8883, WebSocket 8884 if needed. Topics:
  - `doorbell/status` – doorbell state events with Supabase URLs (`{"event":"ring","image_url":"https://..."}`).
  - `doorbell/security` – motion alerts with Supabase URLs (`{"alert":"motion_detected","image_url":"https://..."}`).
  - `doorbell/command` – remote control JSON commands with Supabase URLs for audio playback.
  - `doorbell/telemetry` – temperature readings.
  - `doorbell/heartbeat` – keep-alive payload (uptime, RSSI).
- **HTTP(S) client**: 
  - Upload recordings/images directly to **Supabase Storage** REST API
  - Download audio files from Supabase URLs for playback
  - Receive storage URLs after successful upload

## Hardware Inputs
- **Button (GPIO 39)**: Short press → play chime + upload image to Supabase + MQTT `ring` with image URL. Long press (>3s) → record voice clip, upload WAV to Supabase, publish audio URL via MQTT.
- **PIR (GPIO 14)**: ISR posts event to queue, publish motion alert, capture burst images, upload to Supabase, send URLs via MQTT.
- **Thermal/NTC (GPIO 40)**: ADC read every 5 minutes, convert using Beta curve, publish telemetry.

## Audio Subsystem
- **Speaker (I2S1 pins 3/21/47)**: Supports canned chime, MQTT-triggered file download from Supabase URL and playback, streamed audio from WebSocket.
- **Microphone (I2S0 pins 42/41/2)**: Handles recordings, upload to Supabase, outbound streaming, voice notes.
- **Two-way Audio**: Binary PCM frames in/out. When receiving audio command, ESP32 downloads from Supabase URL and plays. Use ring buffers to decouple network jitter from I2S tasks.

## Video Subsystem
- **Camera**: Use ESP32-S3 CAM driver with PSRAM buffers. Provide MJPEG HTTP or WS endpoint (`/camera` or `ws://.../camera`). Burst mode saves JPEGs to PSRAM then uploads directly to Supabase Storage, returns public URLs.

## Event Flow
1. ISRs push events into FreeRTOS queues.
2. Event manager task decides action (local playback/record, upload to Supabase, MQTT publish with URLs).
3. Upload client task handles HTTPS POST to Supabase Storage REST API:
   - Uploads image/audio file
   - Receives storage URL in response
   - Publishes URL via MQTT to Node-RED/Frontend
4. Node-RED and Frontend access files directly from Supabase using URLs.
5. For playback: Frontend/Node-RED uploads to Supabase, sends URL via MQTT, ESP32 downloads and plays.

## Remote Command Handling (MQTT `doorbell/command`)
- `{"action":"play_sound","url":"https://supabase.co/storage/.../alert.mp3"}` – download from URL and play.
- `{"action":"set_volume","value":80}` – update speaker gain + persist to NVS.
- `{"action":"set_alarm","state":true}` – toggle internal siren logic.
- `{"action":"set_config","sensor":"pir","enabled":false}` – enable/disable interrupts dynamically.
- `{"action":"play_audio","url":"https://supabase.co/storage/.../message.wav"}` – download and play audio from Supabase.

## Storage Strategy
- Prefer PSRAM for transient buffers (audio/video). 
- SPIFFS holds configs and embedded sound assets only.
- **All user-generated content (images, recordings) uploaded directly to Supabase Storage**.
- After Supabase upload success, free PSRAM buffer immediately.
- No local file persistence for uploads - stream directly to cloud.
- For playback: Download from Supabase URL to PSRAM buffer, play, then free memory.

## Telemetry & Health
- Schedule timer to publish temperature + supply voltage to `doorbell/telemetry`.
- Heartbeat every 30s includes firmware version, heap, Wi-Fi RSSI, MQTT status, Supabase upload success rate.

## Tasks & Queues (proposed)
- `TaskConnectivity` – Wi-Fi, MQTT reconnect logic.
- `TaskSensor` – button/PIR handling, queueing actions.
- `TaskAudioOut` – chime playback + downloaded audio from Supabase URLs.
- `TaskAudioIn` – microphone capture, recordings, upload to Supabase.
- `TaskCamera` – streaming + burst capture, upload to Supabase.
- `TaskUploader` – HTTPS uploads to Supabase Storage REST API, receives URLs.
- `TaskDownloader` – Downloads audio files from Supabase URLs for playback.
- Shared queues for events and audio buffers; mutex around PSRAM operations.

## Supabase Integration
- **Storage Bucket**: `doorbell-media` (or configurable)
- **Upload Endpoint**: `https://{PROJECT_REF}.supabase.co/storage/v1/object/{bucket}/{path}`
- **Authentication**: Supabase API Key (anon or service_role)
- **File Naming Convention**: 
  - Images: `images/{device_id}/{timestamp}.jpg`
  - Audio: `audio/{device_id}/{timestamp}.wav`
- **Public URLs**: Returned after successful upload, published via MQTT
- **Download**: ESP32 uses HTTPS client to GET from public URL
- **Cleanup**: Old files managed by Supabase lifecycle policies or backend logic
