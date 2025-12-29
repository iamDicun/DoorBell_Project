import { createClient } from '@supabase/supabase-js';

const supabaseUrl = import.meta.env.VITE_SUPABASE_URL;
const supabaseAnonKey = import.meta.env.VITE_SUPABASE_ANON_KEY;

export const supabase = createClient(supabaseUrl, supabaseAnonKey);

// Helper functions
export const uploadImage = async (file, path) => {
  const { data, error } = await supabase.storage
    .from('bell-images')
    .upload(path, file, {
      cacheControl: '3600',
      upsert: false
    });
  
  if (error) throw error;
  
  // Get public URL
  const { data: { publicUrl } } = supabase.storage
    .from('bell-images')
    .getPublicUrl(path);
  
  return publicUrl;
};

export const uploadAudio = async (file, path) => {
  const { data, error } = await supabase.storage
    .from('bell-audio')
    .upload(path, file);
  
  if (error) throw error;
  
  const { data: { publicUrl } } = supabase.storage
    .from('bell-audio')
    .getPublicUrl(path);
  
  return publicUrl;
};