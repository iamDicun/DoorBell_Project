import axios from 'axios';

const API_BASE = import.meta.env.VITE_API_BASE_URL;

const api = axios.create({
  baseURL: API_BASE,
  headers: {
    'Content-Type': 'application/json'
  }
});

// Events API
export const getEvents = async (params = {}) => {
  const response = await api.get('/events', { params });
  return response.data;
};

export const markEventRead = async (eventId) => {
  const response = await api.patch(`/events/${eventId}/read`);
  return response.data;
};

// Voice Notes API
export const getVoiceNotes = async (params = {}) => {
  const response = await api.get('/voice-notes', { params });
  return response.data;
};

// Sensor Logs API
export const getSensorLogs = async (hours = 24) => {
  const response = await api.get('/sensor-logs', { params: { hours } });
  return response.data;
};

export const getLatestSensor = async () => {
  const response = await api.get('/sensor-logs/latest');
  return response.data;
};

// Settings API
export const getSettings = async () => {
  const response = await api.get('/settings');
  return response.data;
};

export const updateSettings = async (settings) => {
  const response = await api.patch('/settings', settings);
  return response.data;
};

// Commands API
export const takeSnapshot = async () => {
  const response = await api.post('/commands/snapshot');
  return response.data;
};

export const sendAudioMessage = async (audioUrl, volume = 80) => {
  console.log('🔊 [API] Sending audio message...');
  console.log('📍 URL:', audioUrl);
  console.log('🔉 Volume:', volume);
  console.log('🌐 API Base:', API_BASE);
  console.log('📤 Full endpoint:', `${API_BASE}/commands/speak`);
  
  try {
    const payload = {
      audio_url: audioUrl,
      volume
    };
    console.log('📦 Payload:', JSON.stringify(payload, null, 2));
    
    const response = await api.post('/commands/speak', payload);
    console.log('✅ [API] Response received:', response.data);
    return response.data;
  } catch (error) {
    console.error('❌ [API] Error:', error.message);
    if (error.response) {
      console.error('📄 Response status:', error.response.status);
      console.error('📄 Response data:', error.response.data);
    }
    throw error;
  }
};

export const toggleSiren = async (action, duration = 5) => {
  const response = await api.post('/commands/siren', {
    action,
    duration
  });
  return response.data;
};

// Quick Responses API
export const getQuickResponses = async () => {
  const response = await api.get('/quick-responses');
  return response.data;
};

export const updateQuickResponse = async (title, audioUrl) => {
  const response = await api.put(`/quick-responses/${title}`, {
    audio_url: audioUrl
  });
  return response.data;
};

export default api;