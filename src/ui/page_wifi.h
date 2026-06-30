#pragma once

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
<a href="/setup">Guide</a>
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

static String buildWifi() {
  String page = buildPage(_PAGE_WIFI, _PAGE_WIFI2);
  page.replace("%%STA_SSID%%", _cfg.stationSsid);
  page.replace("%%STA_PASS%%", _cfg.stationPass);
  page.replace("%%VERSION%%", VERSION);
  return page;
}

static void handleWifi() {
  _server.send(200, "text/html; charset=utf-8", buildWifi());
}
