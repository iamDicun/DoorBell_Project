import React, { useState, useRef } from 'react';
import { Mic, Send } from 'lucide-react';
import './AudioMessageControl.css';

function AudioMessageControl({ onSendAudioMessage }) {
  const [isRecording, setIsRecording] = useState(false);
  const [recordedAudio, setRecordedAudio] = useState(null);
  const [recordingMessage, setRecordingMessage] = useState('');
  const mediaRecorderRef = useRef(null);
  const audioChunksRef = useRef([]);

  const predefinedMessages = [
    { id: 1, text: 'Vui lòng đợi', icon: '⏰' },
    { id: 2, text: 'Tôi không có nhà', icon: '🏠' },
    { id: 3, text: 'Tôi đang bận', icon: '📵' },
    { id: 4, text: 'Để hàng ở cửa', icon: '📦' }
  ];

  const startRecording = async (messageText) => {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      mediaRecorderRef.current = new MediaRecorder(stream);
      audioChunksRef.current = [];

      mediaRecorderRef.current.ondataavailable = (event) => {
        audioChunksRef.current.push(event.data);
      };

      mediaRecorderRef.current.onstop = () => {
        const audioBlob = new Blob(audioChunksRef.current, { type: 'audio/wav' });
        const audioUrl = URL.createObjectURL(audioBlob);
        setRecordedAudio({ blob: audioBlob, url: audioUrl, message: messageText });
        stream.getTracks().forEach(track => track.stop());
      };

      mediaRecorderRef.current.start();
      setIsRecording(true);
      setRecordingMessage(messageText);
    } catch (err) {
      console.error('Error accessing microphone:', err);
      alert('Không thể truy cập microphone!');
    }
  };

  const stopRecording = () => {
    if (mediaRecorderRef.current && isRecording) {
      mediaRecorderRef.current.stop();
      setIsRecording(false);
    }
  };

  const handlePredefinedMessage = (message) => {
    if (isRecording) {
      stopRecording();
    } else {
      startRecording(message.text);
    }
  };

  const handleSendAudio = () => {
    if (recordedAudio) {
      onSendAudioMessage('recorded', recordedAudio);
      setRecordedAudio(null);
    }
  };

  const handleCancelAudio = () => {
    if (recordedAudio) {
      URL.revokeObjectURL(recordedAudio.url);
      setRecordedAudio(null);
    }
  };

  return (
    <div className="audio-message-control">
      <h3>Gửi Tin Nhắn Audio</h3>
      
      <div className="predefined-messages">
        <h4>Giữ nút để ghi âm:</h4>
        <div className="message-grid">
          {predefinedMessages.map(msg => (
            <button
              key={msg.id}
              className={`message-button ${isRecording && recordingMessage === msg.text ? 'recording' : ''}`}
              onMouseDown={() => handlePredefinedMessage(msg)}
              onMouseUp={stopRecording}
              onMouseLeave={stopRecording}
              onTouchStart={() => handlePredefinedMessage(msg)}
              onTouchEnd={stopRecording}
            >
              <span className="message-icon">{msg.icon}</span>
              <span className="message-text">{msg.text}</span>
              {isRecording && recordingMessage === msg.text && (
                <Mic className="recording-icon" size={16} />
              )}
            </button>
          ))}
        </div>
        {isRecording && (
          <p className="recording-hint">🔴 Đang ghi âm... Thả nút để dừng</p>
        )}
      </div>

      {recordedAudio && (
        <div className="recorded-audio">
          <h4>File ghi âm: "{recordedAudio.message}"</h4>
          <div className="audio-player">
            <audio controls src={recordedAudio.url}>
              Trình duyệt không hỗ trợ phát audio.
            </audio>
          </div>
          <div className="audio-actions">
            <button className="send-audio-button" onClick={handleSendAudio}>
              <Send size={20} />
              <span>Phát file ghi âm này</span>
            </button>
            <button className="cancel-audio-button" onClick={handleCancelAudio}>
              Hủy
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

export default AudioMessageControl;
