import React, { useState } from 'react';
import { Activity, Loader2, ChevronLeft, ChevronRight } from 'lucide-react';
import LogFilter from './LogFilter';
import LogTable from './LogTable';
import './ActivityLogs.css';

function ActivityLogs({ logs, isLoading = false, filter, onFilterChange, onViewDetails }) {
  const [currentPage, setCurrentPage] = useState(1);
  const logsPerPage = 10;

  // Filter by date only
  const filteredLogs = logs.filter(log => {
    if (filter.date && !log.time.includes(filter.date)) {
      return false;
    }
    return true;
  });

  // Pagination
  const totalPages = Math.ceil(filteredLogs.length / logsPerPage);
  const startIndex = (currentPage - 1) * logsPerPage;
  const endIndex = startIndex + logsPerPage;
  const currentLogs = filteredLogs.slice(startIndex, endIndex);

  const goToPage = (page) => {
    setCurrentPage(Math.max(1, Math.min(page, totalPages)));
  };

  return (
    <div className="section">
      <div className="section-header">
        <Activity size={24} />
        <h3 className="section-title">Nhật ký Hoạt động</h3>
        {isLoading && (
          <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: '8px', color: '#3b82f6' }}>
            <Loader2 size={18} style={{ animation: 'spin 1s linear infinite' }} />
            <span style={{ fontSize: '0.875rem' }}>Đang tải...</span>
          </div>
        )}
      </div>

      <LogFilter filter={filter} onFilterChange={onFilterChange} />
      
      {isLoading && logs.length === 0 ? (
        <div style={{ textAlign: 'center', padding: '3rem', color: '#94a3b8' }}>
          <Loader2 size={48} style={{ margin: '0 auto 1rem', animation: 'spin 1s linear infinite' }} />
          <p>Đang tải nhật ký hoạt động...</p>
        </div>
      ) : filteredLogs.length === 0 ? (
        <div style={{ textAlign: 'center', padding: '3rem', color: '#94a3b8' }}>
          <Activity size={48} style={{ margin: '0 auto 1rem', opacity: 0.3 }} />
          <p>Không có nhật ký hoạt động</p>
          <p style={{ fontSize: '0.875rem', marginTop: '0.5rem' }}>Các sự kiện chuyển động PIR sẽ xuất hiện ở đây</p>
        </div>
      ) : (
        <>
          <LogTable logs={currentLogs} onViewDetails={onViewDetails} />
          
          {totalPages > 1 && (
            <div style={{
              display: 'flex',
              justifyContent: 'center',
              alignItems: 'center',
              gap: '1rem',
              marginTop: '1.5rem',
              padding: '1rem'
            }}>
              <button
                onClick={() => goToPage(currentPage - 1)}
                disabled={currentPage === 1}
                style={{
                  padding: '0.5rem 1rem',
                  border: '1px solid #e2e8f0',
                  borderRadius: '0.5rem',
                  background: currentPage === 1 ? '#f1f5f9' : 'white',
                  color: currentPage === 1 ? '#94a3b8' : '#334155',
                  cursor: currentPage === 1 ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '0.5rem'
                }}
              >
                <ChevronLeft size={18} />
                Trang trước
              </button>
              
              <span style={{ color: '#64748b', fontSize: '0.875rem' }}>
                Trang {currentPage} / {totalPages} (Hiển thị {startIndex + 1}-{Math.min(endIndex, filteredLogs.length)} / {filteredLogs.length})
              </span>
              
              <button
                onClick={() => goToPage(currentPage + 1)}
                disabled={currentPage === totalPages}
                style={{
                  padding: '0.5rem 1rem',
                  border: '1px solid #e2e8f0',
                  borderRadius: '0.5rem',
                  background: currentPage === totalPages ? '#f1f5f9' : 'white',
                  color: currentPage === totalPages ? '#94a3b8' : '#334155',
                  cursor: currentPage === totalPages ? 'not-allowed' : 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '0.5rem'
                }}
              >
                Trang sau
                <ChevronRight size={18} />
              </button>
            </div>
          )}
        </>
      )}
    </div>
  );
}

export default ActivityLogs;
