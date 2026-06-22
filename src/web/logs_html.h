/**
 * @file logs_html.h
 * @brief Embedded log download page served at GET /logs.
 *
 * Lists available daily CSV log files stored in LittleFS with download links.
 * Shows storage usage bar and warnings when storage is getting full.
 * Fetches /api/logs to get the file list and storage info.
 */

#pragma once
#include <pgmspace.h>

static const char LOGS_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Log Download – Smart Greenhouse</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  :root {
    --bg:     #0f172a;
    --card:   #1e293b;
    --border: #334155;
    --accent: #22c55e;
    --warn:   #f59e0b;
    --danger: #ef4444;
    --text:   #e2e8f0;
    --muted:  #94a3b8;
    --r:      12px;
  }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: var(--bg); color: var(--text);
    min-height: 100vh; padding: 1rem;
  }
  .container { max-width: 800px; margin: 0 auto; }

  /* ── Header ──────────────────────────────────────────────────────────────── */
  header {
    display: flex; align-items: center; gap: 1rem;
    margin-bottom: 1.5rem; flex-wrap: wrap;
  }
  .back-link {
    color: var(--accent); text-decoration: none;
    font-size: .9rem; font-weight: 600;
    display: inline-flex; align-items: center; gap: .3rem;
    padding: .35rem .75rem; border-radius: 6px;
    border: 1px solid var(--border); background: var(--card);
    transition: background .2s;
  }
  .back-link:hover { background: var(--border); }
  header h1 { font-size: 1.4rem; color: var(--accent); }

  /* ── Cards ───────────────────────────────────────────────────────────────── */
  .card {
    background: var(--card); border: 1px solid var(--border);
    border-radius: var(--r); padding: 1.2rem 1.4rem; margin-bottom: 1rem;
  }
  .card-title {
    font-size: .75rem; font-weight: 700; text-transform: uppercase;
    letter-spacing: .08em; color: var(--muted); margin-bottom: 1rem;
  }

  /* ── Warning banner ──────────────────────────────────────────────────────── */
  .warn-banner {
    display: none;
    background: rgba(245,158,11,.13);
    border: 1px solid var(--warn);
    border-radius: var(--r);
    padding: .8rem 1rem;
    margin-bottom: 1rem;
    font-size: .88rem;
    color: var(--warn);
  }
  .warn-banner a { color: var(--warn); font-weight: 700; }

  /* ── Storage bar ─────────────────────────────────────────────────────────── */
  .storage-row {
    display: flex; justify-content: space-between; align-items: center;
    margin-bottom: .6rem; font-size: .85rem;
  }
  .storage-label { color: var(--muted); }
  .storage-value { font-weight: 700; color: var(--text); }
  .progress-track {
    width: 100%; height: 10px; border-radius: 999px;
    background: var(--border); overflow: hidden; margin-bottom: .5rem;
  }
  .progress-fill {
    height: 100%; border-radius: 999px;
    background: var(--accent);
    transition: width .4s ease, background .4s ease;
  }
  .storage-note { font-size: .78rem; color: var(--muted); }

  /* ── Log table ───────────────────────────────────────────────────────────── */
  table { width: 100%; border-collapse: collapse; }
  thead th {
    text-align: left; padding: .55rem .7rem;
    font-size: .75rem; font-weight: 700; text-transform: uppercase;
    letter-spacing: .06em; color: var(--muted);
    border-bottom: 1px solid var(--border);
  }
  tbody tr { border-bottom: 1px solid var(--border); }
  tbody tr:last-child { border-bottom: none; }
  tbody td { padding: .7rem .7rem; font-size: .9rem; }
  tbody td.date-cell { font-family: monospace; font-size: .95rem; }
  tbody td.size-cell { color: var(--muted); font-size: .85rem; }

  .dl-btn {
    padding: .35rem .8rem; border-radius: 6px;
    border: 1px solid var(--accent); background: transparent;
    color: var(--accent); font-size: .82rem; font-weight: 600;
    cursor: pointer; text-decoration: none;
    display: inline-block;
    transition: background .2s, color .2s;
  }
  .dl-btn:hover { background: var(--accent); color: #0f172a; }

  /* ── Empty / loading states ──────────────────────────────────────────────── */
  .empty-state {
    text-align: center; padding: 2.5rem 1rem; color: var(--muted); font-size: .9rem;
  }
  .info-note {
    font-size: .8rem; color: var(--muted);
    padding: .7rem 1rem;
    background: rgba(148,163,184,.07);
    border-radius: 6px; margin-top: 1rem;
    border-left: 3px solid var(--border);
  }
</style>
</head>
<body>
<div class="container">

  <!-- Header -->
  <header>
    <a class="back-link" href="/">&#8592; Dashboard</a>
    <h1>Log Download</h1>
  </header>

  <!-- Near-full warning banner -->
  <div class="warn-banner" id="warnBanner">
    &#9888; Log storage is getting full. Download and delete old logs to free space.
  </div>

  <!-- Storage usage card -->
  <div class="card">
    <div class="card-title">Storage Usage</div>
    <div class="storage-row">
      <span class="storage-label">LittleFS used</span>
      <span class="storage-value" id="storageText">Loading…</span>
    </div>
    <div class="progress-track">
      <div class="progress-fill" id="progressFill" style="width:0%"></div>
    </div>
    <div class="storage-note" id="storageNote">
      One CSV file per day. Oldest log is auto-removed when storage exceeds 90 %.
    </div>
  </div>

  <!-- Log list card -->
  <div class="card">
    <div class="card-title">Daily Logs (UTC)</div>
    <table>
      <thead>
        <tr>
          <th>Date (UTC)</th>
          <th>Size</th>
          <th>Download</th>
        </tr>
      </thead>
      <tbody id="logTableBody">
        <tr><td colspan="3" class="empty-state">Loading…</td></tr>
      </tbody>
    </table>
    <div class="info-note">
      Timestamps in the CSV files are in UTC (ISO 8601 format).
      Each row contains: timestamp, temperature (&deg;C), humidity (%), light (lux), soil moisture (%).
    </div>
  </div>

</div><!-- /container -->
<script>
function fmtBytes(b) {
  if (b < 1024)         return b + ' B';
  if (b < 1048576)      return (b / 1024).toFixed(1) + ' KB';
  return (b / 1048576).toFixed(2) + ' MB';
}

function loadLogs() {
  fetch('/api/logs')
    .then(r => r.json())
    .then(data => {
      const used  = data.usedBytes  || 0;
      const total = data.totalBytes || 0;

      // Not mounted — partition table mismatch or LittleFS init failure
      if (total === 0) {
        document.getElementById('storageText').textContent = 'LittleFS not mounted';
        document.getElementById('storageNote').textContent =
          'Run: pio run --target erase  then  pio run --target upload';
        document.getElementById('logTableBody').innerHTML =
          '<tr><td colspan="3" class="empty-state">Flash filesystem unavailable. ' +
          'A full erase + re-flash is required to apply the updated partition table.</td></tr>';
        return;
      }

      const pct = Math.round(used * 100 / total);

      // Storage bar
      document.getElementById('storageText').textContent =
        fmtBytes(used) + ' / ' + fmtBytes(total) + ' (' + pct + '%)';

      const fill = document.getElementById('progressFill');
      fill.style.width = pct + '%';
      if (pct >= 90)      fill.style.background = 'var(--danger)';
      else if (pct >= 75) fill.style.background = 'var(--warn)';
      else                fill.style.background = 'var(--accent)';

      // Warning banner
      if (data.nearFull) {
        document.getElementById('warnBanner').style.display = 'block';
      }

      // Table
      const tbody = document.getElementById('logTableBody');
      if (!data.logs || data.logs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3" class="empty-state">No log files yet. Logs are created once NTP time is synced.</td></tr>';
        return;
      }

      tbody.innerHTML = data.logs.map(entry => {
        const dlUrl = '/api/logs/download?date=' + encodeURIComponent(entry.date);
        return '<tr>' +
          '<td class="date-cell">' + entry.date + '</td>' +
          '<td class="size-cell">' + fmtBytes(entry.size) + '</td>' +
          '<td><a class="dl-btn" href="' + dlUrl + '" download="greenhouse-' + entry.date + '.csv">Download</a></td>' +
          '</tr>';
      }).join('');
    })
    .catch(err => {
      document.getElementById('storageText').textContent = 'Error loading';
      document.getElementById('logTableBody').innerHTML =
        '<tr><td colspan="3" class="empty-state">Failed to load log list: ' + err.message + '</td></tr>';
    });
}

loadLogs();
</script>
</body>
</html>
)rawhtml";
