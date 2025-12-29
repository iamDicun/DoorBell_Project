import React from 'react';
import { Bell, BellOff } from 'lucide-react';
import Toggle from '../common/Toggle';
import './NotificationToggle.css';

function NotificationToggle({ enabled, onToggle }) {
  return (
    <div className="section">
      <div className="flex-between">
        <div className="flex-center">
          {enabled ? <Bell size={24} /> : <BellOff size={24} />}
          <div>
            <h3 className="section-title">Chặn Thông báo (Do Not Disturb)</h3>
            <p className="section-subtitle">
              Tắt thông báo email/điện thoại khi đang ở nhà
            </p>
          </div>
        </div>
        <Toggle isOn={enabled} onToggle={onToggle} />
      </div>
    </div>
  );
}

export default NotificationToggle;
