#pragma once

#include <EEPROM.h>
#include "config.h"

#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Updater.h>
using WebServer = ESP8266WebServer;
#endif
#include "mqtt_bridge.h"
#include "logo_png.h"


#define EEPROM_SIZE 512
#define EEPROM_MAGIC 0x45

extern MqttBridge mqtt;
extern char _displayName[];

static GatewayConfig _cfg;
static WebServer _server(80);
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
h2{color:#58a6ff;font-size:.9em;letter-spacing:.05em;text-transform:uppercase;margin:0 0 16px;padding-bottom:4px;border-bottom:1px solid #30363d}
label{display:block;color:#8b949e;font-size:.8em;margin-bottom:3px}
input{display:block;width:100%;padding:10px 12px;margin-bottom:10px;background:#0d1117;color:#c9d1d9;border:1px solid #30363d;border-radius:6px;font-size:.95em;outline:none;transition:border .2s}
select{display:block;width:100%;padding:10px 12px;margin-bottom:10px;background:#0d1117;color:#c9d1d9;border:1px solid #30363d;border-radius:6px;font-size:.95em;outline:none;transition:border .2s;cursor:pointer}
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
<a href="/config/settings">Printer</a>
<a href="/config/wifi">WiFi</a>
<a href="/config/ota">Update</a>
</nav>
<div class="wrapper">
<div class="card" style="max-width:560px">
  <h2>Gateway Status</h2>
  <div style="margin:12px 0">
    <div class="info-row"><span class="lbl">AP SSID</span><span class="val">%%AP_SSID%%</span></div>
    <div class="info-row"><span class="lbl">Station</span><span class="val">%%STA_STATUS%%</span></div>
    <div class="info-row"><span class="lbl">Printer</span><span class="val">%%PRINTER_HOST%%</span></div>
    <div class="info-row"><span class="lbl">MQTT Link</span><span class="val">%%MQTT_STATUS%%</span></div>
    <div class="info-row"><span class="lbl">Uptime</span><span class="val">%%UPTIME%%</span></div>
    <div class="info-row"><span class="lbl">Free Heap</span><span class="val">%%HEAP%%</span></div>
    <div class="info-row"><span class="lbl">Version</span><span class="val">%%VERSION%%</span></div>
  </div>
  </div>
</div>
<div class="wrapper">
<div class="card" style="max-width:560px">
  <h2>Gateway Identity</h2>
  <div style="margin:12px 0">
    <div class="info-row"><span class="lbl">IP</span><span class="val">%%STA_IP%%</span></div>
    <div class="info-row"><span class="lbl">Printer Name</span><span class="val">%%PRINTER_NAME%%</span></div>
    <div class="info-row"><span class="lbl">Serial</span><span class="val">%%GATEWAY_SERIAL%%</span></div>
    <div class="info-row"><span class="lbl">Model</span><span class="val">%%PRINTER_MODEL%%</span></div>
    <div class="info-row"><span class="lbl">TLS Cert</span><span class="val"><a href="/cert" style="color:#58a6ff;text-decoration:none">Import &rarr;</a></span></div>
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
<footer>(c) 2026 by <a href="https://www.bambutagger.de" target="_blank">BambuTagger</a></footer>
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
<a href="/config/settings" class="active">Printer</a>
<a href="/config/wifi">WiFi</a>
<a href="/config/ota">Update</a>
</nav>
<div class="wrapper">
<div class="card">
  <h2>Printer Settings</h2>
  <form method="post" action="/save">
    <input type="hidden" name="form" value="printer">
    <label>Hostname / IP</label>
    <input name="printer_host" placeholder="e.g. 192.168.1.100 or bambu-printer.local" value="%%PRINTER_HOST%%">
    <p class="hint">Printer: Settings &#8594; Network &#8594; IP Address</p>

    <label>Access Code</label>
    <input name="printer_code" type="password" placeholder="8-digit access code" value="%%PRINTER_CODE%%">
    <p class="hint">Printer: Settings &#8594; Network &#8594; Access Code</p>

    <label>Printer Serial Number</label>
    <input name="printer_serial" placeholder="e.g. 22E8BJ5B0900685" value="%%PRINTER_SERIAL%%">
    <p class="hint">Serial of the real Bambu printer (used for upstream MQTT)</p>

    <label>Printer Model</label>
    <select name="printer_model">%%PRINTER_MODEL_OPTIONS%%</select>
    <p class="hint">Used when Bambu Studio asks for printer identity</p>

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
<a href="/config/settings">Printer</a>
<a href="/config/wifi" class="active">WiFi</a>
<a href="/config/ota">Update</a>
</nav>
<div class="wrapper">
<div class="card">
  <h2>WiFi Settings</h2>
  <form method="post" action="/save">
    <input type="hidden" name="form" value="wifi">
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

// ── Update page ────────────────────────────────────────────────────
static const char _PAGE_OTA[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway — Update</title>
<style>
)html";

static const char _PAGE_OTA2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/">Dashboard</a>
<a href="/config/settings">Printer</a>
<a href="/config/wifi">WiFi</a>
<a href="/config/ota" class="active">Update</a>
</nav>
<div class="wrapper">
<div class="card">
  <h2>Firmware Update</h2>
  <p style="color:#8b949e;font-size:13px;margin-bottom:16px">Download and install the latest firmware from GitHub.</p>
  <button onclick="location='/update/github'">Update from GitHub</button>
  <p style="color:#484f58;font-size:12px;margin-top:12px">Current version: %%VERSION%%</p>
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
<meta http-equiv="refresh" content="3;url=/">
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
    char gwSerial[32];
#if defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    uint32_t chipId = (uint32_t)(mac >> 24);
#else
    uint32_t chipId = ESP.getChipId();
#endif
    snprintf(gwSerial, sizeof(gwSerial), "22E8BJ5B%07X", chipId & 0xFFFFFF);
    strlcpy(_cfg.gatewaySerial, gwSerial, sizeof(_cfg.gatewaySerial));
    strlcpy(_cfg.printerModel, PRINTER_MODEL_DFLT, sizeof(_cfg.printerModel));
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

  if (strlen(_cfg.stationSsid) > 0) {
    bool staUp = WiFi.isConnected();
    page.replace("%%STA_STATUS%%", String("<span class=\"") + (staUp ? "up" : "down") + "\">"
      + _cfg.stationSsid + " " + (staUp ? "&#10003;" : "&#10007;") + "</span>");
    page.replace("%%STA_IP%%", staUp ? WiFi.localIP().toString() : "<span class=\"down\">inactive</span>");
  } else {
    page.replace("%%STA_STATUS%%", "<span class=\"down\">disabled</span>");
    page.replace("%%STA_IP%%", "<span class=\"down\">disabled</span>");
  }

  page.replace("%%PRINTER_HOST%%", _cfg.printerHost);
  page.replace("%%GATEWAY_SERIAL%%", _cfg.gatewaySerial);
  page.replace("%%PRINTER_MODEL%%", _cfg.printerModel);
  page.replace("%%PRINTER_NAME%%", _displayName);
  {
    const char *mqttCls, *mqttLabel;
    switch (mqtt.getStatus()) {
      case MQTT_UP:     mqttCls = "up";   mqttLabel = "connected";    break;
      case MQTT_TRYING: mqttCls = "mid";  mqttLabel = "connecting";   break;
      default:          mqttCls = "down"; mqttLabel = "disconnected"; break;
    }
    page.replace("%%MQTT_STATUS%%", String("<span class=\"") + mqttCls + "\">" + mqttLabel + "</span>");
  }
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

  String opts;
  const char *models[] = {"P1S", "P1P", "P2S", "X1C", "X1E", "A1", "A1 Mini", "A2L"};
  for (auto m : models) {
    opts += "<option";
    if (strcmp(_cfg.printerModel, m) == 0) opts += " selected";
    opts += ">";
    opts += m;
    opts += "</option>";
  }
  page.replace("%%PRINTER_MODEL_OPTIONS%%", opts);

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

static void handleUpdateFromGithub() {
  String html = F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Updating&hellip;</title><style>");
  html += FPSTR(_SITE_STYLE);
  html += F("</style></head><body>");
  html += F("<header><div class=\"logo\"><img src=\"/Logo/bambutagger.png\" alt=\"B\"><h1>BambuTagger-Gateway</h1></div></header>");
  html += F("<div class=\"wrapper\"><div class=\"card\" style=\"text-align:center\">");
  html += F("<h1 style=\"color:#58a6ff\">Updating&hellip;</h1>");
  html += F("<p style=\"color:#8b949e;margin-top:8px\">Downloading from GitHub and flashing</p>");
  html += F("</div></div></body></html>");
  _server.send(200, "text/html; charset=utf-8", html);
  _server.client().flush();
  _server.client().stop();

  delay(100);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(30000);
  http.setUserAgent(F("BambuTagger-Gateway"));

  String url = F("https://github.com/VID-PRO/BambuTagger-Gateway/releases/latest/download/BambuTagger-Gateway.ino.bin");

  http.begin(client, url);
  int code = http.GET();

  if (code == 200) {
    int len = http.getSize();
    if (len > 0) {
      WiFiClient *stream = http.getStreamPtr();
      if (Update.begin(len)) {
        size_t written = 0;
        uint8_t buf[256];
        while (written < (size_t)len) {
          int avail = stream->available();
          if (avail > 0) {
            int rd = stream->readBytes(buf, min(avail, (int)sizeof(buf)));
            Update.write(buf, rd);
            written += rd;
          } else {
            delay(1);
          }
        }
        if (Update.end(true)) {
          Serial.println("GitHub update successful, rebooting");
          http.end();
          delay(500);
          ESP.restart();
        }
      }
    }
  }
  http.end();
  Serial.println("GitHub update failed");
}

static bool isValidHost(const String &host) {
  if (host.length() == 0) return false;

  // Check if input looks like an IPv4 address (digits + dots only)
  bool ipv4Like = true;
  int dots = 0;
  for (unsigned int i = 0; i < host.length(); i++) {
    char c = host[i];
    if (c == '.') dots++;
    else if (c < '0' || c > '9') { ipv4Like = false; break; }
  }

  if (ipv4Like && dots == 3) {
    // Validate every octet is 0-255
    int seen = 0, val = 0;
    for (unsigned int i = 0; i <= host.length(); i++) {
      char c = (i < host.length()) ? host[i] : '.';
      if (c >= '0' && c <= '9') {
        val = val * 10 + (c - '0');
      } else if (c == '.') {
        if (i == 0 || host[i-1] == '.') return false; // empty or leading dot
        if (val > 255) return false;
        seen++;
        val = 0;
      }
    }
    return seen == 4;
  }

  // Hostname — accept
  return true;
}

static void handleSave() {
  String formType = _server.arg("form");

  // Printer settings
  if (formType == "printer") {
    String host = _server.arg("printer_host");
    if (!isValidHost(host)) {
      String err = FPSTR(_PAGE_SAVED);
      err.replace("&#10003; Saved!", "&#10007; Invalid Host");
      err.replace("<p>Settings stored. Rebooting&hellip;</p>",
        "<p>The printer host &ldquo;" + host + "&rdquo; is not a valid IP address or hostname.</p>");
      err.replace("<meta http-equiv=\"refresh\" content=\"3;url=/\">",
        "<a href=\"/config/settings\" style=\"color:#58a6ff\">&larr; Back to Printer Settings</a>");
      _server.send(200, "text/html; charset=utf-8", err);
      return;
    }
    strlcpy(_cfg.printerHost, host.c_str(), sizeof(_cfg.printerHost));
    strlcpy(_cfg.printerCode, _server.arg("printer_code").c_str(), sizeof(_cfg.printerCode));
    strlcpy(_cfg.printerSerial, _server.arg("printer_serial").c_str(), sizeof(_cfg.printerSerial));
    strlcpy(_cfg.printerModel, _server.arg("printer_model").c_str(), sizeof(_cfg.printerModel));
  }

  // WiFi settings
  if (formType == "wifi") {
    strlcpy(_cfg.stationSsid, _server.arg("sta_ssid").c_str(), sizeof(_cfg.stationSsid));
    strlcpy(_cfg.stationPass, _server.arg("sta_pass").c_str(), sizeof(_cfg.stationPass));
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
  _server.sendHeader("Cache-Control", "max-age=86400");
  _server.send_P(200, "image/png", (const char*)logo_png, sizeof(logo_png));
}

// ── Cert page ───────────────────────────────────────────────────────────
static const char _PAGE_CERT[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BambuTagger-Gateway — TLS Certificate</title>
<style>
)html";

static const char _PAGE_CERT2[] PROGMEM = R"html(
</style>
</head>
<body>
<header><div class="logo"><img src="/Logo/bambutagger.png" alt="B"><h1>BambuTagger-Gateway</h1></div></header>
<nav>
<a href="/">Dashboard</a>
<a href="/config/settings">Printer</a>
<a href="/config/wifi">WiFi</a>
<a href="/config/ota">Update</a>
</nav>
<div class="wrapper">
<div class="card" style="max-width:560px">
  <h2>TLS Certificate</h2>
  <p style="margin:0 0 12px;font-size:13px;line-height:1.5">
    Bambu Studio and OrcaSlicer verify the TLS certificate presented
    by the printer against their bundled BBL CA. Because this gateway
    uses a self-signed certificate, you must import it into the
    slicer before you can connect via LAN (port 8883).
  </p>
  <p style="margin:0 0 12px;font-size:13px;line-height:1.5">
    Bambu Studio and OrcaSlicer verify the TLS certificate against a
    bundled <code>printer.cer</code> file — they do <strong>not</strong>
    use the system certificate store. To trust this gateway, append the
    downloaded cert to that file:<br><br>
    <strong>macOS</strong>:<br>
    <code style="font-size:11px">sudo bash -c 'cat ~/Downloads/gateway-ca.pem &gt;&gt; /Applications/BambuStudio.app/Contents/Resources/cert/printer.cer'</code><br>
    <small>(OrcaSlicer: <code style="font-size:11px">/Applications/OrcaSlicer.app/Contents/Resources/cert/printer.cer</code>)</small><br><br>
    <strong>Windows</strong>:<br>
    <code style="font-size:11px">Notepad as admin: C:\Program Files\Bambu Studio\resources\cert\printer.cer</code><br>
    <small>(OrcaSlicer: <code style="font-size:11px">C:\Program Files\OrcaSlicer\resources\cert\printer.cer</code>)</small><br><br>
    Paste the contents of <code>gateway-ca.pem</code> at the end of the
    file, save, and restart the slicer.
  </p>
  <a href="/cert?dl=1" style="display:inline-block;padding:10px 20px;margin:0 0 6px;background:#238636;color:#fff;border-radius:6px;text-decoration:none;font-weight:bold;font-size:14px">Download gateway-ca.pem</a>
  <p style="margin:12px 0 0;font-size:11px;color:#484f58">
    Gateway serial: %%GATEWAY_SERIAL%%
  </p>
</div>
</div>
<footer><a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway v%%VERSION%%</a> &mdash; MIT License</footer>
</body>
</html>
)html";

// Serve the TLS CA certificate page (or download the PEM with ?dl=1).
static void handleCert() {
  if (!_server.hasArg("dl")) {
    String page = buildPage(_PAGE_CERT, _PAGE_CERT2);
    page.replace("%%VERSION%%", VERSION);
    page.replace("%%GATEWAY_SERIAL%%", _cfg.gatewaySerial);
    _server.send(200, "text/html", page);
    return;
  }
  const char *pem = MqttBridge::getTlsCert();
  _server.sendHeader("Content-Disposition",
                      "attachment; filename=gateway-ca.pem");
  _server.send_P(200, "application/x-pem-file", pem, strlen_P(pem));
}

// ── Public API ─────────────────────────────────────────────────────────

inline void webconfigInit() {
  configLoad();
}

inline void webconfigBegin() {
  _server.on("/", handleRoot);
  _server.on("/Logo/bambutagger.png", handleLogo);
  _server.on("/cert", handleCert);
  _server.on("/config", []() {
    _server.sendHeader("Location", "/config/settings", true);
    _server.send(302, "text/plain", "");
  });
  _server.on("/config/settings", handleSettings);
  _server.on("/config/wifi", handleWifi);
  _server.on("/config/ota", handleOta);
  _server.on("/update/github", handleUpdateFromGithub);
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
