#include "web_server.h"
#include "camera_module.h"
#include "audio.h"
#include "web_html.h"

WebServer server(WEB_PORT);
WebSocketsServer webSocket(WS_PORT);

bool initWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting WiFi");
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        Serial.print(".");
        timeout--;
    }
    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        Serial.printf("\n========================================\n");
        Serial.printf("WiFi Connected!\n");
        Serial.printf("IP Address: %s\n", ip.c_str());
        Serial.printf("Camera Stream: http://%s/stream\n", ip.c_str());
        Serial.printf("Web Interface: http://%s/\n", ip.c_str());
        Serial.printf("========================================\n");
        return true;
    }
    Serial.println("\nWiFi FAILED!");
    return false;
}

void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleStream() {
    WiFiClient client = server.client();
    
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    client.print(response);

    while (client.connected()) {
        camera_fb_t *fb = captureFrame();
        if (!fb) {
            Serial.println("Camera capture failed");
            break;
        }

        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");
        
        releaseFrame(fb);
        
        if (!client.connected()) break;
        delay(30);
        yield();
    }
}

void handleRecord() {
    startRecording();
    server.send(200, "text/plain", "Recording started");
}

void handleStopRecord() {
    stopRecording();
    server.send(200, "text/plain", "Recording stopped");
}

void handlePlay() {
    playRecording();
    server.send(200, "text/plain", "Playing");
}

void handleDownload() {
    if (!isRecordingReady() || getRecordedBytes() == 0) {
        server.send(404, "text/plain", "No recording");
        return;
    }
    
    // WAV header
    uint8_t wavHeader[44];
    uint32_t dataSize = getRecordedBytes();
    uint32_t fileSize = dataSize + 36;
    
    memcpy(wavHeader, "RIFF", 4);
    memcpy(wavHeader + 4, &fileSize, 4);
    memcpy(wavHeader + 8, "WAVEfmt ", 8);
    uint32_t subchunk1Size = 16;
    memcpy(wavHeader + 16, &subchunk1Size, 4);
    uint16_t audioFormat = 1;
    memcpy(wavHeader + 20, &audioFormat, 2);
    uint16_t numChannels = 1;
    memcpy(wavHeader + 22, &numChannels, 2);
    uint32_t sampleRate = MIC_SAMPLE_RATE;
    memcpy(wavHeader + 24, &sampleRate, 4);
    uint32_t byteRate = MIC_SAMPLE_RATE * 2;
    memcpy(wavHeader + 28, &byteRate, 4);
    uint16_t blockAlign = 2;
    memcpy(wavHeader + 32, &blockAlign, 2);
    uint16_t bitsPerSample = 16;
    memcpy(wavHeader + 34, &bitsPerSample, 2);
    memcpy(wavHeader + 36, "data", 4);
    memcpy(wavHeader + 40, &dataSize, 4);
    
    server.setContentLength(44 + dataSize);
    server.sendHeader("Content-Disposition", "attachment; filename=\"recording.wav\"");
    server.send(200, "audio/wav", "");
    
    WiFiClient client = server.client();
    client.write(wavHeader, 44);
    client.write((uint8_t*)getRecordBuffer(), dataSize);
}

void handleUploadAudio() {
    HTTPUpload& upload = server.upload();
    static size_t totalReceived = 0;
    
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Upload: %s\n", upload.filename.c_str());
        
        // Check if MP3
        if (!upload.filename.endsWith(".mp3")) {
            Serial.println("Only MP3 files accepted");
            return;
        }
        
        uint8_t* buf = getUploadBuffer();
        if (!buf) {
            Serial.println("Failed to allocate upload buffer");
            return;
        }
        
        totalReceived = 0;
        setUploadReady(false);
        setUploadedBytes(0);
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        uint8_t* buf = getUploadBuffer();
        if (buf && totalReceived + upload.currentSize <= MAX_UPLOAD_SIZE) {
            memcpy(buf + totalReceived, upload.buf, upload.currentSize);
            totalReceived += upload.currentSize;
        }
        yield();
        
    } else if (upload.status == UPLOAD_FILE_END) {
        setUploadedBytes(totalReceived);
        setUploadReady(true);
        Serial.printf("Upload complete: %d bytes\n", totalReceived);
    }
}

void handleUploadComplete() {
    server.send(200, "text/plain", "Upload OK");
}

void handlePlayUpload() {
    playUploadedAudio();
    server.send(200, "text/plain", "Playing uploaded audio");
}

void handlePlayUrl() {
    if (server.hasArg("url")) {
        String url = server.arg("url");
        playUrl(url.c_str());
        server.send(200, "text/plain", "Playing URL: " + url);
    } else {
        server.send(400, "text/plain", "Missing url parameter");
    }
}

void handleSetVolume() {
    if (server.hasArg("vol")) {
        float vol = server.arg("vol").toFloat();
        setSpeakerVolume(vol);
        server.send(200, "text/plain", "Volume set");
    } else {
        server.send(400, "text/plain", "Missing vol parameter");
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected\n", num);
            setWsAudioStreaming(false);
            stopWebStream();
            break;
        case WStype_CONNECTED:
            Serial.printf("[%u] Connected\n", num);
            break;
        case WStype_TEXT:
            if (strcmp((char*)payload, "start_audio") == 0) {
                setWsAudioStreaming(true);
                Serial.println("Audio streaming started");
            } else if (strcmp((char*)payload, "stop_audio") == 0) {
                setWsAudioStreaming(false);
                Serial.println("Audio streaming stopped");
            } else if (strcmp((char*)payload, "start_web_mic") == 0) {
                startWebStream();
            } else if (strcmp((char*)payload, "stop_web_mic") == 0) {
                stopWebStream();
            }
            break;
        case WStype_BIN:
            processWebStream(payload, length);
            break;
    }
}

void handleTestTone() {
    playTestTone();
    server.send(200, "text/plain", "Playing test tone");
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/stream", handleStream);
    server.on("/record", handleRecord);
    server.on("/stop_record", handleStopRecord);
    server.on("/play", handlePlay);
    server.on("/download", handleDownload);
    server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadAudio);
    server.on("/play_upload", handlePlayUpload);
    server.on("/play_url", handlePlayUrl);
    server.on("/set_volume", handleSetVolume);
    server.on("/test_tone", handleTestTone);
    
    server.begin();
    Serial.println("Web server started");
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket server started");
}
