import React from 'react';
import { Camera } from 'lucide-react';
import './CameraLive.css';

function CameraLive({ onSnapshot }) {
  return (
    <div className="section">
      <div className="flex-between" style={{marginBottom: '16px'}}>
        <div className="flex-center">
          <Camera size={24} />
          <h3 className="section-title">Camera Trực tiếp</h3>
        </div>
        <button onClick={onSnapshot} className="btn">
          <Camera size={18} />
          Chụp nhanh (Snapshot)
        </button>
      </div>
      <div className="camera-container">
        <div className="camera-placeholder">
          <Camera size={48} style={{margin: '0 auto 8px'}} />
          <p>Đang tải video stream...</p>
        </div>
      </div>
    </div>
  );
}

export default CameraLive;
