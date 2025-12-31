import React from 'react';
import { AlertTriangle, Loader2 } from 'lucide-react';
import AlertCard from './AlertCard';
import './AlertList.css';

function AlertList({ alerts, isLoading, onPlayAlarm, onMarkAsRead }) {
  if (isLoading) {
    return (
      <div className="section">
        <div className="section-header">
          <AlertTriangle size={24} />
          <h3 className="section-title">Cảnh báo An ninh</h3>
        </div>
        <div style={{ textAlign: 'center', padding: '40px' }}>
          <Loader2 size={48} className="spinner" />
          <p style={{ marginTop: '16px', color: '#888' }}>Đang tải cảnh báo...</p>
        </div>
      </div>
    );
  }

  if (alerts.length === 0) {
    return (
      <div className="section">
        <div className="section-header">
          <AlertTriangle size={24} />
          <h3 className="section-title">Cảnh báo An ninh</h3>
        </div>
        <div style={{ textAlign: 'center', padding: '40px' }}>
          <AlertTriangle size={64} style={{ margin: '0 auto', color: '#888' }} />
          <p style={{ marginTop: '16px', color: '#888' }}>Không có cảnh báo chưa đọc</p>
          <p style={{ fontSize: '14px', color: '#888' }}>Tất cả cảnh báo đã được xử lý</p>
        </div>
      </div>
    );
  }

  return (
    <div className="section">
      <div className="section-header">
        <AlertTriangle size={24} style={{ color: '#ff4444' }} />
        <h3 className="section-title">Cảnh báo An ninh ({alerts.length})</h3>
      </div>
      <div className="alert-list">
        {alerts.map(alert => (
          <AlertCard 
            key={alert.timestamp} 
            alert={alert} 
            onPlayAlarm={onPlayAlarm}
            onMarkAsRead={onMarkAsRead}
          />
        ))}
      </div>
    </div>
  );
}

export default AlertList;
