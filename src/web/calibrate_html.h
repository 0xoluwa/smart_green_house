/**
 * @file calibrate_html.h
 * @brief Embedded calibration page served at GET /calibrate.
 *
 * Connects to the same WebSocket as the dashboard (/ws).
 * Reads rawSoilAdc, calDry, calWet from the live JSON broadcast.
 *
 * User workflow:
 *   1. Place probe in completely dry air → click "SET DRY"
 *   2. Submerge probe fully in water  → click "SET WET"
 *   3. Values are saved to NVS instantly – no reboot needed.
 */

#pragma once
#include <pgmspace.h>

static const char CALIBRATE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Soil Calibration</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  :root {
    --bg:     #0f172a; --card:  #1e293b; --border: #334155;
    --accent: #22c55e; --blue:  #38bdf8; --warn:   #f59e0b;
    --danger: #ef4444; --text:  #e2e8f0; --muted:  #94a3b8;
    --r: 12px;
  }
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
         background: var(--bg); color: var(--text); min-height: 100vh; padding: 1rem; }
  .container { max-width: 560px; margin: 0 auto; }

  header { display: flex; align-items: center; justify-content: space-between;
           margin-bottom: 1.5rem; flex-wrap: wrap; gap: .5rem; }
  header h1 { font-size: 1.4rem; color: var(--accent); }
  .back { font-size: .85rem; color: var(--muted); text-decoration: none;
          padding: .4rem .9rem; border: 1px solid var(--border);
          border-radius: 8px; transition: color .2s; }
  .back:hover { color: var(--text); }

  .pill { display: inline-flex; align-items: center; gap: .4rem; padding: .3rem .8rem;
          border-radius: 999px; font-size: .8rem; font-weight: 600;
          background: var(--card); border: 1px solid var(--border); }
  .dot  { width: 8px; height: 8px; border-radius: 50%; background: var(--danger); transition: background .3s; }
  .dot.on { background: var(--accent); }

  .card { background: var(--card); border: 1px solid var(--border);
          border-radius: var(--r); padding: 1.4rem; margin-bottom: 1rem; }
  .card-title { font-size: .72rem; font-weight: 700; text-transform: uppercase;
                letter-spacing: .08em; color: var(--muted); margin-bottom: 1rem; }

  /* Live ADC readout */
  .adc-display { text-align: center; padding: 1.2rem 0 1rem; }
  .adc-value   { font-size: 3.5rem; font-weight: 800; color: var(--accent);
                 letter-spacing: -.02em; line-height: 1; }
  .adc-label   { font-size: .8rem; color: var(--muted); margin-top: .3rem; }
  .moisture    { font-size: 1.1rem; color: var(--text); margin-top: .6rem; font-weight: 600; }

  /* Range bar */
  .bar-wrap { position: relative; height: 18px; background: var(--border);
              border-radius: 9px; overflow: hidden; margin: .8rem 0; }
  .bar-fill { height: 100%; border-radius: 9px;
              background: linear-gradient(90deg, var(--blue), var(--accent));
              transition: width .4s; }
  .bar-labels { display: flex; justify-content: space-between;
                font-size: .72rem; color: var(--muted); margin-top: .3rem; }

  /* Capture buttons */
  .btn-row { display: grid; grid-template-columns: 1fr 1fr; gap: .75rem; margin-top: .8rem; }
  .btn { width: 100%; padding: .9rem; border-radius: 10px; border: none;
         font-size: .95rem; font-weight: 700; cursor: pointer; transition: opacity .2s; }
  .btn:active { opacity: .7; }
  .btn-dry  { background: var(--warn);   color: #0f172a; }
  .btn-wet  { background: var(--blue);   color: #0f172a; }
  .btn-save { background: var(--accent); color: #0f172a; width: 100%;
              padding: .85rem; border-radius: 10px; border: none;
              font-size: 1rem; font-weight: 700; cursor: pointer; margin-top: .75rem; }

  /* Current calibration display */
  .cal-row { display: flex; justify-content: space-between; align-items: center;
             padding: .55rem 0; border-bottom: 1px solid var(--border); }
  .cal-row:last-child { border-bottom: none; }
  .cal-label { font-size: .85rem; color: var(--muted); }
  .cal-val   { font-size: 1rem; font-weight: 700; }

  /* Manual inputs */
  .input-row { display: grid; grid-template-columns: 1fr 1fr; gap: .75rem; margin-top: .2rem; }
  label.field-label { font-size: .78rem; color: var(--muted); display: block; margin-bottom: .3rem; }
  input[type=number] { width: 100%; padding: .65rem .8rem; border: 1px solid var(--border);
                       border-radius: 8px; background: var(--bg); color: var(--text);
                       font-size: 1rem; }
  input[type=number]:focus { outline: none; border-color: var(--accent); }

  .status-msg { text-align: center; font-size: .82rem; color: var(--accent);
                margin-top: .6rem; min-height: 1.2em; }
</style>
</head>
<body>
<div class="container">

  <header>
    <h1>🌱 Soil Calibration</h1>
    <div style="display:flex;gap:.6rem;align-items:center">
      <div class="pill"><div class="dot" id="dot"></div><span id="connText">Connecting…</span></div>
      <a class="back" href="/">← Dashboard</a>
    </div>
  </header>

  <!-- Live reading -->
  <div class="card">
    <div class="card-title">Live Sensor Reading</div>
    <div class="adc-display">
      <div class="adc-value" id="adcVal">—</div>
      <div class="adc-label">Raw ADC (0 – 4095)</div>
      <div class="moisture" id="moistVal">Moisture: —</div>
    </div>
    <!-- Visual position bar between dry and wet -->
    <div class="bar-wrap"><div class="bar-fill" id="barFill" style="width:0%"></div></div>
    <div class="bar-labels">
      <span>Wet <span id="barWet">—</span></span>
      <span>Dry <span id="barDry">—</span></span>
    </div>
  </div>

  <!-- Capture buttons -->
  <div class="card">
    <div class="card-title">Quick Calibration</div>
    <p style="font-size:.85rem;color:var(--muted);margin-bottom:.8rem">
      Place the probe in <strong style="color:var(--warn)">dry air</strong> then press SET DRY.
      Submerge fully in <strong style="color:var(--blue)">water</strong> then press SET WET.
      Values are saved instantly.
    </p>
    <div class="btn-row">
      <button class="btn btn-dry" onclick="captureDry()">☀️ SET DRY</button>
      <button class="btn btn-wet" onclick="captureWet()">💧 SET WET</button>
    </div>
    <div class="status-msg" id="captureStatus"></div>
  </div>

  <!-- Current calibration + manual override -->
  <div class="card">
    <div class="card-title">Current Calibration</div>
    <div class="cal-row">
      <span class="cal-label">Dry reference (ADC)</span>
      <span class="cal-val" id="curDry">—</span>
    </div>
    <div class="cal-row">
      <span class="cal-label">Wet reference (ADC)</span>
      <span class="cal-val" id="curWet">—</span>
    </div>

    <div style="margin-top:1rem">
      <div class="card-title" style="margin-bottom:.6rem">Manual Override</div>
      <div class="input-row">
        <div>
          <label class="field-label">Dry ADC value</label>
          <input type="number" id="inputDry" min="0" max="4095" placeholder="e.g. 2410">
        </div>
        <div>
          <label class="field-label">Wet ADC value</label>
          <input type="number" id="inputWet" min="0" max="4095" placeholder="e.g. 580">
        </div>
      </div>
      <button class="btn-save" onclick="saveManual()">Save Manual Values</button>
      <div class="status-msg" id="manualStatus"></div>
    </div>
  </div>

</div><!-- /container -->
<script>
let ws, reconnTimer;
let latestRaw = 0, latestDry = 0, latestWet = 0;

function send(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

function captureDry() {
  send({ type: 'capture_dry' });
  flash('captureStatus', '☀️ Dry value captured (' + latestRaw + ')', 'var(--warn)');
}

function captureWet() {
  send({ type: 'capture_wet' });
  flash('captureStatus', '💧 Wet value captured (' + latestRaw + ')', 'var(--blue)');
}

function saveManual() {
  const dry = parseInt(document.getElementById('inputDry').value);
  const wet = parseInt(document.getElementById('inputWet').value);
  if (isNaN(dry) || isNaN(wet) || dry <= wet) {
    flash('manualStatus', '⚠ Dry must be > Wet and both must be valid numbers', 'var(--danger)');
    return;
  }
  send({ type: 'set_soil_cal', dry, wet });
  flash('manualStatus', '✓ Saved – dry=' + dry + '  wet=' + wet, 'var(--accent)');
}

function flash(id, msg, color) {
  const el = document.getElementById(id);
  el.textContent = msg;
  el.style.color = color;
  setTimeout(() => { el.textContent = ''; }, 4000);
}

function update(d) {
  latestRaw = d.rawSoilAdc ?? 0;
  latestDry = d.calDry ?? 0;
  latestWet = d.calWet ?? 0;

  document.getElementById('adcVal').textContent = latestRaw;

  const moist = (d.soilMoisture > -900 && d.soilMoisture != null)
    ? d.soilMoisture.toFixed(1) + ' %' : '—';
  document.getElementById('moistVal').textContent = 'Moisture: ' + moist;

  document.getElementById('curDry').textContent = latestDry;
  document.getElementById('curWet').textContent = latestWet;
  document.getElementById('barDry').textContent = latestDry;
  document.getElementById('barWet').textContent = latestWet;

  // Position within bar (wet=left=0%, dry=right=100%)
  if (latestDry > latestWet && latestDry > 0) {
    const pct = Math.max(0, Math.min(100,
      (latestDry - latestRaw) / (latestDry - latestWet) * 100));
    document.getElementById('barFill').style.width = pct.toFixed(1) + '%';
  }

  // Pre-fill manual inputs if empty
  const di = document.getElementById('inputDry');
  const wi = document.getElementById('inputWet');
  if (!di.value) di.value = latestDry;
  if (!wi.value) wi.value = latestWet;
}

function connect() {
  ws = new WebSocket('ws://' + location.hostname + '/ws');
  ws.onopen = () => {
    document.getElementById('dot').classList.add('on');
    document.getElementById('connText').textContent = 'Live';
    clearTimeout(reconnTimer);
  };
  ws.onmessage = ({ data }) => {
    let msg; try { msg = JSON.parse(data); } catch { return; }
    if (msg.type === 'live') update(msg);
  };
  ws.onclose = ws.onerror = () => {
    document.getElementById('dot').classList.remove('on');
    document.getElementById('connText').textContent = 'Reconnecting…';
    reconnTimer = setTimeout(connect, 3000);
  };
}
connect();
</script>
</body>
</html>
)rawhtml";
