import React from 'react';
import ActivityLogs from '../../components/LogsUsed/ActivityLogs';
import LogDetailModal from '../../components/LogsUsed/LogDetailModal';

function LogsTab({ 
  logs, 
  isLoading = false,
  filter, 
  onFilterChange, 
  selectedLog, 
  onViewDetails, 
  onCloseModal 
}) {
  return (
    <>
      <ActivityLogs 
        logs={logs}
        isLoading={isLoading}
        filter={filter}
        onFilterChange={onFilterChange}
        onViewDetails={onViewDetails}
      />
      <LogDetailModal log={selectedLog} onClose={onCloseModal} />
    </>
  );
}

export default LogsTab;
