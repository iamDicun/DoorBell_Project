// Test MQTT doorbell/cmd/settings using MQTT.js
// Run with: node test_mqtt_settings.js

const mqtt = require('mqtt');

// HiveMQ Cloud configuration
const MQTT_BROKER = '1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud';
const MQTT_PORT = 8883;
const MQTT_USERNAME = 'esp_doorbell';
const MQTT_PASSWORD = 'Hcmus123';

const topic = 'doorbell/cmd/settings';

// Test payload - sync_settings with all device settings
const payload = {
    action: 'sync_settings',
    speaker_volume: 75,
    pir_enabled: true,
    alarm_enabled: false,
    notifications_enabled: true,
    do_not_disturb: false,
    alarm_auto_play: true,
    temp_enabled: true
};

console.log('========================================');
console.log('Testing MQTT: doorbell/cmd/settings');
console.log('========================================');
console.log('');
console.log(`MQTT Broker: ${MQTT_BROKER}:${MQTT_PORT}`);
console.log(`Topic: ${topic}`);
console.log(`Payload: ${JSON.stringify(payload, null, 2)}`);
console.log('');

// Connect to HiveMQ Cloud with TLS
const client = mqtt.connect(`mqtts://${MQTT_BROKER}:${MQTT_PORT}`, {
    username: MQTT_USERNAME,
    password: MQTT_PASSWORD,
    rejectUnauthorized: true
});

client.on('connect', () => {
    console.log('✓ Connected to HiveMQ Cloud');
    console.log('');
    
    // Publish the message
    client.publish(topic, JSON.stringify(payload), { qos: 1 }, (err) => {
        if (err) {
            console.error('✗ Failed to publish message:', err.message);
        } else {
            console.log('✓ SUCCESS! Message published to MQTT broker');
            console.log('');
            console.log('Check ESP32 Serial Monitor to see if it received the message!');
            console.log('');
            console.log('========================================');
            console.log('Expected ESP32 Serial Output:');
            console.log('========================================');
            console.log('[MQTT] 📨 CALLBACK TRIGGERED!');
            console.log('[MQTT] Topic: doorbell/cmd/settings');
            console.log(`[MQTT] Message: ${JSON.stringify(payload)}`);
            console.log('[SETTINGS] 🔄 Syncing settings from server...');
            console.log('[Settings] ✓ Speaker volume: 75% (applied)');
            console.log('[Settings] ✓ PIR enabled: YES');
            console.log('...');
        }
        
        // Close connection
        client.end();
    });
});

client.on('error', (err) => {
    console.error('✗ MQTT Connection Error:', err.message);
    console.log('');
    console.log('Common issues:');
    console.log('  1. Check if HiveMQ Cloud cluster is active');
    console.log('  2. Verify username/password are correct');
    console.log('  3. Check firewall settings (port 8883)');
    console.log('  4. Verify cluster URL is correct');
    client.end();
});

client.on('close', () => {
    console.log('');
    console.log('Connection closed.');
});
