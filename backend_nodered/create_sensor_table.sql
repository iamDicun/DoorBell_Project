-- Flow 4: Sensor Data Table
-- Run this script in Supabase SQL Editor

-- Create sensor_data table
CREATE TABLE IF NOT EXISTS sensor_data (
    id BIGINT PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    sensor_type TEXT NOT NULL,
    value NUMERIC NOT NULL,
    unit TEXT NOT NULL,
    metadata JSONB
);

-- Add index for faster queries (important for real-time display)
CREATE INDEX IF NOT EXISTS idx_sensor_data_type_created 
ON sensor_data(sensor_type, created_at DESC);

-- Enable Row Level Security (optional but recommended)
ALTER TABLE sensor_data ENABLE ROW LEVEL SECURITY;

-- Policy: Allow public read access
CREATE POLICY IF NOT EXISTS "Public read access"
ON sensor_data FOR SELECT
USING (true);

-- Policy: Allow public insert access (for Node-RED)
CREATE POLICY IF NOT EXISTS "Public insert access"
ON sensor_data FOR INSERT
WITH CHECK (true);

-- Test insert
INSERT INTO sensor_data (sensor_type, value, unit, metadata)
VALUES (
    'temperature',
    28.5,
    'C',
    '{"device_id": "ESP32_TEST", "raw_timestamp": 1735470000}'::jsonb
);

-- Verify insert
SELECT * FROM sensor_data ORDER BY created_at DESC LIMIT 1;

-- Optional: Auto-cleanup function (delete data older than 7 days)
CREATE OR REPLACE FUNCTION cleanup_old_sensor_data()
RETURNS void AS $$
BEGIN
  DELETE FROM sensor_data
  WHERE created_at < NOW() - INTERVAL '7 days';
  
  RAISE NOTICE 'Cleaned up sensor data older than 7 days';
END;
$$ LANGUAGE plpgsql;

-- Test cleanup function
-- SELECT cleanup_old_sensor_data();

-- Optional: Schedule daily cleanup at 2 AM (requires pg_cron extension)
-- SELECT cron.schedule(
--   'cleanup-sensor-data',
--   '0 2 * * *',
--   'SELECT cleanup_old_sensor_data();'
-- );

COMMENT ON TABLE sensor_data IS 'Stores sensor readings from ESP32 doorbell (temperature, humidity, etc.)';
COMMENT ON COLUMN sensor_data.sensor_type IS 'Type of sensor: temperature, humidity, distance, etc.';
COMMENT ON COLUMN sensor_data.value IS 'Numeric sensor reading';
COMMENT ON COLUMN sensor_data.unit IS 'Unit of measurement: C, F, %, cm, etc.';
COMMENT ON COLUMN sensor_data.metadata IS 'Additional data: device_id, raw_timestamp, etc.';
