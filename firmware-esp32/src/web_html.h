#ifndef WEB_HTML_H
#define WEB_HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>SHARK CAM S3</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  background: linear-gradient(135deg, #1a1a1a 0%, #2d1b2e 100%);
  color: #ff6699;
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 20px;
}
h1 {
  margin: 20px 0;
  font-size: 2em;
  text-shadow: 0 0 20px #ff0055;
}
.video-container {
  width: 90%;
  max-width: 640px;
  border: 4px solid #ff6699;
  border-radius: 15px;
  overflow: hidden;
  box-shadow: 0 0 30px rgba(255, 0, 85, 0.5);
  background: #000;
}
img { width: 100%; height: auto; display: block; }
.controls {
  margin-top: 20px;
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  justify-content: center;
  max-width: 640px;
}
button {
  background: linear-gradient(45deg, #ff0055, #ff6699);
  color: white;
  border: none;
  padding: 12px 25px;
  font-size: 14px;
  font-weight: bold;
  border-radius: 25px;
  cursor: pointer;
  transition: all 0.3s ease;
  box-shadow: 0 4px 15px rgba(255, 0, 85, 0.4);
}
button:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(255, 0, 85, 0.6); }
button:disabled { opacity: 0.5; cursor: not-allowed; transform: none; }
button.active { background: linear-gradient(45deg, #00aa55, #00ff88); }
button.recording { background: linear-gradient(45deg, #ff0000, #ff6666); animation: pulse 1s infinite; }
@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.7; } }
.status {
  margin-top: 15px;
  padding: 10px 20px;
  background: rgba(255, 102, 153, 0.1);
  border-radius: 20px;
  font-size: 14px;
  text-align: center;
}
.audio-meter {
  margin-top: 10px;
  display: flex;
  align-items: center;
  gap: 10px;
}
.meter-bar {
  width: 150px;
  height: 15px;
  background: #333;
  border-radius: 8px;
  overflow: hidden;
}
.meter-fill {
  height: 100%;
  width: 0%;
  background: linear-gradient(90deg, #00ff00, #ffff00, #ff0000);
  transition: width 0.1s;
}
.upload-section {
  margin-top: 20px;
  padding: 15px;
  background: rgba(255,102,153,0.1);
  border-radius: 15px;
  text-align: center;
}
.upload-section h3 { margin-bottom: 10px; font-size: 16px; }
.upload-section input[type=file] { margin: 10px 0; color: #ff6699; }
.volume-control {
  margin-top: 10px;
  display: flex;
  align-items: center;
  gap: 10px;
}
.volume-control input[type=range] {
  width: 150px;
}
</style>
</head><body>
<h1>SHARK CAM S3</h1>
<div class='video-container' id='videoContainer'>
  <img src='' id='video' alt='Camera Stream'>
</div>
<div class='controls'>
  <button onclick='toggleCamera()' id='camBtn' class='active'>CAM ON</button>
  <button onclick='toggleLiveAudio()' id='liveBtn'>LIVE MIC</button>
  <button onclick='toggleWebMic()' id='webMicBtn'>WEB MIC</button>
  <button onclick='recordAudio()' id='recordBtn'>RECORD 10s</button>
  <button onclick='playSpeaker()' id='playBtn' disabled>PLAY REC</button>
  <button onclick='downloadAudio()' id='downloadBtn' disabled>DOWNLOAD</button>
</div>
<div class='audio-meter'>
  <span>MIC</span>
  <div class='meter-bar'><div class='meter-fill' id='levelBar'></div></div>
  <span id='levelText'>0%</span>
</div>
<div class='status' id='status'>Ready</div>

<div class='upload-section'>
  <h3>Speaker Test</h3>
  <button onclick='testTone()' id='testBtn'>🎵 MARIO TEST</button>
  <button onclick='playUpload()' id='playUploadBtn' disabled>Play MP3</button>
  <div class='volume-control'>
    <span>Vol:</span>
    <input type='range' id='volumeSlider' min='0' max='150' value='50' onchange='setVolume(this.value)'>
    <span id='volText'>50%</span>
  </div>
  <h3>Upload MP3</h3>
  <input type='file' id='audioFile' accept='.mp3,audio/mpeg'>
  <button onclick='uploadAudio()' id='uploadBtn'>Upload</button>
  
  <h3>Play from URL</h3>
  <input type='text' id='urlInput' placeholder='http://example.com/song.mp3' style='width: 100%; padding: 8px; margin: 5px 0; border-radius: 5px; border: none;'>
  <button onclick='playUrl()' id='playUrlBtn'>Play URL</button>
</div>

<script>
let ws = null, audioContext = null, isLiveAudio = false, nextPlayTime = 0;
let cameraOn = false;

function toggleCamera() {
  const btn = document.getElementById('camBtn');
  const video = document.getElementById('video');
  
  if (!cameraOn) {
    video.src = '/stream';
    btn.textContent = 'CAM ON';
    btn.classList.add('active');
    cameraOn = true;
  } else {
    video.src = '';
    btn.textContent = 'CAM OFF';
    btn.classList.remove('active');
    cameraOn = false;
  }
}

window.onload = function() { toggleCamera(); };

function toggleLiveAudio() {
  const btn = document.getElementById('liveBtn');
  const status = document.getElementById('status');
  
  if (!isLiveAudio) {
    ws = new WebSocket('ws://' + location.hostname + ':81');
    ws.binaryType = 'arraybuffer';
    
    ws.onopen = () => {
      ws.send('start_audio');
      btn.textContent = 'STOP MIC';
      btn.classList.add('active');
      status.textContent = 'Live streaming...';
      isLiveAudio = true;
      initAudio();
    };
    
    ws.onmessage = (e) => {
      if (e.data instanceof ArrayBuffer) playBuffer(e.data);
    };
    
    ws.onclose = () => stopLiveAudio();
    ws.onerror = () => { status.textContent = 'WebSocket error'; stopLiveAudio(); };
  } else {
    stopLiveAudio();
  }
}

function stopLiveAudio() {
  if (ws) { ws.send('stop_audio'); ws.close(); ws = null; }
  document.getElementById('liveBtn').textContent = 'LIVE MIC';
  document.getElementById('liveBtn').classList.remove('active');
  document.getElementById('status').textContent = 'Ready';
  isLiveAudio = false;
}

function initAudio() {
  if (!audioContext) audioContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
  if (audioContext.state === 'suspended') audioContext.resume();
  nextPlayTime = 0;
}

function playBuffer(buffer) {
  if (!audioContext) return;
  const int16 = new Int16Array(buffer);
  const float32 = new Float32Array(int16.length);
  let max = 0;
  for (let i = 0; i < int16.length; i++) {
    float32[i] = int16[i] / 32768.0;
    max = Math.max(max, Math.abs(float32[i]));
  }
  
  document.getElementById('levelBar').style.width = Math.min(100, max * 150) + '%';
  document.getElementById('levelText').textContent = Math.round(max * 100) + '%';
  
  const audioBuf = audioContext.createBuffer(1, float32.length, 16000);
  audioBuf.getChannelData(0).set(float32);
  
  const source = audioContext.createBufferSource();
  source.buffer = audioBuf;
  const gain = audioContext.createGain();
  gain.gain.value = 3.0;
  source.connect(gain).connect(audioContext.destination);
  
  const now = audioContext.currentTime;
  if (nextPlayTime < now) nextPlayTime = now + 0.05;
  source.start(nextPlayTime);
  nextPlayTime += audioBuf.duration;
}

function recordAudio() {
  const btn = document.getElementById('recordBtn');
  const status = document.getElementById('status');
  
  btn.disabled = true;
  btn.classList.add('recording');
  btn.textContent = 'RECORDING...';
  status.textContent = 'Recording 10 seconds...';
  
  fetch('/record').then(() => {
    setTimeout(() => {
      fetch('/stop_record').then(() => {
        btn.disabled = false;
        btn.classList.remove('recording');
        btn.textContent = 'RECORD 10s';
        document.getElementById('playBtn').disabled = false;
        document.getElementById('downloadBtn').disabled = false;
        status.textContent = 'Recording complete!';
      });
    }, 10000);
  });
}

function playSpeaker() {
  document.getElementById('status').textContent = 'Playing on speaker...';
  fetch('/play').then(() => {
    document.getElementById('status').textContent = 'Playing...';
  });
}

let webMicStream = null;
let webMicContext = null;
let webMicProcessor = null;

function toggleWebMic() {
  const btn = document.getElementById('webMicBtn');
  const status = document.getElementById('status');
  
  if (!webMicStream) {
    // Start Web Mic
    if (!ws) {
      ws = new WebSocket('ws://' + location.hostname + ':81');
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => startWebMicCapture();
    } else {
      startWebMicCapture();
    }
    
    function startWebMicCapture() {
      if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        alert('Microphone access blocked! If using HTTP, go to chrome://flags/#unsafely-treat-insecure-origin-as-secure and add this IP.');
        status.textContent = 'Mic blocked (HTTPS required)';
        return;
      }

      navigator.mediaDevices.getUserMedia({ audio: true }).then(stream => {
        webMicStream = stream;
        webMicContext = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
        const source = webMicContext.createMediaStreamSource(stream);
        webMicProcessor = webMicContext.createScriptProcessor(4096, 1, 1);
        
        source.connect(webMicProcessor);
        webMicProcessor.connect(webMicContext.destination);
        
        ws.send('start_web_mic');
        
        webMicProcessor.onaudioprocess = function(e) {
          if (!ws || ws.readyState !== WebSocket.OPEN) return;
          
          const inputData = e.inputBuffer.getChannelData(0);
          const pcmData = new Int16Array(inputData.length);
          
          for (let i = 0; i < inputData.length; i++) {
            // Convert Float32 to Int16
            let s = Math.max(-1, Math.min(1, inputData[i]));
            pcmData[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
          }
          
          ws.send(pcmData.buffer);
        };
        
        btn.classList.add('active');
        btn.textContent = 'STOP WEB MIC';
        status.textContent = 'Streaming Web Mic to Speaker...';
      }).catch(err => {
        console.error(err);
        status.textContent = 'Mic access denied';
      });
    }
  } else {
    // Stop Web Mic
    if (webMicStream) {
      webMicStream.getTracks().forEach(track => track.stop());
      webMicStream = null;
    }
    if (webMicProcessor) {
      webMicProcessor.disconnect();
      webMicProcessor = null;
    }
    if (webMicContext) {
      webMicContext.close();
      webMicContext = null;
    }
    
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send('stop_web_mic');
    }
    
    btn.classList.remove('active');
    btn.textContent = 'WEB MIC';
    status.textContent = 'Web Mic stopped';
  }
}

function downloadAudio() {
  window.location.href = '/download';
}

function uploadAudio() {
  const fileInput = document.getElementById('audioFile');
  const status = document.getElementById('status');
  
  if (!fileInput.files.length) {
    status.textContent = 'Select a file first';
    return;
  }
  
  const file = fileInput.files[0];
  if (!file.name.endsWith('.mp3')) {
    status.textContent = 'Only MP3 files accepted';
    return;
  }
  
  const formData = new FormData();
  formData.append('file', file);
  
  status.textContent = 'Uploading...';
  document.getElementById('uploadBtn').disabled = true;
  
  fetch('/upload', { method: 'POST', body: formData })
    .then(response => {
      if (response.ok) {
        status.textContent = 'Upload complete!';
        document.getElementById('playUploadBtn').disabled = false;
      } else {
        status.textContent = 'Upload failed';
      }
      document.getElementById('uploadBtn').disabled = false;
    })
    .catch(() => {
      status.textContent = 'Upload error';
      document.getElementById('uploadBtn').disabled = false;
    });
}

function playUpload() {
  document.getElementById('status').textContent = 'Playing uploaded MP3...';
  fetch('/play_upload');
}

function testTone() {
  document.getElementById('status').textContent = 'Playing Mario melody...';
  document.getElementById('testBtn').disabled = true;
  fetch('/test_tone').then(() => {
    setTimeout(() => {
      document.getElementById('testBtn').disabled = false;
      document.getElementById('status').textContent = 'Ready';
    }, 3000);
  });
}

function playUrl() {
  const url = document.getElementById('urlInput').value;
  const status = document.getElementById('status');
  
  if (!url) {
    status.textContent = 'Enter a URL first';
    return;
  }
  
  status.textContent = 'Requesting URL playback...';
  fetch('/play_url?url=' + encodeURIComponent(url))
    .then(response => response.text())
    .then(text => {
      status.textContent = text;
    })
    .catch(err => {
      console.error(err);
      status.textContent = 'Request failed';
    });
}

function setVolume(val) {
  document.getElementById('volText').textContent = val + '%';
  // Send as 0.0 - 1.5
  fetch('/set_volume?vol=' + (val / 100.0));
}
</script>
</body></html>
)rawliteral";

#endif
