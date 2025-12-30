# MQTT & Settings Fix TODO List

## Completed Tasks
- [x] **Fix Node-RED `sync_settings` Flow**: Added `temp_enabled` to the MQTT payload so the ESP32 receives the value.
- [x] **Fix ESP32 Subscriptions**: Removed unreliable wildcard subscription (`doorbell/cmd/#`) and added explicit subscriptions for:
    - `doorbell/cmd/settings`
    - `doorbell/cmd/siren`
    - `doorbell/cmd/speak`
    - `doorbell/cmd/snapshot`
- [x] **Implement `temp_enabled` Logic**: Updated `doorbell_app.cpp` to check `getDeviceSettings().temp_enabled` before reading/publishing temperature.
- [x] **Verify Parsing Logic**: Confirmed `mqtt_service.cpp` correctly parses and applies `temp_enabled`.

## Pending Actions (User Side)
- [ ] **Restart Node-RED**: Required to apply the flow changes (adding `temp_enabled` to payload).
- [ ] **Upload Firmware**: Required to apply the subscription fixes and temperature logic.
- [ ] **Test**: Toggle "Temperature Sensor" in the dashboard and verify:
    1.  Node-RED logs `settings_updated`.
    2.  ESP32 logs `[MQTT] ⚙️ Settings sync command detected`.
    3.  ESP32 logs `[Settings] ✓ Temp sensor enabled: YES/NO`.
