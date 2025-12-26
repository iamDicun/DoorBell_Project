#include "audio.h"
#include <WebSocketsServer.h>
#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// --- Global Variables ---
volatile bool isRecording = false;
volatile bool recordingReady = false;
volatile bool isPlaying = false;
int16_t* recordBuffer = nullptr;
volatile size_t recordedBytes = 0;

// --- Upload buffer ---
uint8_t* uploadBuffer = nullptr;
volatile size_t uploadedBytes = 0;
volatile bool uploadReady = false;

volatile bool wsAudioStreaming = false;
extern WebSocketsServer webSocket;

// Speaker volume (0.0 - 1.0)
float speakerVolume = 0.5f;
AudioOutputI2S* activeAudioOutput = nullptr;

bool micInitialized = false;
bool spkInitialized = false;

// --- Initialize Microphone (I2S_NUM_0) ---
bool initMicrophone() {
    if (micInitialized) return true;
    
    Serial.println("Init Microphone...");
    
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = MIC_SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = 256;
    i2s_config.use_apll = false;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = MIC_SCK_PIN;
    pin_config.ws_io_num = MIC_WS_PIN;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num = MIC_SD_PIN;

    esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Mic install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(MIC_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Mic pins failed: %d\n", err);
        i2s_driver_uninstall(MIC_I2S_PORT);
        return false;
    }

    micInitialized = true;
    Serial.println("Microphone OK!");
    return true;
}

