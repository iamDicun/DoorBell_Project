import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      // Bất cứ request nào bắt đầu bằng /api sẽ được chuyển hướng sang Node-RED
      '/api': {
        target: 'http://localhost:1880', // Địa chỉ Node-RED của anh
        changeOrigin: true,
        secure: false,
        // KHÔNG rewrite - giữ nguyên /api prefix vì Node-RED endpoint cần /api
      },
    },
  },
})
