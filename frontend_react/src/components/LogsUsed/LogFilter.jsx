import React from 'react';
import './LogFilter.css';

function LogFilter({ filter, onFilterChange }) {
  return (
    <div className="filter-row">
      <div>
        <label className="filter-label">Lọc theo ngày</label>
        <input
          type="date"
          value={filter.date}
          onChange={(e) => onFilterChange({...filter, date: e.target.value})}
          className="filter-input"
        />
      </div>
    </div>
  );
}

export default LogFilter;