// --- Initialize Speaker (I2S_NUM_1) ---
bool initSpeaker() {
    if (spkInitialized) return true;
    
    Serial.println("Init Speaker...");

    // SAFETY CHECK: GPIO 45 and 46 on ESP32-S3 are INPUT ONLY.
    if (SPK_DATA_PIN == 45 || SPK_DATA_PIN == 46) {
        Serial.println("\n\n!!! CRITICAL HARDWARE WARNING !!!");
        Serial.printf("GPIO %d is INPUT-ONLY on ESP32-S3. The speaker data line is FLOATING.\n", SPK_DATA_PIN);
        Serial.println("This causes LOUD NOISE and OVERHEATING. Please change SPK_DATA_PIN in config.h to a valid output (e.g., 40, 41, 42, 1, 2...)\n\n");
    }
    
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = SPK_SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = 256;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = true;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = SPK_BCK_PIN;
    pin_config.ws_io_num = SPK_WS_PIN;
    pin_config.data_out_num = SPK_DATA_PIN;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(SPK_I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Speaker install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(SPK_I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Speaker pins failed: %d\n", err);
        i2s_driver_uninstall(SPK_I2S_PORT);
        return false;
    }

    // Explicitly zero the buffer to ensure the line is driven low (silence) immediately
    i2s_zero_dma_buffer(SPK_I2S_PORT);

    spkInitialized = true;
    Serial.println("Speaker OK!");
    return true;
}

// --- WebSocket Audio Streaming Task ---
void wsAudioStreamTask(void* param) {
    int32_t i2s_buffer[256];
    int16_t wav_buffer[256];
    size_t bytes_read = 0;
    
    Serial.println("WebSocket audio task running...");
    
    while (true) {
        if (wsAudioStreaming && !isRecording && !isPlaying) {
            esp_err_t err = i2s_read(MIC_I2S_PORT, i2s_buffer, sizeof(i2s_buffer), &bytes_read, 100 / portTICK_PERIOD_MS);
            if (err == ESP_OK && bytes_read > 0) {
                int samples = bytes_read / 4;
                
                for (int i = 0; i < samples; i++) {
                    int32_t val = i2s_buffer[i] >> 11;
                    val = constrain(val, -32768, 32767);
                    wav_buffer[i] = (int16_t)val;
                }
                
                webSocket.broadcastBIN((uint8_t*)wav_buffer, samples * 2);
            }
        }
        vTaskDelay(1);
    }
}

void setWsAudioStreaming(bool enabled) {
    wsAudioStreaming = enabled;
}

// --- Recording Task ---
void recordTask(void* param) {
    int32_t i2s_buffer[256];
    size_t bytes_read = 0;
    size_t offset = 0;
    
    Serial.println("Recording started...");
    
    while (isRecording && offset < MIC_BUFFER_SIZE) {
        esp_err_t err = i2s_read(MIC_I2S_PORT, i2s_buffer, sizeof(i2s_buffer), &bytes_read, 100 / portTICK_PERIOD_MS);
        if (err == ESP_OK && bytes_read > 0) {
            int samples = bytes_read / 4;
            for (int i = 0; i < samples && offset < MIC_BUFFER_SIZE; i++) {
                int32_t val = i2s_buffer[i] >> 11;
                val = constrain(val, -32768, 32767);
                recordBuffer[offset / 2] = (int16_t)val;
                offset += 2;
            }
        }
    }
    
    recordedBytes = offset;
    isRecording = false;
    recordingReady = true;
    Serial.printf("Recording done: %d bytes\n", recordedBytes);
    vTaskDelete(NULL);
}

void startRecording() {
    if (isRecording || isPlaying) return;
    
    if (!recordBuffer) {
        recordBuffer = (int16_t*)ps_malloc(MIC_BUFFER_SIZE);
        if (!recordBuffer) {
            Serial.println("Failed to allocate record buffer");
            return;
        }
    }
    
    wsAudioStreaming = false;
    recordingReady = false;
    recordedBytes = 0;
    isRecording = true;
    
    xTaskCreatePinnedToCore(recordTask, "RecordTask", 4096, NULL, 2, NULL, 1);
}

void stopRecording() {
    isRecording = false;
}

bool isRecordingActive() { return isRecording; }
bool isRecordingReady() { return recordingReady; }
size_t getRecordedBytes() { return recordedBytes; }
int16_t* getRecordBuffer() { return recordBuffer; }

// --- Speaker Play Task ---
void speakerPlayTask(void* param) {
    size_t bytes_written = 0;
    size_t offset = 0;
    size_t chunkSize = 512;
    const size_t samplesPerChunk = chunkSize / sizeof(int16_t);
    int16_t tempBuffer[samplesPerChunk];
    
    Serial.println("Playing recording on speaker...");
    
    i2s_zero_dma_buffer(SPK_I2S_PORT);
    
    while (offset < recordedBytes && isPlaying) {
        size_t toWrite = min(chunkSize, recordedBytes - offset);
        size_t samples = toWrite / sizeof(int16_t);
        int16_t* src = recordBuffer + (offset / sizeof(int16_t));
        float volume = speakerVolume;

        for (size_t i = 0; i < samples; i++) {
            int32_t scaled = (int32_t)(src[i] * volume);
            if (scaled > 32767) scaled = 32767;
            else if (scaled < -32768) scaled = -32768;
            tempBuffer[i] = (int16_t)scaled;
        }

        size_t remaining = samples * sizeof(int16_t);
        uint8_t* bufPtr = (uint8_t*)tempBuffer;
        while (remaining > 0 && isPlaying) {
            esp_err_t err = i2s_write(SPK_I2S_PORT, bufPtr, remaining, &bytes_written, 100 / portTICK_PERIOD_MS);
            if (err != ESP_OK) {
                break;
            }
            bufPtr += bytes_written;
            remaining -= bytes_written;
            offset += bytes_written;
        }
    }
    
    i2s_zero_dma_buffer(SPK_I2S_PORT);
    isPlaying = false;
    Serial.println("Playback complete!");
    vTaskDelete(NULL);
}

void playRecording() {
    if (isRecording || isPlaying || !recordingReady || recordedBytes == 0) {
        Serial.println("Cannot play: no recording or busy");
        return;
    }
    
    wsAudioStreaming = false;
    delay(50);
    
    // Re-initialize speaker
    if (!initSpeaker()) {
        Serial.println("Failed to init speaker");
        return;
    }
    
    isPlaying = true;
    xTaskCreatePinnedToCore(speakerPlayTask, "SpeakerTask", 4096, NULL, 2, NULL, 1);
}

bool isPlayingActive() { return isPlaying; }

// --- Upload buffer functions ---
uint8_t* getUploadBuffer() {
    if (!uploadBuffer) {
        uploadBuffer = (uint8_t*)ps_malloc(MAX_UPLOAD_SIZE);
    }
    return uploadBuffer;
}

void setUploadedBytes(size_t bytes) { uploadedBytes = bytes; }
void setUploadReady(bool ready) { uploadReady = ready; }
size_t getUploadedBytes() { return uploadedBytes; }

// Volume control
void setSpeakerVolume(float vol) {
    speakerVolume = constrain(vol, 0.0f, 1.5f);
    
    AudioOutputI2S* output = activeAudioOutput;
    if (output) {
        output->SetGain(speakerVolume);
    }
}

float getSpeakerVolume() { return speakerVolume; }

// --- Custom AudioFileSource from RAM buffer ---
class AudioFileSourceRAM : public AudioFileSource {
public:
    AudioFileSourceRAM(uint8_t* data, size_t len) : _data(data), _len(len), _pos(0) {}
    
    virtual bool open(const char* url) override { _pos = 0; return true; }
    virtual uint32_t read(void* data, uint32_t len) override {
        if (_pos >= _len) return 0;
        uint32_t toRead = min((uint32_t)(_len - _pos), len);
        memcpy(data, _data + _pos, toRead);
        _pos += toRead;
        return toRead;
    }
    virtual bool seek(int32_t pos, int dir) override {
        if (dir == SEEK_SET) _pos = pos;
        else if (dir == SEEK_CUR) _pos += pos;
        else if (dir == SEEK_END) _pos = _len + pos;
        return true;
    }
    virtual bool close() override { return true; }
    virtual bool isOpen() override { return true; }
    virtual uint32_t getSize() override { return _len; }
    virtual uint32_t getPos() override { return _pos; }
    
private:
    uint8_t* _data;
    size_t _len;
    size_t _pos;
};

// --- MP3 Play Task ---
void mp3PlayTask(void* param) {
    Serial.println("Playing MP3...");
    Serial.printf("Size: %d bytes\n", uploadedBytes);
    
    // Uninstall speaker I2S first (ESP8266Audio will configure its own)
    i2s_driver_uninstall(SPK_I2S_PORT);
    spkInitialized = false;
    // Removed delay(100) here to reduce floating time
    
    // Create source directly from RAM
    AudioFileSourceRAM *source = new AudioFileSourceRAM(uploadBuffer, uploadedBytes);
    
    AudioOutputI2S *output = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
    output->SetPinout(SPK_BCK_PIN, SPK_WS_PIN, SPK_DATA_PIN);
    output->SetGain(speakerVolume);  // Use volume directly
    output->SetOutputModeMono(true);
    activeAudioOutput = output;
    
    AudioGeneratorMP3 *mp3 = new AudioGeneratorMP3();
    
    if (mp3->begin(source, output)) {
        Serial.println("MP3 decoding started");
        while (mp3->isRunning() && isPlaying) {
            if (!mp3->loop()) break;
            vTaskDelay(1);
        }
        mp3->stop();
        Serial.println("MP3 finished");
    } else {
        Serial.println("MP3 begin failed!");
    }
    
    delete mp3;
    activeAudioOutput = nullptr;
    delete output;
    delete source;
    
    // CRITICAL FIX: Removed delay(100) that was here.
    // Leaving the pins floating for 100ms after delete output causes DC/Noise into the amp.
    initSpeaker();  // Immediately re-drive the bus with silence
    
    isPlaying = false;
    Serial.println("MP3 playback complete!");
    vTaskDelete(NULL);
}

void playUploadedAudio() {
    if (isRecording || isPlaying) {
        Serial.println("Cannot play: busy");
        return;
    }
    if (!uploadReady || uploadedBytes == 0 || uploadBuffer == nullptr) {
        Serial.println("Cannot play: no uploaded audio");
        return;
    }
    
    wsAudioStreaming = false;
    delay(50);
    
    isPlaying = true;
    Serial.println("Starting MP3 playback...");
    xTaskCreatePinnedToCore(mp3PlayTask, "MP3Task", 16384, NULL, 2, NULL, 1);
}

// --- URL Play Task ---
char playUrlBuffer[256];

void playUrlTask(void* param) {
    Serial.printf("Playing URL: %s\n", playUrlBuffer);
    
    // Uninstall speaker I2S first
    i2s_driver_uninstall(SPK_I2S_PORT);
    spkInitialized = false;
    
    AudioFileSourceHTTPStream *source = new AudioFileSourceHTTPStream(playUrlBuffer);
    AudioOutputI2S *output = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
    output->SetPinout(SPK_BCK_PIN, SPK_WS_PIN, SPK_DATA_PIN);
    output->SetGain(speakerVolume);
    output->SetOutputModeMono(true);
    activeAudioOutput = output;
    
    AudioGeneratorMP3 *mp3 = new AudioGeneratorMP3();
    
    if (mp3->begin(source, output)) {
        Serial.println("URL MP3 decoding started");
        while (mp3->isRunning() && isPlaying) {
            if (!mp3->loop()) {
                mp3->stop();
                break;
            }
            vTaskDelay(1);
        }
    } else {
        Serial.println("URL MP3 begin failed!");
    }
    
    delete mp3;
    activeAudioOutput = nullptr;
    delete output;
    delete source;
    
    initSpeaker();
    isPlaying = false;
    Serial.println("URL playback complete!");
    vTaskDelete(NULL);
}

void playUrl(const char* url) {
    if (isRecording || isPlaying) {
        Serial.println("Cannot play: busy");
        return;
    }
    
    strncpy(playUrlBuffer, url, sizeof(playUrlBuffer) - 1);
    playUrlBuffer[sizeof(playUrlBuffer) - 1] = 0;
    
    wsAudioStreaming = false;
    delay(50);
    
    isPlaying = true;
    xTaskCreatePinnedToCore(playUrlTask, "UrlTask", 16384, NULL, 2, NULL, 1);
}

// --- Web to Speaker Streaming ---
volatile bool isWebStreaming = false;

void startWebStream() {
    if (isRecording || isPlaying || isWebStreaming) return;
    
    Serial.println("Starting Web Stream...");
    
    // Init speaker at 16kHz for voice
    if (!initSpeaker()) return;
    i2s_set_sample_rates(SPK_I2S_PORT, 16000);
    
    isWebStreaming = true;
}

void stopWebStream() {
    if (!isWebStreaming) return;
    
    isWebStreaming = false;
    i2s_zero_dma_buffer(SPK_I2S_PORT);
    
    // Restore default sample rate
    i2s_set_sample_rates(SPK_I2S_PORT, SPK_SAMPLE_RATE);
    Serial.println("Web Stream stopped");
}

void processWebStream(uint8_t* payload, size_t length) {
    if (!isWebStreaming) return;
    
    size_t bytes_written = 0;
    
    // Apply volume if needed (software scaling)
    // Assuming payload is int16_t PCM
    int16_t* samples = (int16_t*)payload;
    size_t count = length / 2;
    
    for (size_t i = 0; i < count; i++) {
        int32_t val = samples[i];
        val = (int32_t)(val * speakerVolume);
        if (val > 32767) val = 32767;
        else if (val < -32768) val = -32768;
        samples[i] = (int16_t)val;
    }
    
    i2s_write(SPK_I2S_PORT, payload, length, &bytes_written, 100 / portTICK_PERIOD_MS);
}

// --- Play Test Tone (Mario melody) ---
void playTone(int freq, int duration_ms) {
    if (!spkInitialized) {
        if (!initSpeaker()) return;
    }
    
    const int sampleRate = SPK_SAMPLE_RATE;
    int totalSamples = (sampleRate * duration_ms) / 1000;
    
    // Use smaller chunks to avoid memory issues
    const int chunkSize = 512;
    int16_t buffer[chunkSize];
    
    int samplesWritten = 0;
    while (samplesWritten < totalSamples) {
        int samplesToWrite = min(chunkSize, totalSamples - samplesWritten);
        
        for (int i = 0; i < samplesToWrite; i++) {
            float t = (float)(samplesWritten + i) / sampleRate;
            // Clean sine wave with envelope to reduce clicks
            float envelope = 1.0;
            int pos = samplesWritten + i;
            if (pos < 50) envelope = (float)pos / 50.0;  // Fade in
            if (totalSamples - pos < 50) envelope = (float)(totalSamples - pos) / 50.0;  // Fade out
            
            float val = sin(2.0 * M_PI * freq * t) * 20000 * speakerVolume * envelope;
            buffer[i] = (int16_t)constrain((int)val, -32767, 32767);
        }
        
        size_t written;
        i2s_write(SPK_I2S_PORT, buffer, samplesToWrite * sizeof(int16_t), &written, portMAX_DELAY);
        samplesWritten += samplesToWrite;
    }
}

void playTestTone() {
    if (isPlaying || isRecording) return;
    
    isPlaying = true;
    Serial.println("Playing Mario melody...");
    
    // Mario melody notes (frequency in Hz) - cleaner version
    int melody[] =    {659, 659, 0, 659, 0, 523, 659, 0, 784, 0, 0, 0, 392, 0};
    int durations[] = {150, 150, 150, 150, 150, 150, 150, 150, 300, 150, 150, 300, 300, 200};
    int notes = sizeof(melody) / sizeof(melody[0]);
    
    for (int i = 0; i < notes && isPlaying; i++) {
        if (melody[i] > 0) {
            playTone(melody[i], durations[i]);
        } else {
            delay(durations[i]);
        }
        delay(30);  // Gap between notes
    }
    
    i2s_zero_dma_buffer(SPK_I2S_PORT);
    isPlaying = false;
    Serial.println("Mario melody complete!");
}
