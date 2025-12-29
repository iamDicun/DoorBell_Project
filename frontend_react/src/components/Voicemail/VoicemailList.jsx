import React from 'react';
import { Mail, Download, Trash2, Loader2 } from 'lucide-react';
import './VoicemailList.css';

function VoicemailList({ voicemails, isLoading }) {
  const handleDownload = (audioUrl, name) => {
    const link = document.createElement('a');
    link.href = audioUrl;
    link.download = `${name}.wav`;
    link.target = '_blank';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return (
    <div className="section">
      <div className="section-header">
        <Mail size={24} />
        <h3 className="section-title">Hộp thư Thoại - Quản lý File Ghi âm</h3>
      </div>
      
      {isLoading ? (
        <div className="loading-container">
          <Loader2 size={32} className="spinner" />
          <p>Đang tải tin nhắn thoại...</p>
        </div>
      ) : voicemails.length === 0 ? (
        <div className="empty-state">
          <Mail size={48} />
          <p>Chưa có tin nhắn thoại nào</p>
          <p className="empty-state-hint">Giữ nút chuông &gt; 3 giây để ghi âm tin nhắn</p>
        </div>
      ) : (
        voicemails.map(vm => (
          <div key={vm.id} className="voicemail-item">
            <div className="voicemail-info">
              <div className="voicemail-name">{vm.name}</div>
              <div className="voicemail-meta">{vm.time} • {vm.duration}</div>
            </div>
            <div className="voicemail-controls">
              {vm.audio_url ? (
                <audio controls preload="metadata">
                  <source src={vm.audio_url} type="audio/wav" />
                  Trình duyệt không hỗ trợ phát audio.
                </audio>
              ) : (
                <p style={{ color: '#999', fontSize: '14px' }}>Không có file âm thanh</p>
              )}
              {vm.audio_url && (
                <>
                  <button 
                    className="icon-btn" 
                    title="Tải xuống"
                    onClick={() => handleDownload(vm.audio_url, vm.name)}
                  >
                    <Download size={18} />
                  </button>
                  <button className="icon-btn" title="Xóa để giải phóng bộ nhớ">
                    <Trash2 size={18} />
                  </button>
                </>
              )}
            </div>
          </div>
        ))
      )}
    </div>
  );
}

export default VoicemailList;
