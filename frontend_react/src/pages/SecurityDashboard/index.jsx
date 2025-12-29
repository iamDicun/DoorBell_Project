import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { supabase } from '../../lib/supabase';
import OverviewTab from './OverviewTab';
import ImagesTab from './ImagesTab';
import AlertTab from './AlertTab';
import LogsTab from './LogsTab';
import VoicemailTab from './VoicemailTab';
import './SecurityDashboard.css';

function SecurityDashboard() {
  // State management
  const [notificationEnabled, setNotificationEnabled] = useState(true);
  const [events, setEvents] = useState([]);
  const [isLoadingEvents, setIsLoadingEvents] = useState(false);
  const [pirAlerts, setPirAlerts] = useState([]);
  const [isLoadingAlerts, setIsLoadingAlerts] = useState(false);
  const [activityLogs, setActivityLogs] = useState([]);
  const [isLoadingLogs, setIsLoadingLogs] = useState(false);
  const [sensors, setSensors] = useState([
    { id: 1, name: 'Cảm biến chuyển động PIR', type: 'motion', enabled: true },
    { id: 2, name: 'Cảm biến nhiệt độ', type: 'temperature', enabled: true }
  ]);
  const [temperature, setTemperature] = useState(0);
  const [isLoadingSensor, setIsLoadingSensor] = useState(false);
  const [volume, setVolume] = useState(70);
  const [isPushToTalk, setIsPushToTalk] = useState(false);
  const [isAlarmActive, setIsAlarmActive] = useState(false);
  const [activeTab, setActiveTab] = useState('overview');
  const [selectedLog, setSelectedLog] = useState(null);
  const [selectedImage, setSelectedImage] = useState(null);
  const [logFilter, setLogFilter] = useState({ date: '', type: 'all' });
  const [voicemails, setVoicemails] = useState([]);
  const [isLoadingVoicemails, setIsLoadingVoicemails] = useState(false);

  // Fetch events from Node-RED API
  useEffect(() => {
    const fetchEvents = async () => {
      setIsLoadingEvents(true);
      try {
        const response = await axios.get('http://localhost:1880/api/events?limit=20&type=button_press');
        if (response.data.success) {
          // Transform data for display
          const formattedEvents = response.data.data.map(event => ({
            id: event.id,
            image_url: event.image_url,
            time: new Date(event.created_at).toLocaleString('vi-VN'),
            timestamp: event.created_at,
            event_type: event.event_type,
            metadata: event.metadata
          }));
          setEvents(formattedEvents);
        }
      } catch (error) {
        console.error('Error fetching events:', error);
        // Fallback to mock data if API fails
        setEvents([
          { id: 1, time: '27/11/2025 14:23', image_url: null },
          { id: 2, time: '27/11/2025 11:15', image_url: null },
          { id: 3, time: '26/11/2025 16:45', image_url: null }
        ]);
      } finally {
        setIsLoadingEvents(false);
      }
    };

    fetchEvents();
    
    // Poll for new events every 5 seconds
    const interval = setInterval(fetchEvents, 5000);
    return () => clearInterval(interval);
  }, []);

  // Fetch PIR alerts (burst images)
  useEffect(() => {
    const fetchPirAlerts = async () => {
      setIsLoadingAlerts(true);
      try {
        const response = await axios.get('http://localhost:1880/api/events?limit=20&type=pir_motion');
        if (response.data.success) {
          // Group burst images by timestamp (same timestamp = same alert)
          const alertsMap = {};
          
          response.data.data.forEach(event => {
            const timestamp = event.metadata?.raw_timestamp || event.created_at;
            
            if (!alertsMap[timestamp]) {
              alertsMap[timestamp] = {
                timestamp: timestamp,
                time: new Date(event.created_at).toLocaleString('vi-VN'),
                level: event.metadata?.level || 'high',
                message: event.metadata?.message || 'Phát hiện chuyển động đáng ngờ',
                images: [],
                imageCount: 0
              };
            }
            
            // Add image URL to the burst
            if (event.image_url) {
              alertsMap[timestamp].images.push(event.image_url);
              alertsMap[timestamp].imageCount++;
            }
          });
          
          // Convert map to array and sort by timestamp
          const alertsArray = Object.values(alertsMap)
            .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));
          
          setPirAlerts(alertsArray);
        }
      } catch (error) {
        console.error('Error fetching PIR alerts:', error);
        setPirAlerts([]);
      } finally {
        setIsLoadingAlerts(false);
      }
    };

    fetchPirAlerts();
    
    // Poll for new alerts every 5 seconds
    const interval = setInterval(fetchPirAlerts, 5000);
    return () => clearInterval(interval);
  }, []);

  // Fetch activity logs from Supabase (Flow 2.1 - PIR normal motion)
  useEffect(() => {
    const fetchActivityLogs = async () => {
      setIsLoadingLogs(true);
      try {
        const { data, error } = await supabase
          .from('events')
          .select('*')
          .eq('event_type', 'pir_motion')
          .order('created_at', { ascending: false })
          .limit(50);

        if (error) {
          console.error('Error fetching logs from Supabase:', error);
          setActivityLogs([]);
          return;
        }

        // Transform Supabase data for display
        const formattedLogs = data.map(event => {
          const eventTime = new Date(event.created_at).toLocaleString('vi-VN', {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
          });

          // Determine event name based on metadata
          let eventName = 'Phát hiện chuyển động';
          let status = 'Thông báo';
          let detail = 'Cảm biến PIR phát hiện chuyển động tại cửa.';

          if (event.metadata) {
            const meta = typeof event.metadata === 'string' 
              ? JSON.parse(event.metadata) 
              : event.metadata;
            
            if (meta.level) {
              detail = `Cảm biến PIR phát hiện chuyển động mức ${meta.level.toUpperCase()}. ${meta.message || ''}`;
            }
            if (meta.detections) {
              detail += ` Số lần phát hiện: ${meta.detections}.`;
            }
          }

          return {
            id: event.id,
            time: eventTime,
            event: eventName,
            status: status,
            detail: detail,
            image_url: event.image_url,
            metadata: event.metadata,
            raw_timestamp: event.created_at
          };
        });

        setActivityLogs(formattedLogs);
      } catch (error) {
        console.error('Error in fetchActivityLogs:', error);
        setActivityLogs([]);
      } finally {
        setIsLoadingLogs(false);
      }
    };

    fetchActivityLogs();
    
    // Poll for new logs every 10 seconds
    const interval = setInterval(fetchActivityLogs, 10000);
    return () => clearInterval(interval);
  }, []);

  // Fetch voice notes from Node-RED API
  useEffect(() => {
    const fetchVoicemails = async () => {
      setIsLoadingVoicemails(true);
      try {
        const response = await axios.get('http://localhost:1880/api/events?limit=20&type=voice_note');
        if (response.data.success && Array.isArray(response.data.data)) {
          // Transform data for display
          const formattedVoicemails = response.data.data.map(event => {
            const durationSec = (event.metadata?.duration_ms || 0) / 1000;
            const minutes = Math.floor(durationSec / 60);
            const seconds = Math.floor(durationSec % 60);
            
            return {
              id: event.id,
              audio_url: event.audio_url,
              time: new Date(event.created_at).toLocaleString('vi-VN'),
              duration: `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`,
              name: `Tin nhắn #${String(event.id).padStart(3, '0')}`,
              device_id: event.metadata?.device_id || 'ESP32_DOORBELL',
              timestamp: event.created_at
            };
          });
          setVoicemails(formattedVoicemails);
        }
      } catch (error) {
        console.error('Error fetching voicemails:', error);
        setVoicemails([]);
      } finally {
        setIsLoadingVoicemails(false);
      }
    };

    fetchVoicemails();
    
    // Poll for new voicemails every 10 seconds
    const interval = setInterval(fetchVoicemails, 10000);
    return () => clearInterval(interval);
  }, []);

  // Fetch sensor data from Supabase (Flow 4)
  useEffect(() => {
    const fetchSensorData = async () => {
      setIsLoadingSensor(true);
      try {
        const response = await axios.get('http://localhost:1880/api/sensors/latest?type=temperature');
        
        if (response.data.success && response.data.data) {
          // sensor_logs table has 'temperature' field, not 'value'
          setTemperature(response.data.data.temperature);
        }
      } catch (error) {
        console.error('Error fetching sensor data:', error);
      } finally {
        setIsLoadingSensor(false);
      }
    };

    fetchSensorData();
    
    // Poll for new sensor data every 5 seconds
    const interval = setInterval(fetchSensorData, 5000);
    return () => clearInterval(interval);
  }, []);

  // Event handlers
  const handleSnapshot = () => {
    alert('Đã chụp ảnh! Ảnh sẽ được lưu vào thư viện.');
  };

  const toggleSensor = (id) => {
    setSensors(sensors.map(s => s.id === id ? {...s, enabled: !s.enabled} : s));
  };

  const handleToggleAlarm = () => {
    setIsAlarmActive(!isAlarmActive);
    // Send MQTT command: doorbell/cmd/siren
    console.log('Toggle alarm:', !isAlarmActive ? 'ON' : 'OFF');
  };

  const handleSendAudioMessage = (messageType, content) => {
    // messageType: 'predefined', 'tts', 'file'
    // content: message text or file
    console.log('Send audio message:', messageType, content);
    // Upload to Supabase and send MQTT: doorbell/cmd/speak
    alert(`Đang gửi tin nhắn audio: ${messageType}`);
  };

  const handlePlayAlarm = (alert) => {
    // Send MQTT command: doorbell/cmd/siren with payload {"action":"ON"}
    console.log('Play alarm for alert:', alert);
    // TODO: Implement MQTT publish via Node-RED API
    alert('Đang kích hoạt cảnh báo... (Chức năng sẽ được thêm sau)');
  };

  const tabs = [
    { id: 'overview', label: 'Tổng quan' },
    { id: 'alerts', label: 'Báo động' },
    { id: 'images', label: 'Hình ảnh' },
    { id: 'logs', label: 'Nhật ký' },
    { id: 'voicemail', label: 'Hộp thư thoại' }
  ];

  return (
    <div className="dashboard-container">
      <div className="dashboard-max-w">
        <div className="dashboard-header">
          <h1>Hệ Thống Giám Sát An Ninh</h1>
          <p>Quản lý và theo dõi nhà của bạn</p>
        </div>

        <div className="dashboard-tabs">
          {tabs.map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`tab ${activeTab === tab.id ? 'active' : ''}`}
            >
              {tab.label}
            </button>
          ))}
        </div>

        {activeTab === 'overview' && (
          <OverviewTab
            notificationEnabled={notificationEnabled}
            onToggleNotification={() => setNotificationEnabled(!notificationEnabled)}
            temperature={temperature}
            sensors={sensors}
            onToggleSensor={toggleSensor}
            onSnapshot={handleSnapshot}
            isPushToTalk={isPushToTalk}
            onPushToTalkStart={() => setIsPushToTalk(true)}
            onPushToTalkEnd={() => setIsPushToTalk(false)}
            volume={volume}
            onVolumeChange={setVolume}
            isAlarmActive={isAlarmActive}
            onToggleAlarm={handleToggleAlarm}
            onSendAudioMessage={handleSendAudioMessage}
          />
        )}

        {activeTab === 'alerts' && (
          <AlertTab
            alerts={pirAlerts}
            isLoading={isLoadingAlerts}
            onPlayAlarm={handlePlayAlarm}
          />
        )}

        {activeTab === 'images' && (
          <ImagesTab
            images={events}
            isLoading={isLoadingEvents}
            selectedImage={selectedImage}
            onImageClick={setSelectedImage}
            onCloseModal={() => setSelectedImage(null)}
          />
        )}

        {activeTab === 'logs' && (
          <LogsTab
            logs={activityLogs}
            isLoading={isLoadingLogs}
            filter={logFilter}
            onFilterChange={setLogFilter}
            selectedLog={selectedLog}
            onViewDetails={setSelectedLog}
            onCloseModal={() => setSelectedLog(null)}
          />
        )}

        {activeTab === 'voicemail' && (
          <VoicemailTab 
            voicemails={voicemails} 
            isLoading={isLoadingVoicemails}
          />
        )}
      </div>
    </div>
  );
}

export default SecurityDashboard;
