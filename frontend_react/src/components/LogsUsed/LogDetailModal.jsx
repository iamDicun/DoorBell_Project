import React from 'react';
import './LogDetailModal.css';

function LogDetailModal({ log, onClose }) {
  if (!log) return null;

  return (
    <div className="modal" onClick={onClose}>
      <div className="modal-content" onClick={e => e.stopPropagation()}>
        <h3>Chi tiết Sự kiện</h3>
        <div className="modal-field">
          <div className="modal-field-label">Thời gian</div>
          <div className="modal-field-value">{log.time}</div>
        </div>
        <div className="modal-field">
          <div className="modal-field-label">Loại sự kiện</div>
          <div className="modal-field-value">{log.event}</div>
        </div>
        <div className="modal-field">
          <div className="modal-field-label">Trạng thái</div>
          <div>
            <span className={`status-badge ${log.status === 'Cảnh báo' ? 'alert' : ''}`}>
              {log.status}
            </span>
          </div>
        </div>
        <div className="modal-field">
          <div className="modal-field-label">Mô tả chi tiết</div>
          <div className="modal-field-detail">{log.detail}</div>
        </div>
        <button onClick={onClose} className="btn btn-full">
          Đóng
        </button>
      </div>
    </div>
  );
}

export default LogDetailModal;
