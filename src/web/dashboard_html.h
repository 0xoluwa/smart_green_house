/**
 * @file dashboard_html.h
 * @brief Embedded HTML dashboard served at GET /.
 *
 * WebSocket protocol (JSON text frames):
 *
 *   Server → Client  (every WEB_BROADCAST_PERIOD_MS)
 *   { "type":"live", "temperature":25.3, "humidity":65.2, "lux":4200.0,
 *     "soilMoisture":42.5, "batteryV":11.8, "batteryPct":75,
 *     "batteryLow":false, "pumpRunning":false, "pumpAuto":true,
 *     "manualOn":false, "threshold":30.0, "timeoutS":60,
 *     "uptimeMs":12345, "rssi":-45 }
 *
 *   Server → Client  (on connect)
 *   { "type":"history",
 *     "data": [{"ts":0,"te":25.1,"hu":65.0,"lx":4200.0,"so":42.0}, …] }
 *
 *   Client → Server  (user actions)
 *   {"type":"set_auto"}  {"type":"set_manual"}
 *   {"type":"pump_on"}   {"type":"pump_off"}
 *   {"type":"set_threshold","value":35.0}
 *   {"type":"set_timeout","value":90}
 */

#pragma once
#include <pgmspace.h>

static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Greenhouse</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.3/dist/chart.umd.min.js"></script>
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
  .container { max-width: 1100px; margin: 0 auto; }

  /* ── Header ──────────────────────────────────────────────────────────────── */
  header {
    display: flex; align-items: center; justify-content: space-between;
    margin-bottom: 1.5rem; flex-wrap: wrap; gap: .5rem;
  }
  header h1 { font-size: 1.5rem; color: var(--accent); }
  .pill {
    display: inline-flex; align-items: center; gap: .4rem;
    padding: .3rem .8rem; border-radius: 999px;
    font-size: .8rem; font-weight: 600;
    background: var(--card); border: 1px solid var(--border);
  }
  .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--danger); transition: background .3s; }
  .dot.on { background: var(--accent); }

  /* ── Sensor tiles ────────────────────────────────────────────────────────── */
  .tiles {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(155px, 1fr));
    gap: .75rem; margin-bottom: 1rem;
  }
  .tile {
    background: var(--card); border: 1px solid var(--border);
    border-radius: var(--r); padding: 1rem 1.2rem;
    display: flex; flex-direction: column; gap: .3rem;
  }
  .tile-label { font-size: .72rem; font-weight: 700; text-transform: uppercase; letter-spacing: .06em; color: var(--muted); }
  .tile-value { font-size: 1.7rem; font-weight: 700; line-height: 1.1; }
  .tile-unit  { font-size: .88rem; color: var(--muted); }
  .tile-sub   { font-size: .78rem; color: var(--muted); margin-top: .2rem; }
  .tile.warn   .tile-value { color: var(--warn); }
  .tile.danger .tile-value { color: var(--danger); }

  /* ── Pump control ────────────────────────────────────────────────────────── */
  .card { background: var(--card); border: 1px solid var(--border); border-radius: var(--r); padding: 1.2rem 1.4rem; margin-bottom: 1rem; }
  .card-title { font-size: .75rem; font-weight: 700; text-transform: uppercase; letter-spacing: .08em; color: var(--muted); margin-bottom: 1rem; }

  .pump-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; align-items: start; }
  @media (max-width: 540px) { .pump-grid { grid-template-columns: 1fr; } }

  .toggle-row { display: flex; align-items: center; gap: .8rem; margin-bottom: .8rem; }
  .toggle-label { font-size: .9rem; color: var(--muted); }
  .mode-btn {
    padding: .45rem 1rem; border-radius: 6px;
    border: 1px solid var(--border); background: var(--bg);
    color: var(--muted); font-size: .85rem; font-weight: 600; cursor: pointer;
    transition: background .2s, color .2s;
  }
  .mode-btn.active { background: var(--accent); color: #0f172a; border-color: var(--accent); }

  .pump-btn {
    width: 100%; padding: .8rem; border-radius: 8px; border: none;
    font-size: 1rem; font-weight: 700; cursor: pointer;
    transition: opacity .2s; margin-top: .4rem;
  }
  .pump-btn.on  { background: var(--accent); color: #0f172a; }
  .pump-btn.off { background: var(--danger);  color: #fff; }
  .pump-btn:disabled { opacity: .35; cursor: not-allowed; }

  .pump-status {
    font-size: .85rem; margin-top: .5rem; padding: .4rem .8rem;
    border-radius: 6px; background: var(--bg); color: var(--muted); text-align: center;
  }
  .pump-status.running { color: var(--accent); }

  .slider-group { margin-top: .6rem; }
  .slider-group label { display: flex; justify-content: space-between; font-size: .82rem; color: var(--muted); margin-bottom: .3rem; }
  .slider-group label span { color: var(--text); font-weight: 600; }
  input[type=range] { width: 100%; accent-color: var(--accent); cursor: pointer; }

  /* ── Charts ──────────────────────────────────────────────────────────────── */
  .chart-wrap { position: relative; height: 200px; }

  /* ── System bar ──────────────────────────────────────────────────────────── */
  .sysbar {
    display: flex; flex-wrap: wrap; gap: .5rem 1.5rem;
    padding: .8rem 1.2rem;
    background: var(--card); border: 1px solid var(--border); border-radius: var(--r);
    font-size: .78rem; color: var(--muted);
  }
  .sysbar strong { color: var(--text); }
</style>
</head>
<body>
<div class="container">

  <!-- Header -->
  <header>
    <h1>🌿 Smart Greenhouse</h1>
    <div style="display:flex;gap:.5rem;flex-wrap:wrap">
      <div class="pill">
        <div class="dot" id="statusDot"></div>
        <span id="statusText">Connecting…</span>
      </div>
      <div class="pill">
        <div class="dot" id="ntpDot"></div>
        <span id="ntpText">Time sync…</span>
      </div>
    </div>
  </header>

  <!-- Storage warning (hidden until logWarn=true) -->
  <div id="logWarnBanner" style="display:none;background:rgba(245,158,11,.12);border:1px solid #f59e0b;border-radius:var(--r);padding:.7rem 1rem;margin-bottom:1rem;font-size:.84rem;color:#f59e0b;">
    ⚠ Log storage is getting full. <a href="/logs" style="color:#f59e0b;font-weight:700">Download logs now</a> before the oldest data is removed.
  </div>

  <!-- Sensor tiles -->
  <div class="tiles">
    <div class="tile" id="tileTemp">
      <div class="tile-label">Temperature</div>
      <div class="tile-value" id="valTemp">—<span class="tile-unit"> °C</span></div>
    </div>
    <div class="tile" id="tileHum">
      <div class="tile-label">Humidity</div>
      <div class="tile-value" id="valHum">—<span class="tile-unit"> %</span></div>
    </div>
    <div class="tile" id="tileLight">
      <div class="tile-label">Light</div>
      <div class="tile-value" id="valLight">—<span class="tile-unit"> lx</span></div>
      <div class="tile-sub" id="valLightDesc"></div>
    </div>
    <div class="tile" id="tileSoil">
      <div class="tile-label">Soil Moisture</div>
      <div class="tile-value" id="valSoil">—<span class="tile-unit"> %</span></div>
    </div>
    <div class="tile" id="tileBatt">
      <div class="tile-label">Battery</div>
      <div class="tile-value" id="valBatt">—<span class="tile-unit"> %</span></div>
      <div class="tile-sub" id="valBattV"></div>
    </div>
  </div>

  <!-- Pump control -->
  <div class="card">
    <div class="card-title">Pump Control</div>
    <div class="pump-grid">
      <div>
        <div class="toggle-row">
          <span class="toggle-label">Mode:</span>
          <button class="mode-btn" id="btnAuto"   onclick="setMode('auto')">Auto</button>
          <button class="mode-btn" id="btnManual" onclick="setMode('manual')">Manual</button>
        </div>
        <button class="pump-btn on"  id="btnPumpOn"  onclick="pumpCmd('pump_on')"  disabled>Pump ON</button>
        <button class="pump-btn off" id="btnPumpOff" onclick="pumpCmd('pump_off')" disabled>Pump OFF</button>
        <div class="pump-status" id="pumpStatus">—</div>
      </div>
      <div>
        <div class="slider-group">
          <label>Dry threshold <span id="threshVal">30</span> %</label>
          <input type="range" id="sliderThresh" min="5" max="95" value="30"
                 oninput="onThreshInput(this.value)" onchange="sendThreshold(this.value)">
        </div>
        <div class="slider-group" style="margin-top:1rem">
          <label>Max run time <span id="timeoutVal">60</span> s</label>
          <input type="range" id="sliderTimeout" min="5" max="300" step="5" value="60"
                 oninput="onTimeoutInput(this.value)" onchange="sendTimeout(this.value)">
        </div>
      </div>
    </div>
  </div>

  <!-- Charts -->
  <div class="card">
    <div class="card-title">Temperature &amp; Humidity — last 24 h</div>
    <div class="chart-wrap"><canvas id="chartTH"></canvas></div>
  </div>
  <div class="card">
    <div class="card-title">Light Level — last 24 h</div>
    <div class="chart-wrap"><canvas id="chartLight"></canvas></div>
  </div>
  <div class="card">
    <div class="card-title">Soil Moisture — last 24 h</div>
    <div class="chart-wrap"><canvas id="chartSoil"></canvas></div>
  </div>

  <!-- System info -->
  <div class="sysbar">
    <span>Uptime: <strong id="sysUptime">—</strong></span>
    <span>RSSI: <strong id="sysRssi">—</strong> dBm</span>
    <span>IP: <strong id="sysIP">—</strong></span>
    <span>Sensors: DHT22 + BH1750</span>
    <span><a href="/calibrate" style="color:var(--accent);text-decoration:none">&#127807; Soil Calibration &#8594;</a></span>
    <span><a href="/settings"  style="color:var(--accent);text-decoration:none">&#9881; Settings &#8594;</a></span>
    <span><a href="/logs"      style="color:var(--accent);text-decoration:none">&#128190; Log Download &#8594;</a></span>
  </div>

</div><!-- /container -->
<script>
// ── Utilities ─────────────────────────────────────────────────────────────────
function fmt(v, d) {
  return (v === null || v === undefined || v < -900) ? '—' : Number(v).toFixed(d ?? 1);
}
function fmtUptime(ms) {
  let s = Math.floor(ms / 1000);
  const h = Math.floor(s / 3600); s %= 3600;
  const m = Math.floor(s / 60);   s %= 60;
  return `${h}h ${m}m ${s}s`;
}
const MIN_REAL_EPOCH_MS = 1577836800000; // Jan 1 2020 – anything below means "no NTP sync"
function tsLabel(ms) {
  if (!ms || ms < MIN_REAL_EPOCH_MS) return '--:--';
  const d = new Date(ms);  // browser shows in local timezone automatically
  return d.getHours().toString().padStart(2,'0') + ':' +
         d.getMinutes().toString().padStart(2,'0');
}
// Convert lux to a human-readable description
function luxDesc(lux) {
  if (lux < 0)     return '';
  if (lux < 50)    return 'Dark';
  if (lux < 500)   return 'Dim';
  if (lux < 2000)  return 'Indoor light';
  if (lux < 10000) return 'Overcast';
  return 'Full sunlight';
}

// ── Gap detection for charts ───────────────────────────────────────────────────
// epochTs[i] mirrors the chart label array; holds raw epoch ms for each point.
let epochTs = [];
const GAP_MS = 5 * 60 * 1000; // 5 min – more than 2× the 1-min sample interval

function isGap(ctx) {
  const i0 = ctx.p0DataIndex, i1 = ctx.p1DataIndex;
  if (i0 < 0 || i1 >= epochTs.length) return false;
  const t0 = epochTs[i0], t1 = epochTs[i1];
  return (t0 && t1 && t1 - t0 > GAP_MS);
}

function gapSegment(normalColor) {
  return {
    borderColor:     ctx => isGap(ctx) ? 'rgba(100,116,139,0.45)' : normalColor,
    borderDash:      ctx => isGap(ctx) ? [5, 4] : undefined,
    borderWidth:     ctx => isGap(ctx) ? 1 : undefined,
    backgroundColor: ctx => isGap(ctx) ? 'rgba(0,0,0,0)' : undefined,
  };
}

// ── Chart setup ───────────────────────────────────────────────────────────────
const cDefaults = {
  responsive: true, maintainAspectRatio: false, animation: false,
  plugins: { legend: { labels: { color: '#94a3b8', boxWidth: 12 } } },
  scales: {
    x: { ticks: { color: '#64748b', maxTicksLimit: 8 }, grid: { color: '#1e293b' } },
    y: { ticks: { color: '#64748b' },                   grid: { color: '#334155' } }
  }
};
function makeChart(id, datasets) {
  return new Chart(document.getElementById(id), { type: 'line', data: { labels: [], datasets }, options: cDefaults });
}

const chartTH = makeChart('chartTH', [
  { label: 'Temperature (°C)', data: [], borderColor: '#f97316', backgroundColor: 'rgba(249,115,22,.1)',  fill: true, tension: .4, pointRadius: 2, spanGaps: true, segment: gapSegment('#f97316') },
  { label: 'Humidity (%)',     data: [], borderColor: '#38bdf8', backgroundColor: 'rgba(56,189,248,.08)', fill: true, tension: .4, pointRadius: 2, spanGaps: true, segment: gapSegment('#38bdf8') }
]);
const chartLight = makeChart('chartLight', [
  { label: 'Light (lux)', data: [], borderColor: '#fbbf24', backgroundColor: 'rgba(251,191,36,.12)', fill: true, tension: .4, pointRadius: 2, spanGaps: true, segment: gapSegment('#fbbf24') }
]);
const chartSoil = makeChart('chartSoil', [
  { label: 'Soil Moisture (%)', data: [], borderColor: '#22c55e', backgroundColor: 'rgba(34,197,94,.12)', fill: true, tension: .4, pointRadius: 2, spanGaps: true, segment: gapSegment('#22c55e') }
]);

function pushHistory(samples) {
  epochTs = samples.map(s => s.ts || 0);
  const labs = samples.map(s => tsLabel(s.ts));
  chartTH.data.labels = labs;
  chartTH.data.datasets[0].data = samples.map(s => s.te < -900 ? null : s.te);
  chartTH.data.datasets[1].data = samples.map(s => s.hu < -900 ? null : s.hu);
  chartTH.update('none');

  chartLight.data.labels = labs;
  chartLight.data.datasets[0].data = samples.map(s => s.lx < -900 ? null : s.lx);
  chartLight.update('none');

  chartSoil.data.labels = labs;
  chartSoil.data.datasets[0].data = samples.map(s => s.so < -900 ? null : s.so);
  chartSoil.update('none');
}

// ── Pump controls ─────────────────────────────────────────────────────────────
function setMode(m) { send({ type: m === 'auto' ? 'set_auto' : 'set_manual' }); }
function pumpCmd(c) { send({ type: c }); }
function onThreshInput(v)  { document.getElementById('threshVal').textContent = v; }
function onTimeoutInput(v) { document.getElementById('timeoutVal').textContent = v; }
function sendThreshold(v)  { send({ type: 'set_threshold', value: parseFloat(v) }); }
function sendTimeout(v)    { send({ type: 'set_timeout',   value: parseFloat(v) }); }

function updateControls(d) {
  document.getElementById('btnAuto').classList.toggle('active',  d.pumpAuto);
  document.getElementById('btnManual').classList.toggle('active', !d.pumpAuto);

  const onBtn  = document.getElementById('btnPumpOn');
  const offBtn = document.getElementById('btnPumpOff');
  onBtn.disabled  = d.pumpAuto || d.pumpRunning;
  offBtn.disabled = d.pumpAuto || !d.pumpRunning;

  const st = document.getElementById('pumpStatus');
  if (d.pumpRunning) { st.textContent = '● Pump is ON'; st.className = 'pump-status running'; }
  else               { st.textContent = d.pumpAuto ? 'Auto mode – monitoring soil' : 'Pump is OFF'; st.className = 'pump-status'; }

  const ts = document.getElementById('sliderThresh');
  if (!ts.matches(':active')) { ts.value = d.threshold; document.getElementById('threshVal').textContent = d.threshold.toFixed(0); }
  const to = document.getElementById('sliderTimeout');
  if (!to.matches(':active')) { to.value = d.timeoutS;  document.getElementById('timeoutVal').textContent = d.timeoutS; }
}

// ── Live data update ──────────────────────────────────────────────────────────
function updateLive(d) {
  document.getElementById('valTemp').innerHTML  = fmt(d.temperature)  + '<span class="tile-unit"> °C</span>';
  document.getElementById('valHum').innerHTML   = fmt(d.humidity)     + '<span class="tile-unit"> %</span>';

  // Light tile
  const lv = d.lux;
  document.getElementById('valLight').innerHTML = fmt(lv, 0) + '<span class="tile-unit"> lx</span>';
  document.getElementById('valLightDesc').textContent = (lv > -900) ? luxDesc(lv) : '';

  document.getElementById('valSoil').innerHTML  = fmt(d.soilMoisture) + '<span class="tile-unit"> %</span>';

  // Soil colour
  const soilTile = document.getElementById('tileSoil');
  soilTile.className = 'tile' +
    (d.soilMoisture < d.threshold * 0.8 ? ' danger' :
     d.soilMoisture < d.threshold       ? ' warn'   : '');

  // Battery
  document.getElementById('valBatt').innerHTML  = d.batteryPct + '<span class="tile-unit"> %</span>';
  document.getElementById('valBattV').textContent = fmt(d.batteryV, 2) + ' V';
  const battTile = document.getElementById('tileBatt');
  battTile.className = 'tile' + (d.batteryLow ? ' danger' : d.batteryPct < 30 ? ' warn' : '');

  // System bar
  document.getElementById('sysUptime').textContent = fmtUptime(d.uptimeMs);
  document.getElementById('sysRssi').textContent   = d.rssi;
  if (d.ip) document.getElementById('sysIP').textContent = d.ip;

  updateControls(d);

  // NTP indicator
  document.getElementById('ntpDot').classList.toggle('on', !!d.ntpOk);
  document.getElementById('ntpText').textContent = d.ntpOk ? 'NTP synced' : 'No time sync';

  // Storage warning banner
  document.getElementById('logWarnBanner').style.display = d.logWarn ? 'block' : 'none';
}

// ── WebSocket ─────────────────────────────────────────────────────────────────
let ws, reconnTimer;
function send(obj) { if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj)); }

function connect() {
  ws = new WebSocket('ws://' + location.hostname + '/ws');
  ws.onopen = () => {
    document.getElementById('statusDot').classList.add('on');
    document.getElementById('statusText').textContent = 'Connected';
    clearTimeout(reconnTimer);
  };
  ws.onmessage = ({ data }) => {
    let msg; try { msg = JSON.parse(data); } catch { return; }
    if      (msg.type === 'live')    updateLive(msg);
    else if (msg.type === 'history') pushHistory(msg.data);
  };
  ws.onclose = ws.onerror = () => {
    document.getElementById('statusDot').classList.remove('on');
    document.getElementById('statusText').textContent = 'Reconnecting…';
    reconnTimer = setTimeout(connect, 3000);
  };
}
connect();
</script>
</body>
</html>
)rawhtml";
