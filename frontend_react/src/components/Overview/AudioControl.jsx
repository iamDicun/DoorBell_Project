import React from 'react';
import { Mic, Volume2 } from 'lucide-react';
import './AudioControl.css';

function AudioControl({ 
  isPushToTalk, 
  onPushToTalkStart, 
  onPushToTalkEnd,
  volume,
  onVolumeChange 
}) {
  return (
    <div className="section">
      <div className="section-header">
        <Mic size={24} />
        <h3 className="section-title">Trung tâm Hội thoại</h3>
      </div>
      <button
        onMouseDown={onPushToTalkStart}
        onMouseUp={onPushToTalkEnd}
        onMouseLeave={onPushToTalkEnd}
        className={`btn-push ${isPushToTalk ? 'on' : 'off'}`}
      >
        {isPushToTalk ? 'ĐANG NÓI...' : 'GIỮ ĐỂ NÓI'}
      </button>
      <div className="volume-control">
        <Volume2 size={20} />
        <input
          type="range"
          min="0"
          max="100"
          value={volume}
          onChange={(e) => onVolumeChange(e.target.value)}
          className="volume-slider"
          style={{
            background: `linear-gradient(to right, white ${volume}%, rgba(255,255,255,0.2) ${volume}%)`
          }}
        />
        <span className="volume-value">{volume}%</span>
      </div>
    </div>
  );
}

export default AudioControl;
