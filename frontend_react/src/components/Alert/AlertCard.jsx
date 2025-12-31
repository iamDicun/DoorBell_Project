import React, { useState } from 'react';
import { Camera, Volume2, Clock, Image as ImageIcon, CheckCircle } from 'lucide-react';
import './AlertCard.css';

function AlertCard({ alert, onPlayAlarm, onMarkAsRead }) {
  const [selectedImage, setSelectedImage] = useState(null);

  const handleImageClick = (imageUrl) => {
    setSelectedImage(imageUrl);
  };

  const closeModal = () => {
    setSelectedImage(null);
  };

  return (
    <>
      <div className="alert-card">
        <div className="alert-header">
          <div className="alert-title">
            <Camera size={20} style={{ color: '#ff4444' }} />
            <span>Phát hiện chuyển động đáng ngờ</span>
          </div>
          <div className="alert-time">
            <Clock size={16} />
            <span>{alert.time}</span>
          </div>
        </div>

        <div className="alert-info">
          <p>
            <strong>Mức độ:</strong> 
            <span className={`alert-level alert-level-${alert.level}`}>
              {alert.level === 'high' ? '🚨 Cao' : alert.level === 'medium' ? '⚠️ Trung bình' : '✓ Bình thường'}
            </span>
          </p>
          <p><strong>Số ảnh:</strong> {alert.imageCount} ảnh liên tiếp</p>
          {alert.message && (
            <p className="alert-message">{alert.message}</p>
          )}
        </div>

        <div className="alert-images">
          {alert.images.map((imageUrl, index) => (
            <div 
              key={index} 
              className="alert-image-container"
              onClick={() => handleImageClick(imageUrl)}
            >
              {imageUrl ? (
                <img 
                  src={imageUrl} 
                  alt={`Alert ${index + 1}`}
                  className="alert-image"
                />
              ) : (
                <div className="alert-image-placeholder">
                  <ImageIcon size={32} />
                  <p>Ảnh {index + 1}</p>
                </div>
              )}
              <div className="alert-image-label">#{index + 1}</div>
            </div>
          ))}
        </div>

        <div className="alert-actions">
          <button 
            className="btn-alert btn-mark-read"
            onClick={() => onMarkAsRead(alert)}
            title="Đánh dấu đã đọc"
          >
            <CheckCircle size={18} />
            <span>Đánh dấu đã đọc</span>
          </button>
          <button 
            className="btn-alert btn-disabled"
            onClick={() => onPlayAlarm(alert)}
            disabled
            title="Chức năng sẽ được thêm sau"
          >
            <Volume2 size={18} />
            <span>Phát loa cảnh báo</span>
          </button>
        </div>
      </div>

      {selectedImage && (
        <div className="modal" onClick={closeModal}>
          <div className="modal-image" onClick={e => e.stopPropagation()}>
            <div className="modal-header">
              <div className="modal-time">{alert.time}</div>
              <button onClick={closeModal} className="btn">
                Đóng
              </button>
            </div>
            <div className="modal-image-content">
              <img 
                src={selectedImage} 
                alt="Alert full view"
                style={{ 
                  width: '100%', 
                  height: 'auto', 
                  maxHeight: '70vh', 
                  objectFit: 'contain',
                  borderRadius: '8px'
                }}
              />
            </div>
          </div>
        </div>
      )}
    </>
  );
}

export default AlertCard;
