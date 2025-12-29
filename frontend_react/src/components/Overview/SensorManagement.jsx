import React from 'react';
import Toggle from '../common/Toggle';
import './SensorManagement.css';

function SensorManagement({ sensors, onToggleSensor }) {
  return (
    <div className="section">
      <h3 className="section-title" style={{marginBottom: '16px'}}>
        Quản lý Cảm biến
      </h3>
      {sensors.map(sensor => (
        <div key={sensor.id} className="sensor-item">
          <div>
            <div className="sensor-name">{sensor.name}</div>
            <div className="sensor-type">
              {sensor.type === 'motion' ? 'Chuyển động' : 'Nhiệt độ'}
            </div>
          </div>
          <Toggle 
            isOn={sensor.enabled} 
            onToggle={() => onToggleSensor(sensor.id)} 
          />
        </div>
      ))}
    </div>
  );
}

export default SensorManagement;
