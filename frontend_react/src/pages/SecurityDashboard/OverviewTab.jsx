import React from 'react';
import NotificationToggle from '../../components/Overview/NotificationToggle';
import TemperatureDisplay from '../../components/Overview/TemperatureDisplay';
import SensorManagement from '../../components/Overview/SensorManagement';
import CameraLive from '../../components/Overview/CameraLive';
import AudioControl from '../../components/Overview/AudioControl';
import AlarmControl from '../../components/Overview/AlarmControl';
import AudioMessageControl from '../../components/Overview/AudioMessageControl';

function OverviewTab({
  notificationEnabled,
  onToggleNotification,
  temperature,
  sensors,
  onToggleSensor,
  onSnapshot,
  isPushToTalk,
  onPushToTalkStart,
  onPushToTalkEnd,
  volume,
  onVolumeChange,
  isAlarmActive,
  onToggleAlarm,
  onSendAudioMessage
}) {
  return (
    <div>
      <NotificationToggle 
        enabled={notificationEnabled} 
        onToggle={onToggleNotification} 
      />
      
      <AlarmControl 
        isAlarmActive={isAlarmActive}
        onToggleAlarm={onToggleAlarm}
      />
      
      <TemperatureDisplay temperature={temperature} />
      
      <SensorManagement 
        sensors={sensors} 
        onToggleSensor={onToggleSensor} 
      />
      
      <CameraLive onSnapshot={onSnapshot} />
      
      <AudioControl
        isPushToTalk={isPushToTalk}
        onPushToTalkStart={onPushToTalkStart}
        onPushToTalkEnd={onPushToTalkEnd}
        volume={volume}
        onVolumeChange={onVolumeChange}
      />
      
      <AudioMessageControl 
        onSendAudioMessage={onSendAudioMessage}
      />
    </div>
  );
}

export default OverviewTab;
