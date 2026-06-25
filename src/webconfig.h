#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Updater.h>
#include "config.h"
#include "mqtt_bridge.h"

static const char LOGO_B64[] PROGMEM = R"(
iVBORw0KGgoAAAANSUhEUgAAAYEAAAF7CAYAAAA0UDdbAAAA8HpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4
aWYAAHjajVFrbgYhCPzvKXoEeahwHPfxJb1Bj99ZxW73S5qURBhHFgY2nV+fr/RxGRsnLc2q15ph6urc
ASxP24anrMMPMwpETz61LSAjCqLMB+codoIHprh7NKGVvwotQB2o3A+9B789+S0Ksr0XCgVCs3M+4oMo
JByKdN73UFTd2mO0Y89Ps/uoNK6lUlN45dxadWDjrA37PC6hr519FCpzoT/Euq9UhiY+hSTDi9hUKdep
0hELPAknnnQXHZSOVIyCXwkJUO7RKEa9lvl7N/eO/rD/jJW+AZy5dvcCVXlDAAABhWlDQ1BJQ0MgcHJv
ZmlsZQAAeJx9kb9Lw0AcxV9TS0UqQu0g4pChOtlFRRxLFYtgobQVWnUwufQXNGlIUlwcBdeCgz8Wqw4u
zro6uAqC4A8Q/wBxUnSREr+XFFrEeHDch3f3HnfvAKFVY6rZFwdUzTIyyYSYL6yKwVcEEEIYMYQlZuqp
7GIOnuPrHj6+3sV4lve5P8egUjQZ4BOJ40w3LOIN4tlNS+e8TxxhFUkhPieeNOiCxI9cl11+41x2WOCZ
ESOXmSeOEIvlHpZ7mFUMlXiGOKqoGuULeZcVzluc1VqDde7JXxgqaitZrtMcQxJLSCENETIaqKIGi/qq
QiPFRIb2Ex7+UcefJpdMrioYORZQhwrJ8YP/we9uzdL0lJsUSgCBF9v+GAeCu0C7advfx7bdPgH8z8CV
1vXXW8DcJ+nNrhY9Aoa2gYvrribvAZc7wMiTLhmSI/lpCqUS8H5G31QAhm+BgTW3t84+Th+AHHW1fAMc
HAITZcpe93h3f29v/57p9PcDt7ZywhdWimUAAA12aVRYdFhNTDpjb20uYWRvYmUueG1wAAAAAAA8P3hw
YWNrZXQgYmVnaW49Iu+7vyIgaWQ9Ilc1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCI/Pgo8eDp4bXBtZXRh
IHhtbG5zOng9ImFkb2JlOm5zOm1ldGEvIiB4OnhtcHRrPSJYTVAgQ29yZSA0LjQuMC1FeGl2MiI+CiA8
cmRmOlJERiB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1u
cyMiPgogIDxyZGY6RGVzY3JpcHRpb24gcmRmOmFib3V0PSIiCiAgICB4bWxuczp4bXBNTT0iaHR0cDov
L25zLmFkb2JlLmNvbS94YXAvMS4wL21tLyIKICAgIHhtbG5zOnN0RXZ0PSJodHRwOi8vbnMuYWRvYmUu
Y29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VFdmVudCMiCiAgICB4bWxuczpkYz0iaHR0cDovL3B1cmwu
b3JnL2RjL2VsZW1lbnRzLzEuMS8iCiAgICB4bWxuczpHSU1QPSJodHRwOi8vd3d3LmdpbXAub3JnL3ht
cC8iCiAgICB4bWxuczp0aWZmPSJodHRwOi8vbnMuYWRvYmUuY29tL3RpZmYvMS4wLyIKICAgIHhtbG5z
OnhtcD0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wLyIKICAgeG1wTU06RG9jdW1lbnRJRD0iZ2lt
cDpkb2NpZDpnaW1wOmVlNWNmMTQ2LTlkZTItNDBjMS05YzNkLTA4OGI1MTUzMmVkMiIKICAgeG1wTU06
SW5zdGFuY2VJRD0ieG1wLmlpZDozMTg3OWFiOC1iYWJiLTQzNjMtYTMzMC0yOTcwODliZDcxNTIiCiAg
IHhtcE1NOk9yaWdpbmFsRG9jdW1lbnRJRD0ieG1wLmRpZDpkNjA3YjVmNy0zNmViLTQ1OGUtYTM0Yi1i
YzA3YjIzYWVhNWQiCiAgIGRjOkZvcm1hdD0iaW1hZ2UvcG5nIgogICBHSU1QOkFQST0iMi4wIgogICBH
SU1QOlBsYXRmb3JtPSJXaW5kb3dzIgogICBHSU1QOlRpbWVTdGFtcD0iMTc3ODYxMTU0ODQ1MzA5OCIK
ICAgR0lNUDpWZXJzaW9uPSIyLjEwLjM4IgogICB0aWZmOk9yaWVudGF0aW9uPSIxIgogICB4bXA6Q3Jl
YXRvclRvb2w9IkdJTVAgMi4xMCIKICAgeG1wOk1ldGFkYXRhRGF0ZT0iMjAyNjowNToxMlQyMDo0NTo0
OCswMjowMCIKICAgeG1wOk1vZGlmeURhdGU9IjIwMjY6MDU6MTJUMjA6NDU6NDgrMDI6MDAiPgogICA8
eG1wTU06SGlzdG9yeT4KICAgIDxyZGY6U2VxPgogICAgIDxyZGY6bGkKICAgICAgc3RFdnQ6YWN0aW9u
PSJzYXZlZCIKICAgICAgc3RFdnQ6Y2hhbmdlZD0iLyIKICAgICAgc3RFdnQ6aW5zdGFuY2VJRD0ieG1w
LmlpZDo5NDI2ZmRjZS03YTEwLTQ4NDgtYmNkMS04ODUyNTdjOGE2NWYiCiAgICAgIHN0RXZ0OnNvZnR3
YXJlQWdlbnQ9IkdpbXAgMi4xMCAoV2luZG93cykiCiAgICAgIHN0RXZ0OndoZW49IjIwMjYtMDUtMTJU
MjA6NDU6NDgiLz4KICAgIDwvcmRmOlNlcT4KICAgPC94bXBNTTpIaXN0b3J5PgogIDwvcmRmOkRlc2Ny
aXB0aW9uPgogPC9yZGY6UkRGPgo8L3g6eG1wbWV0YT4KICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
CiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAg
ICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgIAo8P3hwYWNrZXQgZW5kPSJ3Ij8+
jmPnfQAAAAZiS0dEAP8A/AD1Qi7w1AAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+oFDBItMOBs
v9EAAAAZdEVYdENvbW1lbnQAQ3JlYXRlZCB3aXRoIEdJTVBXgQ4XAAAgAElEQVR42u2dPW8bW5KGi2aD
IjGQIwYKJGAwgLL5Nwo2NRwtrqMF7s8QsJGFjQynG9x/s5mBxQJ24ECRhAtLBGVtcNmeVqtJ9sf5qKrz
vPFcj83urqfet86HCEIIIYQQQgghhBAqSDN+AmRR95u7ubL39/l08faJJ4OsqeInQAYL/fzi8vxh82Oj
5u+6WC3kfnO3FJEnQIGAAELTi/3eQv+0/Sm///Gbun/H+mz9MK/ejAUFgEBZRByEVBZ7rYV+qq6vbqQL
FIvVQr5++dYEBFBAQAC5KvhFFfupgOiAAmBAQACZKfqvCj7FfrprwC0gIIC0Fv4XRZ+Cn8UtAAQEBFCe
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
TkSuQmCC
)";

