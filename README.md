# RFID Door Access Log Dashboard

A lightweight web dashboard that displays recent RFID door-access events from a Supabase `door_logs` table. The page refreshes automatically and shows each event’s time, name, UID, access status, and door state.

## Project files

- `index.html` — dashboard page and log table.
- `style.css` — layout, colors, status badges, and mobile styling.
- `app.js` — fetches and renders log entries from Supabase.

## Requirements

- A Supabase project with a `door_logs` table.
- A static web server or hosting service.
- A Supabase URL and a key suitable for client-side access.

The dashboard expects these table fields:

| Field | Purpose |
| --- | --- |
| `created_at` | Event timestamp; used to order the newest entries first |
| `name` | Person’s name; optional |
| `uid` | RFID card or tag identifier |
| `status` | Access result, such as `granted` or `denied` |
| `door_state` | Door state; optional |

## Setup

1. Configure the Supabase URL and client-side key in `app.js`.
2. Ensure the table and fields listed above exist.
3. Configure Supabase access policies so the dashboard can read the required rows.
4. Serve the project folder with a static web server, or deploy it to a static hosting provider.
5. Open the hosted page in a browser.

The dashboard requests the latest 50 records and refreshes every 3 seconds. On narrow screens, the UID column is hidden.

## Security

Never put a Supabase `service_role` key in browser code. Use a client-side key with appropriately restrictive Row Level Security policies. If access logs should not be publicly readable, add user authentication or put the database request behind a trusted backend.

## Scope

This project is the log-viewing dashboard. It does not include RFID reader firmware or code that controls the door hardware.