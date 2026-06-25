#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Updater.h>
#include "config.h"
#include "mqtt_bridge.h"
#include "logo_png.h"


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
<link rel="icon" href="/Logo/bambutagger.png" type="image/png">
<style>
)html";

static const char _PAGE_ROOT2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
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
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
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
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
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
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
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
<link rel="icon" href="/Logo/bambutagger.png" type="image/png">
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
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
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

// ── Logo handler ───────────────────────────────────────────────────────

static void handleLogo() {
  _server.send_P(200, "image/png", (const char*)logo_png, sizeof(logo_png));
}

// ── Public API ─────────────────────────────────────────────────────────

inline void webconfigBegin() {
  configLoad();

  _server.on("/", handleRoot);
  _server.on("/Logo/bambutagger.png", handleLogo);
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
