import React, { useState } from 'react';
import { Bell, BellOff, Volume2, VolumeX } from 'lucide-react';
import './AlarmControl.css';
import axios from 'axios';

function AlarmControl({ isAlarmActive, onToggleAlarm }) {
  const [isSirenPlaying, setIsSirenPlaying] = useState(false);

  const handleManualSiren = async () => {
    const action = isSirenPlaying ? 'OFF' : 'ON';
    try {
      // Call Node-RED API
      await axios.post('http://localhost:1880/api/commands/siren', {
        action: action,
        duration: 15 // Auto off after 15s
      });
      setIsSirenPlaying(!isSirenPlaying);
      
      // If turning on, auto-reset state after 15s (visual only)
      if (action === 'ON') {
        setTimeout(() => setIsSirenPlaying(false), 15000);
      }
    } catch (error) {
      console.error('Error toggling siren:', error);
      alert('Lỗi khi điều khiển còi hú!');
    }
  };

  return (
    <div className="section">
      <div className="flex-between" style={{ marginBottom: '1rem' }}>
        <div className="flex-center">
          {isAlarmActive ? <Bell size={24} color="#ef4444" /> : <BellOff size={24} />}
          <div>
            <h3 className="section-title">Chế độ Báo động (PIR)</h3>
            <p className="section-subtitle">
              {isAlarmActive 
                ? '🚨 Đang giám sát - Sẽ hú còi khi có chuyển động'
                : '✓ Đang tắt giám sát'
              }
            </p>
          </div>
        </div>
        <button
          onClick={onToggleAlarm}
          className={`alarm-toggle ${isAlarmActive ? 'active' : ''}`}
        >
          {isAlarmActive ? 'TẮT' : 'BẬT'}
        </button>
      </div>

      {/* Manual Siren Control */}
      <div className="flex-between" style={{ borderTop: '1px solid #e5e7eb', paddingTop: '1rem' }}>
        <div className="flex-center">
          {isSirenPlaying ? <Volume2 size={24} color="#dc2626" className="animate-pulse" /> : <VolumeX size={24} />}
          <div>
            <h3 className="section-title">Còi hú Khẩn cấp</h3>
            <p className="section-subtitle">
              {isSirenPlaying 
                ? '🔊 ĐANG HÚ CÒI! (Tự tắt sau 15s)'
                : 'Kích hoạt còi hú ngay lập tức'
              }
            </p>
          </div>
        </div>
        <button
          onClick={handleManualSiren}
          style={{ 
            backgroundColor: isSirenPlaying ? '#dc2626' : '#f3f4f6',
            color: isSirenPlaying ? 'white' : '#374151',
            border: 'none',
            padding: '8px 16px',
            borderRadius: '6px',
            fontWeight: '600',
            cursor: 'pointer',
            minWidth: '80px'
          }}
        >
          {isSirenPlaying ? 'DỪNG' : 'HÚ CÒI'}
        </button>
      </div>
    </div>
  );
}

export default AlarmControl;
