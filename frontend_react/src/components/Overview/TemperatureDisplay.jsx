import React from 'react';
import { Thermometer } from 'lucide-react';
import './TemperatureDisplay.css';

function TemperatureDisplay({ temperature }) {
  return (
    <div className="section">
      <div className="section-header">
        <Thermometer size={24} />
        <h3 className="section-title">Nhiệt độ Môi trường</h3>
      </div>
      <div className="temp-display">{temperature}°C</div>
      <p className="temp-update">Cập nhật: Hôm nay 14:30</p>
    </div>
  );
}

export default TemperatureDisplay;
