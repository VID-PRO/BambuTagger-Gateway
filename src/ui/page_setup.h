#pragma once

static const char _PAGE_SETUP[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway — Setup Guide</title>
<style>
ol{color:#c9d1d9;font-size:.95em;padding-left:22px;margin:0 0 14px}
ol li{margin-bottom:10px;line-height:1.5}
ol li strong{color:#f0f6fc}
code{background:#0d1117;color:#f0f6fc;padding:2px 6px;border-radius:3px;font-size:.9em}
.param-table{width:100%;border-collapse:collapse;font-size:.85em;margin:10px 0 16px}
.param-table th{text-align:left;color:#8b949e;padding:6px 8px;border-bottom:1px solid #30363d;font-weight:normal}
.param-table td{padding:6px 8px;border-bottom:1px solid #1c2128;color:#c9d1d9}
.param-table td:first-child{color:#f0f6fc;font-weight:600;white-space:nowrap}
.note{background:rgba(88,166,255,0.08);border-left:3px solid #58a6ff;padding:10px 14px;margin:14px 0;border-radius:0 6px 6px 0;font-size:.85em;color:#8b949e;line-height:1.5}
.note strong{color:#c9d1d9}
.step-num{display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;background:#238636;color:#fff;border-radius:50%;font-size:.8em;font-weight:bold;margin-right:8px;flex-shrink:0}
.step{display:flex;align-items:flex-start;margin-bottom:16px;line-height:1.5}
.step-body{flex:1}
.step-body strong{color:#f0f6fc;font-size:.95em}
.step-body p{color:#8b949e;font-size:.88em;margin:3px 0 0}
.step-body p code{font-size:.9em}
)html";

static const char _PAGE_SETUP2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/">Dashboard</a>
<a href="/setup" class="active">Guide</a>
<a href="/config/settings">Printer</a>
<a href="/config/wifi">WiFi</a>
<a href="/config/ota">Update</a>
</nav>
<div class="wrapper">
<div class="card" style="max-width:580px">
  <h2>Setup Guide</h2>
  <p style="color:#8b949e;font-size:.85em;margin-bottom:20px">
    How to connect Bambu Studio (or OrcaSlicer) through the gateway.
  </p>

  <div class="step">
    <span class="step-num">1</span>
    <div class="step-body">
      <strong>Connect to the Gateway</strong>
      <p>Join WiFi network <strong>BambuTagger-Gateway</strong> (open).
         Or connect your computer to the same LAN as the gateway if you
         configured station WiFi.</p>
    </div>
  </div>

  <div class="step">
    <span class="step-num">2</span>
    <div class="step-body">
      <strong>Configure Printer Settings</strong>
      <p>Open the <a href="/config/settings" style="color:#58a6ff">Printer page</a>.
         Enter your printer's IP address, access code, serial number,
         and model. Click <strong>Save &amp; Reboot</strong>.</p>
    </div>
  </div>

  <div class="step">
    <span class="step-num">3</span>
    <div class="step-body">
      <strong>Verify Gateway Status</strong>
      <p>On the <a href="/" style="color:#58a6ff">Dashboard</a>,
         check that <strong>Printer Link</strong> shows
         <span style="color:#3fb950">connected</span>.
         The LED on the gateway should turn off.</p>
    </div>
  </div>

  <div class="step">
    <span class="step-num">4</span>
    <div class="step-body">
      <strong>Add Printer in Bambu Studio</strong>
      <p>Open Bambu Studio &rarr; <strong>Prepare</strong> tab &rarr;
         click the printer icon &rarr; <strong>Add Printer</strong>.
         Select your printer model, choose <strong>LAN</strong> mode,
         and enter the gateway's IP and your printer's access code.</p>
    </div>
  </div>

  <div class="step">
    <span class="step-num">5</span>
    <div class="step-body">
      <strong>Start Printing</strong>
      <p>The printer should appear online. Send a print job &mdash;
         the gateway passes all traffic (MQTT, camera, file transfer)
         to the real printer transparently.</p>
    </div>
  </div>

  <h2 style="margin-top:24px">Connection Parameters</h2>
  <table class="param-table">
    <tr><th>Service</th><th>Gateway Value</th></tr>
    <tr><td>MQTT (Studio)</td><td><code>%%GATEWAY_IP%%:8883</code> &mdash; TLS passthrough, end-to-end encrypted</td></tr>
    <tr><td>MQTT (local tools)</td><td><code>%%GATEWAY_IP%%:1883</code> &mdash; plain TCP, multiplexed</td></tr>
    <tr><td>Camera</td><td><code>%%GATEWAY_IP%%:6000</code></td></tr>
    <tr><td>FTPS</td><td><code>%%GATEWAY_IP%%:990</code></td></tr>
  </table>

  <div class="note">
    <strong>TLS Passthrough:</strong> Port 8883 is a transparent TCP relay.
    TLS is negotiated end-to-end between Bambu Studio and the real printer.
    The printer presents its BBL-signed certificate &mdash; <strong>no
    certificate import is needed</strong> on the gateway side.
  </div>

  <div class="note" style="border-left-color:#da3633;background:rgba(218,54,51,0.08)">
    <strong>Trouble connecting?</strong> Make sure the printer has
    <strong>LAN mode</strong> enabled (Settings &rarr; Network &rarr;
    LAN Mode). The gateway and printer must be on the same network.
    Verify the serial number and access code match the printer's
    sticker or settings page.
  </div>

  <h2 style="margin-top:24px">Printer Settings Reference</h2>
  <table class="param-table">
    <tr><th>Setting</th><th>Where to find it on your printer</th></tr>
    <tr><td>IP Address</td><td>Settings &rarr; Network &rarr; IP Address</td></tr>
    <tr><td>Access Code</td><td>Settings &rarr; Network &rarr; Access Code</td></tr>
    <tr><td>Serial Number</td><td>Settings &rarr; General &rarr; Machine Info, or sticker on the printer</td></tr>
    <tr><td>LAN Mode</td><td>Settings &rarr; Network &rarr; LAN Mode (must be ON)</td></tr>
  </table>

</div>
</div>
<footer>&copy; 2026 — <a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway</a></footer>
</body>
</html>
)html";

static String buildSetup() {
  String page = buildPage(_PAGE_SETUP, _PAGE_SETUP2);
  page.replace("%%GATEWAY_IP%%",
    WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
  return page;
}

static void handleSetup() {
  _server.send(200, "text/html; charset=utf-8", buildSetup());
}