#define EEPROM_SIZE 256
#define EEPROM_MAGIC 0x42

extern MqttBridge mqtt;

static GatewayConfig _cfg;
static ESP8266WebServer _server(80);
static bool _webconfigActive = false;

static const char _SITE_STYLE[] PROGMEM = R"(
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;min-height:100vh;padding-bottom:28px}
header{background:#161b22;border-bottom:1px solid #30363d;padding:12px 24px}
header .logo{display:flex;align-items:center;gap:10px}
header .logo img{width:28px;height:28px;flex-shrink:0;border-radius:4px}
header h1{font-size:18px;color:#58a6ff}
nav{background:#161b22;border-bottom:1px solid #30363d;display:flex;gap:0}
nav a{padding:10px 20px;color:#8b949e;text-decoration:none;font-size:14px;border-bottom:2px solid transparent;transition:all .2s}
nav a:hover{color:#c9d1d9}
nav a.active{color:#58a6ff;border-bottom-color:#58a6ff}
.wrapper{display:flex;align-items:center;justify-content:center;padding:16px}
.card{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:28px;width:100%;max-width:480px}
.subnav{display:flex;gap:0;margin-bottom:18px;border-bottom:1px solid #30363d}
.subnav a{padding:8px 18px;color:#8b949e;text-decoration:none;font-size:13px;font-weight:600;border-bottom:2px solid transparent;margin-bottom:-1px;transition:all .2s}
.subnav a:hover{color:#c9d1d9}
.subnav a.active{color:#58a6ff;border-bottom-color:#58a6ff}
h2{color:#58a6ff;font-size:.9em;letter-spacing:.05em;text-transform:uppercase;margin:0 0 16px;padding-bottom:4px;border-bottom:1px solid #30363d}
label{display:block;color:#8b949e;font-size:.8em;margin-bottom:3px}
input{display:block;width:100%;padding:10px 12px;margin-bottom:10px;background:#0d1117;color:#c9d1d9;border:1px solid #30363d;border-radius:6px;font-size:.95em;outline:none;transition:border .2s}
input:focus{border-color:#58a6ff}
button{display:block;width:100%;padding:13px;margin-top:6px;background:#238636;color:#fff;border:none;border-radius:6px;font-size:1.05em;font-weight:bold;cursor:pointer;transition:background .2s}
button:hover{background:#2ea043}
.hint{color:#484f58;font-size:.78em;margin-top:-7px;margin-bottom:10px}
footer{position:fixed;bottom:0;left:0;right:0;text-align:center;padding:6px;font-size:10px;color:#484f58;background:#0d1117;border-top:1px solid #30363d}
footer a{color:#484f58;text-decoration:none}
footer a:hover{color:#c9d1d9}
)";

// ── pages ──────────────────────────────────────────────────────────────

static const char _PAGE_ROOT[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway</title>
<link rel="icon" href="data:image/png;base64,%%LOGO_B64%%" type="image/png">
<style>
)html";

static const char _PAGE_ROOT2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="data:image/png;base64,%%LOGO_B64%%" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/" class="active">Dashboard</a>
<a href="/config/settings">Settings</a>
<a href="/config/ota">OTA Update</a>
</nav>
<div class="wrapper">
<div class="card" style="max-width:560px">
  <h2>Gateway Status</h2>
  <div style="margin:12px 0">
    <div class="info-row"><span class="lbl">AP SSID</span><span class="val">%%AP_SSID%%</span></div>
    <div class="info-row"><span class="lbl">AP IP</span><span class="val">%%AP_IP%%</span></div>
    <div class="info-row"><span class="lbl">Station</span><span class="val">%%STA_STATUS%%</span></div>
    <div class="info-row"><span class="lbl">Printer</span><span class="val">%%PRINTER_HOST%%</span></div>
    <div class="info-row"><span class="lbl">MQTT Link</span><span class="val">%%MQTT_STATUS%%</span></div>
    <div class="info-row"><span class="lbl">Uptime</span><span class="val">%%UPTIME%%</span></div>
    <div class="info-row"><span class="lbl">Free Heap</span><span class="val">%%HEAP%%</span></div>
    <div class="info-row"><span class="lbl">Version</span><span class="val">%%VERSION%%</span></div>
  </div>
  </div>
</div>
</div>
<style>
.info-row{display:flex;justify-content:space-between;padding:6px 0;font-size:13px;border-bottom:1px solid #1c2128}
.info-row:last-child{border:none}
.info-row .lbl{color:#8b949e}
.info-row .val{color:#f0f6fc;font-weight:600;text-align:right}
.info-row .val.up{color:#3fb950}
.info-row .val.mid{color:#d29922}
.info-row .val.down{color:#da3633}
</style>
<footer><a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway v%%VERSION%%</a> &mdash; MIT License</footer>
</body>
</html>
)html";

// ── Settings page ──────────────────────────────────────────────────────
static const char _PAGE_SETTINGS[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway — Settings</title>
<style>
)html";

static const char _PAGE_SETTINGS2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="data:image/png;base64,%%LOGO_B64%%" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/">Dashboard</a>
<a href="/config/settings" class="active">Settings</a>
<a href="/config/ota">OTA Update</a>
</nav>
<div class="wrapper">
<div class="card">
  <div class="subnav"><a href="/config/settings" class="active">Printer</a><a href="/config/wifi">WiFi</a></div>
  <h2>Printer Settings</h2>
  <form method="post" action="/save">
    <label>Hostname / IP</label>
    <input name="printer_host" placeholder="e.g. 192.168.1.100 or bambu-printer.local" value="%%PRINTER_HOST%%">
    <p class="hint">Printer: Settings &#8594; Network &#8594; IP Address</p>

    <label>Access Code</label>
    <input name="printer_code" type="password" placeholder="8-digit access code" value="%%PRINTER_CODE%%">
    <p class="hint">Printer: Settings &#8594; Network &#8594; Access Code</p>

    <label>Serial Number</label>
    <input name="printer_serial" placeholder="e.g. 01S00C123456789" value="%%PRINTER_SERIAL%%">
    <p class="hint">Printer: Settings &#8594; About</p>

    <button type="submit">Save &amp; Reboot</button>
  </form>
</div>
</div>
<footer><a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway v%%VERSION%%</a> &mdash; MIT License</footer>
</body>
</html>
)html";

// ── WiFi page ──────────────────────────────────────────────────────────
static const char _PAGE_WIFI[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway — WiFi</title>
<style>
)html";

static const char _PAGE_WIFI2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="data:image/png;base64,%%LOGO_B64%%" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/">Dashboard</a>
<a href="/config/settings" class="active">Settings</a>
<a href="/config/ota">OTA Update</a>
</nav>
<div class="wrapper">
<div class="card">
  <div class="subnav"><a href="/config/settings">Printer</a><a href="/config/wifi" class="active">WiFi</a></div>
  <h2>WiFi Settings</h2>
  <form method="post" action="/save">
    <label>SSID</label>
    <input name="sta_ssid" placeholder="Your WiFi name" value="%%STA_SSID%%">
    <p class="hint">Leave empty to use AP-only mode</p>

    <label>Password</label>
    <input name="sta_pass" type="password" placeholder="WiFi password" value="%%STA_PASS%%">

    <button type="submit">Save &amp; Reboot</button>
  </form>
</div>
</div>
<footer><a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway v%%VERSION%%</a> &mdash; MIT License</footer>
</body>
</html>
)html";

// ── OTA Update page ────────────────────────────────────────────────────
static const char _PAGE_OTA[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway — OTA Update</title>
<style>
)html";

static const char _PAGE_OTA2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="data:image/png;base64,%%LOGO_B64%%" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/">Dashboard</a>
<a href="/config/settings">Settings</a>
<a href="/config/ota" class="active">OTA Update</a>
</nav>
<div class="wrapper">
<div class="card">
  <h2>Firmware Update</h2>
  <p style="color:#8b949e;font-size:13px;margin-bottom:16px">Upload a <code>.bin</code> firmware file to update the gateway. The device will reboot after a successful update.</p>
  <form method="post" action="/update" enctype="multipart/form-data">
    <label>Firmware binary</label>
    <input type="file" name="firmware" accept=".bin" required>
    <button type="submit">Upload &amp; Update</button>
  </form>
</div>
</div>
<footer><a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway v%%VERSION%%</a> &mdash; MIT License</footer>
</body>
</html>
)html";

// ── Saved confirmation ─────────────────────────────────────────────────
static const char _PAGE_SAVED[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="15;url=/">
<title>BambuTagger-Gateway</title>
<link rel="icon" href="data:image/png;base64,%%LOGO_B64%%" type="image/png">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;min-height:100vh;padding-bottom:28px}
header{background:#161b22;border-bottom:1px solid #30363d;padding:12px 24px}
header .logo{display:flex;align-items:center;gap:10px}
header .logo img{width:28px;height:28px;flex-shrink:0;border-radius:4px}
header h1{font-size:18px;color:#58a6ff}
.wrapper{display:flex;align-items:center;justify-content:center;padding:40px 16px;text-align:center}
.card{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:40px 32px;max-width:480px}
h1{color:#3fb950;font-size:2em;margin-bottom:10px}
p{color:#8b949e}
footer{position:fixed;bottom:0;left:0;right:0;text-align:center;padding:6px;font-size:10px;color:#484f58;background:#0d1117;border-top:1px solid #30363d}
</style>
</head>
<body>
<header><div class="logo"><img src="data:image/png;base64,%%LOGO_B64%%" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<div class="wrapper">
<div class="card">
  <h1>&#10003; Saved!</h1>
  <p>Settings stored. Rebooting&hellip;</p>
  <p style="margin-top:12px;font-size:.85em">Reconnecting in 15 seconds&hellip;</p>
</div>
</div>
<footer>BambuTagger-Gateway v%%VERSION%% &mdash; MIT License</footer>
</body>
</html>
)html";

// ── EEPROM helpers ─────────────────────────────────────────────────────

static void configLoad() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, _cfg);
  if (_cfg.magic != EEPROM_MAGIC) {
    memset(&_cfg, 0, sizeof(_cfg));
    _cfg.magic = EEPROM_MAGIC;
    strlcpy(_cfg.printerHost, PRINTER_HOST_DFLT, sizeof(_cfg.printerHost));
    strlcpy(_cfg.printerCode, PRINTER_CODE_DFLT, sizeof(_cfg.printerCode));
    strlcpy(_cfg.printerSerial, PRINTER_SERIAL_DFLT, sizeof(_cfg.printerSerial));
    _cfg.stationSsid[0] = '\0';
    _cfg.stationPass[0] = '\0';
  }
  EEPROM.end();
}

static void configSave() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, _cfg);
  EEPROM.commit();
  EEPROM.end();
}

// ── page builders ──────────────────────────────────────────────────────

static String buildPage(const char *tmpl1, const char *tmpl2) {
  String page = FPSTR(tmpl1);
  page += FPSTR(_SITE_STYLE);
  page += FPSTR(tmpl2);
  page.replace("%%LOGO_B64%%", FPSTR(LOGO_B64));
  return page;
}

static String buildRoot() {
  String page = buildPage(_PAGE_ROOT, _PAGE_ROOT2);
  page.replace("%%AP_SSID%%", GATEWAY_AP_SSID);
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    page.replace("%%AP_IP%%", WiFi.softAPIP().toString());
  } else {
    page.replace("%%AP_IP%%", "<span class=\"down\">inactive</span>");
  }

  if (strlen(_cfg.stationSsid) > 0) {
    bool staUp = WiFi.isConnected();
    page.replace("%%STA_STATUS%%", String("<span class=\"") + (staUp ? "up" : "down") + "\">"
      + _cfg.stationSsid + " " + (staUp ? "&#10003;" : "&#10007;") + "</span>");
  } else {
    page.replace("%%STA_STATUS%%", "<span class=\"down\">disabled</span>");
  }

  page.replace("%%PRINTER_HOST%%", _cfg.printerHost);
  {
    const char *mqttCls, *mqttLabel;
    switch (mqtt.getStatus()) {
      case MQTT_UP:     mqttCls = "up";   mqttLabel = "connected";    break;
      case MQTT_TRYING: mqttCls = "mid";  mqttLabel = "connecting";   break;
      default:          mqttCls = "down"; mqttLabel = "disconnected"; break;
    }
    page.replace("%%MQTT_STATUS%%", String("<span class=\"") + mqttCls + "\">" + mqttLabel + "</span>");
  }
  // These will be updated from main if needed; we use placeholders
  unsigned long secs = millis() / 1000;
  char upt[32];
  snprintf(upt, sizeof(upt), "%dd %02d:%02d:%02d",
    (int)(secs / 86400), (int)((secs % 86400) / 3600),
    (int)((secs % 3600) / 60), (int)(secs % 60));
  page.replace("%%UPTIME%%", upt);
  page.replace("%%HEAP%%", String(ESP.getFreeHeap()) + " B");
  page.replace("%%VERSION%%", VERSION);
  return page;
}

static String buildOta() {
  String page = buildPage(_PAGE_OTA, _PAGE_OTA2);
  page.replace("%%VERSION%%", VERSION);
  return page;
}

static String buildSettings() {
  String page = buildPage(_PAGE_SETTINGS, _PAGE_SETTINGS2);
  page.replace("%%PRINTER_HOST%%", _cfg.printerHost);
  page.replace("%%PRINTER_CODE%%", _cfg.printerCode);
  page.replace("%%PRINTER_SERIAL%%", _cfg.printerSerial);
  page.replace("%%VERSION%%", VERSION);
  return page;
}

static String buildWifi() {
  String page = buildPage(_PAGE_WIFI, _PAGE_WIFI2);
  page.replace("%%STA_SSID%%", _cfg.stationSsid);
  page.replace("%%STA_PASS%%", _cfg.stationPass);
  page.replace("%%VERSION%%", VERSION);
  return page;
}

// ── handlers ───────────────────────────────────────────────────────────

static void handleRoot() {
  _server.send(200, "text/html; charset=utf-8", buildRoot());
}

static void handleSettings() {
  _server.send(200, "text/html; charset=utf-8", buildSettings());
}

static void handleWifi() {
  _server.send(200, "text/html; charset=utf-8", buildWifi());
}

static void handleOta() {
  _server.send(200, "text/html; charset=utf-8", buildOta());
}

static void handleUpdateUpload() {
  HTTPUpload &upload = _server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s (%u bytes)\n", upload.filename.c_str(), upload.contentLength);
    if (!Update.begin(upload.contentLength)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    Serial.println("Update aborted");
  }
}

static void handleUpdate() {
  if (Update.hasError()) {
    _server.send(200, "text/html", "<html><body><h1>Update Failed</h1><p>Check serial output for details.</p><a href='/config/ota'>Back</a></body></html>");
  } else {
    _server.send(200, "text/html", "<html><body><h1>Update Successful</h1><p>Rebooting...</p></body></html>");
    delay(500);
    ESP.restart();
  }
}

static void handleSave() {
  // Printer settings
  String host = _server.arg("printer_host");
  String code = _server.arg("printer_code");
  String serial = _server.arg("printer_serial");

  if (host.length() > 0) {
    strlcpy(_cfg.printerHost, host.c_str(), sizeof(_cfg.printerHost));
  }
  if (code.length() > 0) {
    strlcpy(_cfg.printerCode, code.c_str(), sizeof(_cfg.printerCode));
  }
  if (serial.length() > 0) {
    strlcpy(_cfg.printerSerial, serial.c_str(), sizeof(_cfg.printerSerial));
  }

  // WiFi settings (can come from same form or separate)
  String ssid = _server.arg("sta_ssid");
  if (_server.hasArg("sta_ssid")) {
    strlcpy(_cfg.stationSsid, ssid.c_str(), sizeof(_cfg.stationSsid));
  }
  String pass = _server.arg("sta_pass");
  if (_server.hasArg("sta_pass")) {
    strlcpy(_cfg.stationPass, pass.c_str(), sizeof(_cfg.stationPass));
  }

  configSave();

  String page = FPSTR(_PAGE_SAVED);
  page.replace("%%LOGO_B64%%", FPSTR(LOGO_B64));
  page.replace("%%VERSION%%", VERSION);
  _server.send(200, "text/html; charset=utf-8", page);
  _server.client().flush();
  _server.client().stop();
  delay(1500);
  ESP.restart();
}

static void handleNotFound() {
  String loc;
  if (WiFi.getMode() == WIFI_AP) {
    loc = "http://" + WiFi.softAPIP().toString();
  } else {
    loc = "http://" + WiFi.localIP().toString();
  }
  _server.sendHeader("Location", loc, true);
  _server.send(302, "text/html",
    "<html><body><a href=\"" + loc + "\">Continue</a></body></html>");
}

// ── Public API ─────────────────────────────────────────────────────────

inline void webconfigBegin() {
  configLoad();

  _server.on("/", handleRoot);
  _server.on("/config", []() {
    _server.sendHeader("Location", "/config/settings", true);
    _server.send(302, "text/plain", "");
  });
  _server.on("/config/settings", handleSettings);
  _server.on("/config/wifi", handleWifi);
  _server.on("/config/ota", handleOta);
  _server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
  _server.on("/save", HTTP_POST, handleSave);
  _server.onNotFound(handleNotFound);

  _server.begin();
  _webconfigActive = true;
}

inline void webconfigLoop() {
  if (_webconfigActive) {
    _server.handleClient();
  }
}

inline GatewayConfig *webconfigGetConfig() {
  return &_cfg;
}
