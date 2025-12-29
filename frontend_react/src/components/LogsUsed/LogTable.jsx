import React from 'react';
import './LogTable.css';

function LogTable({ logs, onViewDetails }) {
  return (
    <table>
      <thead>
        <tr>
          <th>Thời gian</th>
          <th>Loại sự kiện</th>
          <th>Trạng thái</th>
          <th style={{textAlign: 'right'}}>Thao tác</th>
        </tr>
      </thead>
      <tbody>
        {logs.map(log => (
          <tr key={log.id}>
            <td className="log-time">{log.time}</td>
            <td>{log.event}</td>
            <td>
              <span className={`status-badge ${log.status === 'Cảnh báo' ? 'alert' : ''}`}>
                {log.status}
              </span>
            </td>
            <td style={{textAlign: 'right'}}>
              <button onClick={() => onViewDetails(log)} className="btn-small">
                Xem chi tiết
              </button>
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

export default LogTable;
