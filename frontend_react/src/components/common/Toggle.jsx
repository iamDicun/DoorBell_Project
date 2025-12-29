import React from 'react';
import './Toggle.css';

function Toggle({ isOn, onToggle }) {
  return (
    <button
      onClick={onToggle}
      className={`toggle ${isOn ? 'on' : 'off'}`}
    >
      <div className="toggle-circle" />
    </button>
  );
}

export default Toggle;
