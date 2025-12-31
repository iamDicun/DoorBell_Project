import React from 'react';
import AlertList from '../../components/Alert/AlertList';

function AlertTab({ alerts, isLoading, onPlayAlarm, onMarkAsRead }) {
  return (
    <AlertList 
      alerts={alerts} 
      isLoading={isLoading}
      onPlayAlarm={onPlayAlarm}
      onMarkAsRead={onMarkAsRead}
    />
  );
}

export default AlertTab;
