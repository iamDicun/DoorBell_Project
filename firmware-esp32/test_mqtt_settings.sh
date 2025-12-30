#!/bin/bash
# Test MQTT doorbell/cmd/settings using mosquitto_pub
# Install mosquitto-clients first: sudo apt-get install mosquitto-clients (Linux)
# Or: brew install mosquitto (macOS)
# Or: Download from https://mosquitto.org/download/ (Windows)

MQTT_BROKER="1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud"
MQTT_PORT=8883
MQTT_USERNAME="esp_doorbell"
MQTT_PASSWORD="Hcmus123"
TOPIC="doorbell/cmd/settings"

# Test payload - sync_settings with all device settings
PAYLOAD='{"action":"sync_settings","speaker_volume":75,"pir_enabled":true,"alarm_enabled":false,"notifications_enabled":true,"do_not_disturb":false,"alarm_auto_play":true,"temp_enabled":true}'

echo "========================================"
echo "Testing MQTT: doorbell/cmd/settings"
echo "========================================"
echo ""
echo "MQTT Broker: $MQTT_BROKER:$MQTT_PORT"
echo "Topic: $TOPIC"
echo "Payload: $PAYLOAD"
echo ""
echo "Publishing message..."
echo ""

# Publish using mosquitto_pub
mosquitto_pub \
    -h "$MQTT_BROKER" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USERNAME" \
    -P "$MQTT_PASSWORD" \
    -t "$TOPIC" \
    -m "$PAYLOAD" \
    -q 1 \
    --capath /etc/ssl/certs/

if [ $? -eq 0 ]; then
    echo "✓ SUCCESS! Message published to MQTT broker"
    echo ""
    echo "Check ESP32 Serial Monitor to see if it received the message!"
else
    echo "✗ FAILED to publish message"
    echo ""
    echo "Common issues:"
    echo "  1. mosquitto-clients not installed"
    echo "  2. Check if HiveMQ Cloud cluster is active"
    echo "  3. Verify username/password are correct"
    echo "  4. Check firewall settings (port 8883)"
fi

echo ""
echo "========================================"
echo "Expected ESP32 Serial Output:"
echo "========================================"
echo "[MQTT] 📨 CALLBACK TRIGGERED!"
echo "[MQTT] Topic: doorbell/cmd/settings"
echo "[MQTT] Message: $PAYLOAD"
echo "[SETTINGS] 🔄 Syncing settings from server..."
echo "[Settings] ✓ Speaker volume: 75% (applied)"
echo "[Settings] ✓ PIR enabled: YES"
echo "..."
