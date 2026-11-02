const SUPABASE_URL = "https://zxysonquxjsylmnhnpbw.supabase.co";
const SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inp4eXNvbnF1eGpzeWxtbmhucGJ3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTIzODcwNzYsImV4cCI6MjEwNzk2MzA3Nn0.Y1fGhNvaGWqeNrjmsE8BVTAWGr7LiBxUgfT4ARRnur0";
const REFRESH_MS = 3000;

const tbody = document.getElementById("logs");
const statusEl = document.getElementById("status");

async function loadLogs() {
  try {
    const res = await fetch(
      `${SUPABASE_URL}/rest/v1/door_logs?select=*&order=created_at.desc&limit=50`,
      { headers: { apikey: SUPABASE_KEY, Authorization: `Bearer ${SUPABASE_KEY}` } }
    );
    if (!res.ok) throw new Error(res.status);
    const rows = await res.json();
    render(rows);
    statusEl.textContent = "updated " + new Date().toLocaleTimeString();
  } catch (e) {
    statusEl.textContent = "error: " + e.message;
  }
}

function render(rows) {
  tbody.innerHTML = "";
  for (const r of rows) {
    const tr = document.createElement("tr");
    const cells = [
      new Date(r.created_at).toLocaleString(),
      r.name ?? "-",
      r.uid,
      r.status,
      r.door_state ?? "-",
    ];
    cells.forEach((v, i) => {
      const td = document.createElement("td");
      if (i === 2) td.className = "uid";
      if (i === 3) {
        const b = document.createElement("span");
        b.className = "badge " + v;
        b.textContent = v;
        td.appendChild(b);
      } else td.textContent = v;   // textContent avoids HTML injection
      tr.appendChild(td);
    });
    tbody.appendChild(tr);
  }
}

loadLogs();
setInterval(loadLogs, REFRESH_MS);