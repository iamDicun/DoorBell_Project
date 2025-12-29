# Doorbell Project - Quick Start

## Prerequisites

1. Node.js v22+ installed
2. Node-RED installed globally: `npm install -g node-red`
3. Supabase project setup với table `events`

## Khởi động hệ thống

### 1. Backend (Node-RED)

```bash
cd backend_nodered
node-red --userDir .
```

✅ Node-RED chạy tại: http://localhost:1880

### 2. Frontend (React)

```bash
cd frontend_react

# Lần đầu: Tạo file .env
echo VITE_SUPABASE_URL=https://xznnnklhqkccylxxzsdh.supabase.co > .env
echo VITE_SUPABASE_ANON_KEY=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8 >> .env

# Khởi động
npm run dev
```

✅ Frontend chạy tại: http://localhost:5173

## Kiểm tra

### Test Node-RED API
```bash
curl http://localhost:1880/api/events?limit=5
```

### Test Frontend
1. Mở http://localhost:5173
2. Tab "Nhật ký" sẽ hiển thị logs từ Supabase
3. Tab "Hình ảnh" và "Báo động" cần Node-RED API

## Cấu trúc project

```
DoorBell_Project/
├── firmware-esp32/        # ESP32 code (PlatformIO)
├── backend_nodered/       # Node-RED flows & API
│   ├── flows.json         # Flow definitions
│   ├── start-nodered.bat  # Start script
│   └── FIX_404_ERROR.md   # Troubleshooting guide
└── frontend_react/        # React dashboard
    ├── .env               # Supabase credentials
    └── src/
```

## Flows đã implement

- ✅ **Flow 1**: Button Press → Supabase → API `/api/events?type=button_press`
- ✅ **Flow 2**: PIR HIGH ALERT (3 burst) → Supabase → API `/api/events?type=pir_motion`  
- ✅ **Flow 2.1**: PIR Normal Motion → Supabase (direct fetch)

## Troubleshooting

### Lỗi 404 khi gọi API
→ Xem [FIX_404_ERROR.md](backend_nodered/FIX_404_ERROR.md)

### Tab "Nhật ký" không có dữ liệu
→ Kiểm tra file `.env` có đúng Supabase credentials

### MQTT connection error
→ Không ảnh hưởng API, chỉ ảnh hưởng realtime MQTT subscriptions
