# Frontend React - Cấu hình

## Bước 1: Cài đặt dependencies

```bash
npm install
```

## Bước 2: Cấu hình Supabase

Tạo file `.env` trong thư mục `frontend_react/`:

```bash
VITE_SUPABASE_URL=https://xznnnklhqkccylxxzsdh.supabase.co
VITE_SUPABASE_ANON_KEY=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8
```

## Bước 3: Chạy development server

```bash
npm run dev
```

Ứng dụng sẽ chạy tại: http://localhost:5173

## Tab "Nhật ký" - Activity Logs

Tab Nhật ký hiển thị các sự kiện PIR normal motion (Flow 2.1) từ Supabase:

### Dữ liệu hiển thị:
- **Event Type**: `pir_motion` (chuyển động PIR thông thường)
- **Timestamp**: Thời gian phát hiện
- **Level**: Mức độ (normal, medium, high)
- **Details**: Chi tiết sự kiện
- **Auto-refresh**: Tự động làm mới mỗi 10 giây

### Cấu trúc database:
```sql
-- Table: events
{
  id: UUID,
  device_id: TEXT,
  event_type: TEXT, -- 'pir_motion' cho Flow 2.1
  image_url: TEXT,
  metadata: JSONB,
  created_at: TIMESTAMP
}
```

### Filter:
- Lọc theo ngày
- Lọc theo loại sự kiện (tất cả, chuyển động, tin nhắn)

## Các Tab khác:

1. **Tổng quan**: Thống kê và điều khiển
2. **Báo động**: PIR HIGH ALERT với 3 ảnh burst (Flow 2)
3. **Hình ảnh**: Button press images (Flow 1)
4. **Nhật ký**: PIR normal motion logs (Flow 2.1) ✅
5. **Hộp thư thoại**: Voice notes (Flow 3 - future)
