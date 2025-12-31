# Node-RED Flow: Batch Update is_read

## Endpoint: PATCH /api/events/batch-read

### Flow Structure
```
[HTTP In] → [Validate & Prepare] → [HTTP Request to Supabase] → [Response] → [HTTP Response]
```

### Node 1: HTTP In
- **Method**: PATCH
- **URL**: `/api/events/batch-read`
- **Name**: "PATCH /api/events/batch-read"

### Node 2: Function - Validate & Prepare
Name: `Validate & Prepare Batch Update`

```javascript
const payload = msg.payload;

// Validate event_ids array
if (!payload.event_ids || !Array.isArray(payload.event_ids) || payload.event_ids.length === 0) {
    msg.statusCode = 400;
    msg.payload = {
        success: false,
        error: 'Missing or invalid event_ids array'
    };
    return [null, msg]; // Send to error output
}

// Build Supabase RPC call or use filter
const eventIds = payload.event_ids.join(',');

// Option 1: Use Supabase filter with IN clause
msg.url = `https://xznnnklhqkccylxxzsdh.supabase.co/rest/v1/events?id=in.(${eventIds})`;
msg.method = 'PATCH';
msg.payload = {
    is_read: true
};
msg.headers = {
    'apikey': 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Authorization': 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8',
    'Content-Type': 'application/json',
    'Prefer': 'return=representation'
};

// Store original event_ids for response
msg.originalEventIds = payload.event_ids;

return [msg, null]; // Send to success output
```

**Outputs**: 2 (success, error)

### Node 3: HTTP Request
- **Method**: Use msg.method
- **URL**: Use msg.url
- **Return**: a parsed JSON object
- **Name**: "Update Supabase Events"

### Node 4: Function - Format Response
Name: `Format Success Response`

```javascript
const updatedRecords = msg.payload;

msg.payload = {
    success: true,
    message: `Marked ${msg.originalEventIds.length} events as read`,
    updated_ids: msg.originalEventIds,
    updated_count: Array.isArray(updatedRecords) ? updatedRecords.length : 0
};

msg.statusCode = 200;
return msg;
```

### Node 5: HTTP Response
- Connect both error and success outputs here

---

## How to Add in Node-RED

1. Open Node-RED editor: http://127.0.0.1:1880
2. Create new tab or use existing Flow 4
3. Add nodes as described above
4. Deploy
5. Test với:

```bash
curl -X PATCH http://localhost:1880/api/events/batch-read \
  -H "Content-Type: application/json" \
  -d '{"event_ids": [1466, 1467, 1468]}'
```

Expected response:
```json
{
  "success": true,
  "message": "Marked 3 events as read",
  "updated_ids": [1466, 1467, 1468],
  "updated_count": 3
}
```

---

## Alternative: Direct Supabase Query

Nếu muốn dùng Supabase client trong function:

```javascript
// In Node-RED function
const { createClient } = require('@supabase/supabase-js');

const supabase = createClient(
    'https://xznnnklhqkccylxxzsdh.supabase.co',
    'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inh6bm5ua2xocWtjY3lseHh6c2RoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjY5OTEzMDAsImV4cCI6MjA4MjU2NzMwMH0.qucNiuvWiQc-ntPAuBiNKZAphhnReb3e4gnUAhEE1Z8'
);

const eventIds = msg.payload.event_ids;

const { data, error } = await supabase
    .from('events')
    .update({ is_read: true })
    .in('id', eventIds);

if (error) {
    msg.statusCode = 500;
    msg.payload = { success: false, error: error.message };
} else {
    msg.statusCode = 200;
    msg.payload = { success: true, updated: data };
}

return msg;
```

**Note**: Cần install `@supabase/supabase-js` trước:
```bash
cd backend_nodered
npm install @supabase/supabase-js
```
