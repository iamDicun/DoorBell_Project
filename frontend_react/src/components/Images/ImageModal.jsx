import React from 'react';
import { Camera, Download, X } from 'lucide-react';
import './ImageModal.css';

function ImageModal({ image, onClose }) {
  if (!image) return null;

  const handleDownload = async () => {
    if (!image.image_url) return;
    
    try {
      const response = await fetch(image.image_url);
      const blob = await response.blob();
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `doorbell_${image.id}_${new Date(image.timestamp).getTime()}.jpg`;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } catch (error) {
      console.error('Download failed:', error);
      alert('Không thể tải ảnh. Vui lòng thử lại.');
    }
  };

  return (
    <div className="modal" onClick={onClose}>
      <div className="modal-image" onClick={e => e.stopPropagation()}>
        <div className="modal-header">
          <div className="modal-time">{image.time}</div>
          <button onClick={onClose} className="btn">
            <X size={18} style={{ marginRight: '4px' }} />
            Đóng
          </button>
        </div>
        <div className="modal-image-content">
          {image.image_url ? (
            <img 
              src={image.image_url} 
              alt={`Snapshot ${image.id}`}
              style={{ 
                width: '100%', 
                height: 'auto', 
                maxHeight: '70vh', 
                objectFit: 'contain',
                borderRadius: '8px'
              }}
            />
          ) : (
            <div className="modal-image-placeholder">
              <Camera size={96} style={{margin: '0 auto 16px'}} />
              <p style={{fontSize: '20px'}}>Đang tải ảnh...</p>
              <p className="modal-image-subtitle">
                Vui lòng đợi
              </p>
            </div>
          )}
        </div>
        {image.image_url && (
          <div style={{ marginTop: '16px', textAlign: 'center' }}>
            <button className="btn" onClick={handleDownload} style={{ marginRight: '8px' }}>
              <Download size={18} style={{ marginRight: '4px' }} />
              Tải xuống
            </button>
            {image.event_type && (
              <span style={{ fontSize: '14px', color: '#888', marginLeft: '16px' }}>
                Loại: {image.event_type === 'button_press' ? '🔔 Nhấn nút chuông' : image.event_type}
              </span>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

export default ImageModal;
