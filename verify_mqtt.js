const mqtt = require('mqtt');

// Config from firmware-esp32/src/config.h
const BROKER = 'mqtts://1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud:8883';
const USERNAME = 'esp_doorbell';
const PASSWORD = 'Hcmus123';
const TOPIC = 'doorbell/cmd/#';

console.log('Connecting to MQTT Broker...');
console.log(`Broker: ${BROKER}`);

const client = mqtt.connect(BROKER, {
    username: USERNAME,
    password: PASSWORD,
    rejectUnauthorized: false // Allow self-signed certs if needed (HiveMQ usually needs true CA, but let's try loose first)
});

client.on('connect', () => {
    console.log('✅ Connected to MQTT Broker!');
    client.subscribe(TOPIC, (err) => {
        if (!err) {
            console.log(`✅ Subscribed to ${TOPIC}`);
            console.log('Waiting for messages... (Send a command from Frontend now)');
        } else {
            console.error('❌ Subscription failed:', err);
        }
    });
});

// Keep alive
setInterval(() => {
    console.log('...Listening...');
}, 5000);

client.on('message', (topic, message) => {
    console.log('\n📨 Message Received!');
    console.log(`Topic: ${topic}`);
    console.log(`Payload: ${message.toString()}`);
});

client.on('error', (err) => {
    console.error('❌ MQTT Error:', err);
});
