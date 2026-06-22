/**
 * @file settings_html.h
 * @brief Embedded HTML settings page served at GET /settings.
 *
 * Provides two sections:
 *   - AP Hotspot: rename the provisioning access point and set a password.
 *   - WiFi Network: change the router SSID/password (triggers restart).
 *
 * REST API consumed by this page:
 *   GET  /api/settings          → { "apSsid":"...", "wifiSsid":"..." }
 *   POST /api/settings/ap       → body: ssid=...&pass=...
 *   POST /api/settings/wifi     → body: ssid=...&pass=...  (device restarts)
 */

#pragma once
#include <pgmspace.h>

static const char SETTINGS_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings — Greenhouse</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  :root {
    --bg:#0f172a; --card:#1e293b; --border:#334155;
    --accent:#22c55e; --danger:#ef4444;
    --text:#e2e8f0; --muted:#94a3b8; --r:12px;
  }
  body { font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
         background:var(--bg);color:var(--text);min-height:100vh;padding:1rem; }
  .container { max-width:520px; margin:0 auto; }
  header { display:flex;align-items:center;gap:1rem;margin-bottom:1.5rem; }
  header h1 { font-size:1.4rem;color:var(--accent); }
  .back { color:var(--muted);text-decoration:none;font-size:.9rem; }
  .back:hover { color:var(--text); }
  .card { background:var(--card);border:1px solid var(--border);
          border-radius:var(--r);padding:1.4rem;margin-bottom:1rem; }
  .card-title { font-size:.75rem;font-weight:700;text-transform:uppercase;
                letter-spacing:.08em;color:var(--muted);margin-bottom:1.2rem; }
  label { display:block;font-size:.82rem;color:var(--muted);
          margin-bottom:.3rem;margin-top:.9rem; }
  label:first-of-type { margin-top:0; }
  label small { color:#64748b;font-weight:400; }
  input[type=text], input[type=password] {
    width:100%;padding:.65rem .9rem;
    border:1px solid var(--border);border-radius:8px;
    background:var(--bg);color:var(--text);font-size:.95rem;
  }
  input:focus { outline:none;border-color:var(--accent); }
  .btn { margin-top:1.2rem;width:100%;padding:.75rem;border:none;
         border-radius:8px;font-size:.95rem;font-weight:700;cursor:pointer; }
  .btn-green { background:var(--accent);color:#0f172a; }
  .btn-red   { background:var(--danger);color:#fff; }
  .btn:disabled { opacity:.4;cursor:not-allowed; }
  .msg { margin-top:.8rem;padding:.5rem .9rem;border-radius:6px;
         font-size:.82rem;display:none; }
  .msg.ok  { background:rgba(34,197,94,.15);color:var(--accent);display:block; }
  .msg.err { background:rgba(239,68,68,.15);color:var(--danger);display:block; }
  .note { font-size:.76rem;color:var(--muted);margin-top:.65rem;line-height:1.5; }
</style>
</head>
<body>
<div class="container">

  <header>
    <a class="back" href="/">&#8592; Dashboard</a>
    <h1>&#9881; Settings</h1>
  </header>

  <!-- AP Hotspot -->
  <div class="card">
    <div class="card-title">AP Hotspot</div>
    <form id="apForm">
      <label>Hotspot Name (SSID)</label>
      <input type="text"     id="apSsid" name="ssid" maxlength="31" placeholder="GreenHouse_Setup" required>
      <label>Password <small>(min 8 chars &mdash; leave blank for open network)</small></label>
      <input type="password" id="apPass" name="pass" maxlength="63" placeholder="Leave blank for open network">
      <button type="submit" class="btn btn-green">Save Hotspot Settings</button>
    </form>
    <div class="msg" id="apMsg"></div>
    <p class="note">
      Takes effect the next time the device enters setup mode
      (when it cannot find a saved WiFi network on boot).
    </p>
  </div>

  <!-- WiFi Network -->
  <div class="card">
    <div class="card-title">WiFi Network</div>
    <form id="wifiForm">
      <label>Network SSID</label>
      <input type="text"     id="wifiSsid" name="ssid" maxlength="63" placeholder="Your router SSID" required>
      <label>Password <small>(leave blank to keep current)</small></label>
      <input type="password" id="wifiPass" name="pass" maxlength="63" placeholder="Current password not shown">
      <button type="submit" class="btn btn-red" id="wifiBtn">Save &amp; Restart</button>
    </form>
    <div class="msg" id="wifiMsg"></div>
    <p class="note">
      The device restarts immediately after saving.
      Reconnect via <strong>greenhouse.local</strong> or find the new IP on your router.
    </p>
  </div>

</div>
<script>
fetch('/api/settings')
  .then(function(r) { return r.json(); })
  .then(function(d) {
    if (d.apSsid)   document.getElementById('apSsid').value   = d.apSsid;
    if (d.wifiSsid) document.getElementById('wifiSsid').value = d.wifiSsid;
  })
  .catch(function() {});

function showMsg(id, text, ok) {
  var el = document.getElementById(id);
  el.textContent = text;
  el.className = 'msg ' + (ok ? 'ok' : 'err');
}

document.getElementById('apForm').addEventListener('submit', function(e) {
  e.preventDefault();
  var ssid = document.getElementById('apSsid').value.trim();
  var pass = document.getElementById('apPass').value;
  if (pass.length > 0 && pass.length < 8) {
    showMsg('apMsg', 'Password must be at least 8 characters (or leave blank for open network).', false);
    return;
  }
  var btn = e.target.querySelector('button');
  btn.disabled = true;
  fetch('/api/settings/ap', {
    method: 'POST',
    body: new URLSearchParams({ ssid: ssid, pass: pass })
  })
  .then(function(r) { return r.json(); })
  .then(function(d) {
    showMsg('apMsg', d.ok ? 'Hotspot settings saved.' : ('Error: ' + (d.error || 'unknown')), d.ok);
  })
  .catch(function() { showMsg('apMsg', 'Request failed.', false); })
  .finally(function() { btn.disabled = false; });
});

document.getElementById('wifiForm').addEventListener('submit', function(e) {
  e.preventDefault();
  var ssid = document.getElementById('wifiSsid').value.trim();
  var pass = document.getElementById('wifiPass').value;
  var btn  = document.getElementById('wifiBtn');
  btn.disabled = true;
  fetch('/api/settings/wifi', {
    method: 'POST',
    body: new URLSearchParams({ ssid: ssid, pass: pass })
  })
  .then(function(r) { return r.json(); })
  .then(function(d) {
    if (d.ok) {
      showMsg('wifiMsg', 'Saved. Device is restarting — reconnect via greenhouse.local', true);
    } else {
      showMsg('wifiMsg', 'Error: ' + (d.error || 'unknown'), false);
      btn.disabled = false;
    }
  })
  .catch(function() {
    showMsg('wifiMsg', 'Request failed.', false);
    btn.disabled = false;
  });
});
</script>
</body>
</html>
)rawhtml";
