import React from 'react';
import { Bell, BellOff } from 'lucide-react';
import './AlarmControl.css';

function AlarmControl({ isAlarmActive, onToggleAlarm }) {
  return (
    <div className="section">
      <div className="flex-between">
        <div className="flex-center">
          {isAlarmActive ? <Bell size={24} /> : <BellOff size={24} />}
          <div>
            <h3 className="section-title">Báo động Chống trộm</h3>
            <p className="section-subtitle">
              {isAlarmActive 
                ? '🚨 Báo động đang BẬT - ESP32 sẽ hú còi khi phát hiện chuyển động'
                : '✓ Báo động đang TẮT - Chỉ ghi nhận sự kiện'
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
    </div>
  );
}

export default AlarmControl;
