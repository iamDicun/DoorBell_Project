import React from 'react';
import AlertList from '../../components/Alert/AlertList';

function AlertTab({ alerts, isLoading, onPlayAlarm }) {
  return (
    <AlertList 
      alerts={alerts} 
      isLoading={isLoading}
      onPlayAlarm={onPlayAlarm}
    />
  );
}

export default AlertTab;
