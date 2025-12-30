import React from 'react';
import { Thermometer } from 'lucide-react';
import './TemperatureDisplay.css';

function TemperatureDisplay({ temperature, lastUpdate }) {
  const formatTimestamp = (timestamp) => {
    if (!timestamp) return 'Chưa có dữ liệu';
    const date = new Date(timestamp);
    const now = new Date();
    const diffMs = now - date;
    const diffMins = Math.floor(diffMs / 60000);
    
    if (diffMins < 1) return 'Vừa xong';
    if (diffMins < 60) return `${diffMins} phút trước`;
    if (diffMins < 1440) return `${Math.floor(diffMins / 60)} giờ trước`;
    return date.toLocaleString('vi-VN');
  };

  return (
    <div className="section">
      <div className="section-header">
        <Thermometer size={24} />
        <h3 className="section-title">Nhiệt độ Môi trường</h3>
      </div>
      <div className="temp-display">{temperature}°C</div>
      <p className="temp-update">Cập nhật: {formatTimestamp(lastUpdate)}</p>
    </div>
  );
}

export default TemperatureDisplay;
