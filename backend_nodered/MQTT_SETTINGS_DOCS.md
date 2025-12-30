# MQTT Settings & Temperature Feature Documentation

## 1. Overview
This document outlines the implementation of the real-time settings synchronization and the temperature sensor feature. It details the data flow from the Frontend to the ESP32 and how the `temp_enabled` setting is applied.

## 2. Data Flow: Settings Update
The system uses a "Notification -> Fetch" pattern to ensure settings are always synchronized correctly.

1.  **Frontend (React)**:
    *   User toggles a setting (e.g., `temp_enabled`).
    *   `axios.put('/api/settings', { temp_enabled: true })` is called.

2.  **Backend (Node-RED)**:
    *   **Endpoint**: `PUT /api/settings`
    *   **Action 1**: Updates the `device_settings` table in Supabase.
    *   **Action 2**: Publishes a notification to MQTT topic `doorbell/cmd/settings`.
        *   Payload: `{"action": "settings_updated", "timestamp": ...}`

3.  **Firmware (ESP32)**:
    *   **Subscription**: Subscribes explicitly to `doorbell/cmd/settings`.
    *   **Trigger**: Receives `settings_updated` payload.
    *   **Action**: Publishes a request to `doorbell/cmd/request_settings`.
        *   Payload: `{"device_id": "ESP32", "action": "get_settings"}`

4.  **Backend (Node-RED)**:
    *   **Trigger**: Listens to `doorbell/cmd/request_settings`.
    *   **Action**: Fetches the latest settings from Supabase.
    *   **Response**: Publishes full settings to `doorbell/cmd/settings`.
        *   Payload: `{"action": "sync_settings", "temp_enabled": true, ...}`

5.  **Firmware (ESP32)**:
    *   **Trigger**: Receives `sync_settings` payload.
    *   **Action**: Updates the global `deviceSettings` struct.
    *   **Log**: Prints `[Settings] 🎯 Settings applied successfully!`.

## 3. Temperature Feature Implementation

### 3.1 Settings Structure (`mqtt_service.h`)
The `DeviceSettings` struct includes the flag:
```cpp
struct DeviceSettings {
    // ... other settings
    bool temp_enabled; // Controls whether temperature is read/published
};
```

### 3.2 Parsing Logic (`mqtt_service.cpp`)
The `mqttHandleCommandPayload` function parses the JSON:
```cpp
if (doc.containsKey("temp_enabled")) {
    bool newTemp = doc["temp_enabled"];
    if (deviceSettings.temp_enabled != newTemp) {
        deviceSettings.temp_enabled = newTemp;
        // Logs the change
    }
}
```

### 3.3 Application Logic (`doorbell_app.cpp`)
The main loop checks the setting before reading:
```cpp
if (now - lastTempRead >= TEMP_READ_INTERVAL_MS) {
    if (getDeviceSettings().temp_enabled) {
        readEnvironmentTemperature(); // Only runs if enabled
    }
}
```

## 4. Troubleshooting MQTT Issues

If settings are not applying:
1.  **Check Subscriptions**: Ensure ESP32 subscribes to `doorbell/cmd/settings` (Fixed by removing wildcard).
2.  **Check Logs**: Look for `[MQTT] ⚙️ Settings sync command detected`.
3.  **Check Node-RED**: Ensure the `sync_settings` flow includes all fields (Fixed `temp_enabled` missing).
