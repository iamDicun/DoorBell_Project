import React, { useState, useEffect } from 'react';
import { Camera, RefreshCw, Loader } from 'lucide-react';
import axios from 'axios';
import './CameraLive.css';

function CameraLive({ onSnapshot }) {
  const [latestSnapshot, setLatestSnapshot] = useState(null);
  const [isLoading, setIsLoading] = useState(false);
  const [isTakingSnapshot, setIsTakingSnapshot] = useState(false);
  const [error, setError] = useState(null);

  // Fetch latest snapshot from events table
  const fetchLatestSnapshot = async () => {
    try {
      setIsLoading(true);
      setError(null);
      const response = await axios.get('http://localhost:1880/api/events?type=snapshot&limit=1');
      
      if (response.data.data && response.data.data.length > 0) {
        setLatestSnapshot(response.data.data[0]);
      } else {
        setLatestSnapshot(null);
      }
    } catch (err) {
      console.error('Error fetching latest snapshot:', err);
      setError('Không thể tải ảnh snapshot');
    } finally {
      setIsLoading(false);
    }
  };

  // Load latest snapshot on mount
  useEffect(() => {
    fetchLatestSnapshot();
  }, []);

  // Handle snapshot button click
  const handleTakeSnapshot = async () => {
    try {
      setIsTakingSnapshot(true);
      setError(null);
      
      // Send MQTT command via Node-RED
      await axios.post('http://localhost:1880/api/commands/snapshot');
      
      // Wait a moment for ESP32 to process and upload
      setTimeout(() => {
        fetchLatestSnapshot();
        setIsTakingSnapshot(false);
      }, 3000);
      
    } catch (err) {
      console.error('Error taking snapshot:', err);
      setError('Không thể chụp ảnh. Kiểm tra kết nối ESP32.');
      setIsTakingSnapshot(false);
    }
  };

  const formatTimestamp = (timestamp) => {
    if (!timestamp) return '';
    const date = new Date(timestamp);
    return date.toLocaleString('vi-VN');
  };

  return (
    <div className="section">
      <div className="flex-between" style={{marginBottom: '16px'}}>
        <div className="flex-center">
          <Camera size={24} />
          <h3 className="section-title">Camera Snapshot</h3>
        </div>
        <div style={{display: 'flex', gap: '8px'}}>
          <button 
            onClick={fetchLatestSnapshot} 
            className="btn" 
            title="Làm mới"
            disabled={isLoading}
          >
            <RefreshCw size={18} className={isLoading ? 'spinning' : ''} />
            Làm mới
          </button>
          <button 
            onClick={handleTakeSnapshot} 
            className="btn btn-primary"
            disabled={isTakingSnapshot}
          >
            {isTakingSnapshot ? (
              <>
                <Loader size={18} className="spinning" />
                Đang chụp...
              </>
            ) : (
              <>
                <Camera size={18} />
                Chụp ngay
              </>
            )}
          </button>
        </div>
      </div>
      
      <div className="camera-container">
        {isLoading && !latestSnapshot && (
          <div className="camera-placeholder">
            <Loader size={48} style={{margin: '0 auto 8px'}} className="spinning" />
            <p>Đang tải ảnh...</p>
          </div>
        )}
        
        {error && (
          <div className="camera-placeholder camera-error">
            <Camera size={48} style={{margin: '0 auto 8px', color: '#ff6b6b'}} />
            <p style={{color: '#ff6b6b', fontWeight: 600}}>{error}</p>
          </div>
        )}
        
        {!isLoading && !error && !latestSnapshot && (
          <div className="camera-placeholder">
            <Camera size={48} style={{margin: '0 auto 8px'}} />
            <p>Chưa có ảnh snapshot</p>
            <p style={{fontSize: '12px', opacity: 0.6, marginTop: '8px'}}>
              Nhấn "Chụp ngay" để chụp ảnh từ ESP32
            </p>
          </div>
        )}
        
        {latestSnapshot && latestSnapshot.image_url && (
          <div style={{position: 'relative'}}>
            <img 
              src={latestSnapshot.image_url}
              alt="Latest Snapshot"
              className="camera-stream"
              style={{
                width: '100%',
                height: '100%',
                objectFit: 'contain',
                borderRadius: '8px'
              }}
            />
            <div style={{
              marginTop: '8px',
              fontSize: '12px',
              opacity: 0.7,
              textAlign: 'center'
            }}>
              Chụp lúc: {formatTimestamp(latestSnapshot.created_at)}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default CameraLive;
