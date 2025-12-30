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
  const [temperatureTimestamp, setTemperatureTimestamp] = useState(null);
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
  const [isLoadingSettings, setIsLoadingSettings] = useState(false);

  // Fetch settings from device_settings table
  useEffect(() => {
    const fetchSettings = async () => {
      setIsLoadingSettings(true);
      try {
        const response = await axios.get('http://localhost:1880/api/settings');
        console.log('[Settings] API Response:', response.data);
        
        if (response.data.success && response.data.data && response.data.data.length > 0) {
          const settings = response.data.data[0];
          
          // Update UI state from database
          setIsAlarmActive(settings.alarm_enabled || false);
          setVolume(settings.speaker_volume || 70);
          setNotificationEnabled(settings.do_not_disturb || false);
          
          // Update sensors state (PIR enabled)
          setSensors(prevSensors => prevSensors.map(s => {
            if (s.type === 'motion') {
              return { ...s, enabled: settings.pir_enabled !== false };
            }
            return s;
          }));
          
          console.log('[Settings] Loaded settings:', settings);
        }
      } catch (error) {
        console.error('[Settings] Error fetching settings:', error);
      } finally {
        setIsLoadingSettings(false);
      }
    };

    fetchSettings();
  }, []);

  // Fetch events from Node-RED API (chỉ load 1 lần)
  useEffect(() => {
    const fetchEvents = async () => {
      setIsLoadingEvents(true);
      try {
        const response = await axios.get('http://localhost:1880/api/events?limit=20&type=button_press');
        console.log('[ImagesTab] API Response:', response.data);
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
          console.log('[ImagesTab] Formatted events:', formattedEvents);
          setEvents(formattedEvents);
        }
      } catch (error) {
        console.error('[ImagesTab] Error fetching events:', error);
        setEvents([]);
      } finally {
        setIsLoadingEvents(false);
      }
    };

    fetchEvents();
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
  }, []);

  // Fetch activity logs from Supabase (Flow 2.1 - PIR normal motion)
  useEffect(() => {
    const fetchActivityLogs = async () => {
      setIsLoadingLogs(true);
      try {
        console.log('[LogsTab] Fetching activity logs from Supabase...');
        const { data, error } = await supabase
          .from('events')
          .select('*')
          .eq('event_type', 'motion_detected')
          .order('created_at', { ascending: false })
          .limit(50);

        if (error) {
          console.error('[LogsTab] Supabase error:', error);
          setActivityLogs([]);
          return;
        }

        console.log('[LogsTab] Raw data from Supabase:', data);
        console.log('[LogsTab] Number of records:', data?.length || 0);

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

        console.log('[LogsTab] Formatted logs:', formattedLogs);
        setActivityLogs(formattedLogs);
      } catch (error) {
        console.error('[LogsTab] Exception in fetchActivityLogs:', error);
        setActivityLogs([]);
      } finally {
        setIsLoadingLogs(false);
      }
    };

    fetchActivityLogs();
  }, []);

  // Fetch voice notes từ bảng voice_notes trong Supabase
  useEffect(() => {
    const fetchVoicemails = async () => {
      setIsLoadingVoicemails(true);
      try {
        console.log('[VoicemailTab] Fetching voice notes from Supabase...');
        const { data, error } = await supabase
          .from('voice_notes')
          .select('*')
          .order('created_at', { ascending: false })
          .limit(20);

        if (error) {
          console.error('[VoicemailTab] Supabase error:', error);
          setVoicemails([]);
          return;
        }

        console.log('[VoicemailTab] Raw data from Supabase:', data);
        console.log('[VoicemailTab] Number of records:', data?.length || 0);

        // Transform data for display
        const formattedVoicemails = data.map(voiceNote => {
          const durationSec = voiceNote.duration_seconds || 0;
          const minutes = Math.floor(durationSec / 60);
          const seconds = durationSec % 60;
          
          return {
            id: voiceNote.id,
            audio_url: voiceNote.audio_url,
            time: new Date(voiceNote.created_at).toLocaleString('vi-VN'),
            duration: `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`,
            name: `Tin nhắn #${String(voiceNote.id).padStart(3, '0')}`,
            device_id: voiceNote.metadata?.device_id || 'ESP32_DOORBELL',
            timestamp: voiceNote.created_at,
            is_listened: voiceNote.is_listened
          };
        });

        console.log('[VoicemailTab] Formatted voicemails:', formattedVoicemails);
        setVoicemails(formattedVoicemails);
      } catch (error) {
        console.error('[VoicemailTab] Exception in fetchVoicemails:', error);
        setVoicemails([]);
      } finally {
        setIsLoadingVoicemails(false);
      }
    };

    fetchVoicemails();
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
          setTemperatureTimestamp(response.data.data.created_at);
        }
      } catch (error) {
        console.error('Error fetching sensor data:', error);
      } finally {
        setIsLoadingSensor(false);
      }
    };

    fetchSensorData();
    // Poll every 60 seconds for new data
    const interval = setInterval(fetchSensorData, 60000);
    return () => clearInterval(interval);
  }, []);

  // Helper function to update settings in database
  const updateSettings = async (settingsUpdate) => {
    try {
      console.log('[Settings] Updating settings:', settingsUpdate);
      const response = await axios.put('http://localhost:1880/api/settings', settingsUpdate);
      
      if (response.data.success) {
        console.log('[Settings] Update successful:', response.data);
        return true;
      } else {
        console.error('[Settings] Update failed:', response.data);
        return false;
      }
    } catch (error) {
      console.error('[Settings] Error updating settings:', error);
      return false;
    }
  };

  // Event handlers
  const handleSnapshot = () => {
    alert('Đã chụp ảnh! Ảnh sẽ được lưu vào thư viện.');
  };

  const toggleSensor = async (id) => {
    const sensor = sensors.find(s => s.id === id);
    const newEnabled = !sensor.enabled;
    
    // Update UI immediately
    setSensors(sensors.map(s => s.id === id ? {...s, enabled: newEnabled} : s));
    
    // Save to database if it's PIR sensor
    if (sensor.type === 'motion') {
      await updateSettings({ pir_enabled: newEnabled });
    }
  };

  const handleToggleAlarm = async () => {
    const newAlarmState = !isAlarmActive;
    
    // Update UI immediately
    setIsAlarmActive(newAlarmState);
    
    // Save to database (this will trigger MQTT sync in Node-RED)
    await updateSettings({ alarm_enabled: newAlarmState });
    
    console.log('[Settings] Alarm toggled:', newAlarmState ? 'ON' : 'OFF');
  };

  const handleSendAudioMessage = async (messageType, content) => {
    // messageType: 'recorded', 'predefined', etc.
    // content: { blob, url, message } for recorded audio
    try {
      console.log('Sending audio message:', messageType, content);
      
      if (messageType === 'recorded' && content.blob) {
        // Step 1: Upload to Supabase Storage
        const timestamp = Date.now();
        const fileName = `message_${timestamp}.wav`;
        
        const { data, error } = await supabase.storage
          .from('bell-audio')
          .upload(fileName, content.blob, {
            contentType: 'audio/wav',
            cacheControl: '3600',
            upsert: false
          });

        if (error) {
          console.error('Upload error:', error);
          alert('Lỗi khi tải lên file audio!');
          return;
        }

        // Step 2: Get public URL
        const { data: { publicUrl } } = supabase.storage
          .from('bell-audio')
          .getPublicUrl(fileName);

        console.log('Audio uploaded:', publicUrl);

        // Step 3: Send command to Node-RED
        const response = await axios.post('http://localhost:1880/api/commands/speak', {
          audio_url: publicUrl,
          volume: volume,
          message_type: 'custom'
        });

        if (response.data.success) {
          alert(`✅ Đã gửi tin nhắn audio: "${content.message}"`);
        } else {
          alert('❌ Lỗi khi gửi lệnh!');
        }
      }
    } catch (error) {
      console.error('Error sending audio message:', error);
      alert('❌ Lỗi: ' + error.message);
    }
  };

  const handlePlayAlarm = (alert) => {
    // Send MQTT command: doorbell/cmd/siren with payload {"action":"ON"}
    console.log('Play alarm for alert:', alert);
    // TODO: Implement MQTT publish via Node-RED API
    alert('Đang kích hoạt cảnh báo... (Chức năng sẽ được thêm sau)');
  };

  const handleToggleNotification = async () => {
    const newState = !notificationEnabled;
    
    // Update UI immediately
    setNotificationEnabled(newState);
    
    // Save to database
    await updateSettings({ do_not_disturb: newState });
  };

  const handleVolumeChange = async (newVolume) => {
    // Update UI immediately
    setVolume(newVolume);
    
    // Debounce API calls - only save after user stops dragging
    if (window.volumeTimeout) clearTimeout(window.volumeTimeout);
    window.volumeTimeout = setTimeout(async () => {
      await updateSettings({ speaker_volume: parseInt(newVolume) });
    }, 500);
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
            onToggleNotification={handleToggleNotification}
            temperature={temperature}
            temperatureTimestamp={temperatureTimestamp}
            sensors={sensors}
            onToggleSensor={toggleSensor}
            onSnapshot={handleSnapshot}
            isPushToTalk={isPushToTalk}
            onPushToTalkStart={() => setIsPushToTalk(true)}
            onPushToTalkEnd={() => setIsPushToTalk(false)}
            volume={volume}
            onVolumeChange={handleVolumeChange}
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
