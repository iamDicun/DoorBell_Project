import React from 'react';
import { Camera, Loader2 } from 'lucide-react';
import './ImageGallery.css';

function ImageGallery({ images, isLoading, onImageClick }) {
  if (isLoading) {
    return (
      <div className="section">
        <div className="section-header">
          <Camera size={24} />
          <h3 className="section-title">Thư viện Hình ảnh</h3>
        </div>
        <div style={{ textAlign: 'center', padding: '40px' }}>
          <Loader2 size={48} className="spinner" />
          <p style={{ marginTop: '16px', color: '#888' }}>Đang tải hình ảnh...</p>
        </div>
      </div>
    );
  }

  if (images.length === 0) {
    return (
      <div className="section">
        <div className="section-header">
          <Camera size={24} />
          <h3 className="section-title">Thư viện Hình ảnh</h3>
        </div>
        <div style={{ textAlign: 'center', padding: '40px' }}>
          <Camera size={64} style={{ margin: '0 auto', color: '#888' }} />
          <p style={{ marginTop: '16px', color: '#888' }}>Chưa có hình ảnh nào</p>
          <p style={{ fontSize: '14px', color: '#888' }}>Nhấn nút chuông để chụp ảnh</p>
        </div>
      </div>
    );
  }

  return (
    <div className="section">
      <div className="section-header">
        <Camera size={24} />
        <h3 className="section-title">Thư viện Hình ảnh ({images.length})</h3>
      </div>
      <div className="grid">
        {images.map(img => (
          <div key={img.id} className="image-card">
            <div
              className="image-placeholder"
              onClick={() => onImageClick(img)}
              style={{
                backgroundImage: img.image_url ? `url(${img.image_url})` : 'none',
                backgroundSize: 'cover',
                backgroundPosition: 'center',
                cursor: 'pointer'
              }}
            >
              {!img.image_url && (
                <div className="image-content">
                  <Camera size={48} style={{margin: '0 auto 8px'}} />
                  <p style={{fontSize: '14px'}}>Đang tải...</p>
                </div>
              )}
            </div>
            <div className="image-info">
              <span>{img.time}</span>
              {img.event_type && (
                <span style={{ fontSize: '12px', color: '#888', marginLeft: '8px' }}>
                  {img.event_type === 'button_press' ? '🔔 Nhấn nút' : img.event_type}
                </span>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

export default ImageGallery;
