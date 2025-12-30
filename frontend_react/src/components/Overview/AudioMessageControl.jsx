import React, { useState, useRef, useEffect } from 'react';
import { Mic, Send, Upload } from 'lucide-react';
import './AudioMessageControl.css';
import { getQuickResponses, updateQuickResponse, sendAudioMessage } from '../../lib/api';
import { uploadAudio } from '../../lib/supabase';
import { convertWebMToWav } from '../../lib/audioConverter';

function AudioMessageControl({ onSendAudioMessage }) {
  const [quickResponses, setQuickResponses] = useState([]);
  const [loading, setLoading] = useState(true);
  const [selectedMessage, setSelectedMessage] = useState(null);
  const [isRecording, setIsRecording] = useState(false);
  const [recordedAudio, setRecordedAudio] = useState(null);
  const mediaRecorderRef = useRef(null);
  const audioChunksRef = useRef([]);

  // Message display mapping
  const messageDisplay = {
    wait: { text: 'Vui lòng đợi', icon: '⏰' },
    notHome: { text: 'Tôi không có nhà', icon: '🏠' },
    busy: { text: 'Tôi đang bận', icon: '📵' },
    package: { text: 'Để hàng ở cửa', icon: '📦' }
  };

  useEffect(() => {
    fetchQuickResponses();
  }, []);

  const fetchQuickResponses = async () => {
    try {
      setLoading(true);
      const response = await getQuickResponses();
      if (response.success) {
        setQuickResponses(response.data);
      }
    } catch (error) {
      console.error('Error fetching quick responses:', error);
      alert('Không thể tải danh sách tin nhắn!');
    } finally {
      setLoading(false);
    }
  };

  const startRecording = async () => {
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      mediaRecorderRef.current = new MediaRecorder(stream);
      audioChunksRef.current = [];

      mediaRecorderRef.current.ondataavailable = (event) => {
        audioChunksRef.current.push(event.data);
      };

      mediaRecorderRef.current.onstop = async () => {
        const webmBlob = new Blob(audioChunksRef.current, { type: 'audio/webm' });
        
        try {
          // Convert WebM (Opus) to true WAV (PCM)
          const wavBlob = await convertWebMToWav(webmBlob);
          const audioUrl = URL.createObjectURL(wavBlob);
          setRecordedAudio({ blob: wavBlob, url: audioUrl });
        } catch (error) {
          console.error('Audio conversion failed:', error);
          alert('Lỗi xử lý âm thanh! Vui lòng thử lại.');
        }
        
        stream.getTracks().forEach(track => track.stop());
      };

      mediaRecorderRef.current.start();
      setIsRecording(true);
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

  const handleSelectMessage = (message) => {
    setSelectedMessage(message);
    setRecordedAudio(null); // Clear previous recording
  };

  const handleUploadRecording = async () => {
    if (!recordedAudio || !selectedMessage) return;

    try {
      setLoading(true);
      
      // Upload to Supabase Storage
      const timestamp = Date.now();
      const fileName = `${selectedMessage.title}_${timestamp}.wav`;
      const audioUrl = await uploadAudio(recordedAudio.blob, fileName);

      // Update quick_response in database
      const updateResponse = await updateQuickResponse(selectedMessage.title, audioUrl);
      
      if (updateResponse.success) {
        alert('Cập nhật ghi âm thành công!');
        // Refresh quick responses
        await fetchQuickResponses();
        setRecordedAudio(null);
      }
    } catch (error) {
      console.error('Error uploading audio:', error);
      alert('Không thể cập nhật ghi âm!');
    } finally {
      setLoading(false);
    }
  };

  const handleSendAudio = async () => {
    if (!selectedMessage) {
      alert('Vui lòng chọn một tin nhắn!');
      return;
    }

    if (!selectedMessage.audio_url) {
      alert('Tin nhắn này chưa có ghi âm. Vui lòng ghi âm trước!');
      return;
    }

    try {
      setLoading(true);
      console.log('🎵 [AudioControl] Sending message:', selectedMessage.title);
      console.log('🎵 [AudioControl] Audio URL:', selectedMessage.audio_url);
      
      const response = await sendAudioMessage(selectedMessage.audio_url, 80);
      console.log('🎵 [AudioControl] Response:', response);
      
      if (response.success) {
        alert('Đã gửi tin nhắn audio đến chuông cửa!');
        if (onSendAudioMessage) {
          onSendAudioMessage('predefined', selectedMessage);
        }
      } else {
        console.error('🎵 [AudioControl] Response not successful:', response);
        alert('Lỗi: ' + (response.error || 'Unknown error'));
      }
    } catch (error) {
      console.error('❌ [AudioControl] Error sending audio:', error);
      alert('Không thể gửi tin nhắn audio: ' + error.message);
    } finally {
      setLoading(false);
    }
  };

  const handleCancelRecording = () => {
    if (recordedAudio) {
      URL.revokeObjectURL(recordedAudio.url);
      setRecordedAudio(null);
    }
  };

  if (loading && quickResponses.length === 0) {
    return <div className="audio-message-control">Đang tải...</div>;
  }

  return (
    <div className="audio-message-control">
      <h3>Gửi Tin Nhắn Audio</h3>
      
      <div className="predefined-messages">
        <h4>Chọn tin nhắn:</h4>
        <div className="message-grid">
          {quickResponses.map(msg => {
            const display = messageDisplay[msg.title] || { text: msg.title, icon: '🔔' };
            const hasAudio = msg.audio_url && msg.audio_url.trim() !== '';
            const isSelected = selectedMessage?.id === msg.id;
            
            return (
              <button
                key={msg.id}
                className={`message-button ${isSelected ? 'selected' : ''} ${!hasAudio ? 'no-audio' : ''}`}
                onClick={() => handleSelectMessage(msg)}
                disabled={loading}
              >
                <span className="message-icon">{display.icon}</span>
                <span className="message-text">{display.text}</span>
                {!hasAudio && <span className="no-audio-badge">🎤 Chưa ghi âm</span>}
                {hasAudio && <span className="has-audio-badge">✅</span>}
              </button>
            );
          })}
        </div>
      </div>

      {selectedMessage && (
        <div className="selected-message-actions">
          <h4>Tin nhắn: {messageDisplay[selectedMessage.title]?.text || selectedMessage.title}</h4>
          
          <div className="recording-section">
            {!isRecording && !recordedAudio && (
              <button 
                className="record-button" 
                onClick={startRecording}
                disabled={loading}
              >
                <Mic size={20} />
                <span>Ghi âm mới</span>
              </button>
            )}
            
            {isRecording && (
              <div className="recording-active">
                <button className="stop-recording-button" onClick={stopRecording}>
                  🔴 Đang ghi... (Click để dừng)
                </button>
              </div>
            )}
            
            {recordedAudio && (
              <div className="recorded-audio">
                <h5>File ghi âm mới:</h5>
                <div className="audio-player">
                  <audio controls src={recordedAudio.url}>
                    Trình duyệt không hỗ trợ phát audio.
                  </audio>
                </div>
                <div className="audio-actions">
                  <button 
                    className="upload-audio-button" 
                    onClick={handleUploadRecording}
                    disabled={loading}
                  >
                    <Upload size={20} />
                    <span>Lưu ghi âm này</span>
                  </button>
                  <button 
                    className="cancel-audio-button" 
                    onClick={handleCancelRecording}
                    disabled={loading}
                  >
                    Hủy
                  </button>
                </div>
              </div>
            )}
          </div>

          <div className="send-section">
            <button 
              className="send-audio-button" 
              onClick={handleSendAudio}
              disabled={loading || !selectedMessage.audio_url}
            >
              <Send size={20} />
              <span>Phát tin nhắn này qua loa</span>
            </button>
            {!selectedMessage.audio_url && (
              <p className="warning-text">⚠️ Cần ghi âm trước khi gửi</p>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

export default AudioMessageControl;
