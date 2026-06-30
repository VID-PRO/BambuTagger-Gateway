#pragma once

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
<a href="/setup">Guide</a>
<a href="/config/ota">Update</a>
</nav>
<div class="wrapper">
<div class="card" style="max-width:560px">
  <h2>Connection Info</h2>
  <p style="margin:0 0 12px;font-size:13px;line-height:1.5">
    Port <strong>8883</strong> is a transparent TCP relay to the real printer.
    Bambu Studio negotiates TLS directly with the printer's certificate — no
    gateway certificate import is needed.
  </p>
  <p style="margin:0 0 12px;font-size:13px;line-height:1.5">
    For local MQTT tools, use port <strong>1883</strong> (plain TCP).
    The gateway automatically subscribes and forwards printer reports.
  </p>
  <p style="margin:0 0 12px;font-size:13px;line-height:1.5">
    Access code (from Dashboard): <code>%%PRINTER_CODE%%</code>
  </p>
</div>
</div>
<footer><a href="https://github.com/VID-PRO/BambuTagger-Gateway" target="_blank">BambuTagger-Gateway v%%VERSION%%</a> &mdash; MIT License</footer>
</body>
</html>
)html";

static void handleCert() {
  String page = buildPage(_PAGE_CERT, _PAGE_CERT2);
  page.replace("%%VERSION%%", VERSION);
  page.replace("%%PRINTER_CODE%%", _cfg.printerCode);
  _server.send(200, "text/html", page);
}

static void handleCaPem() {
  _server.send(200, "application/x-pem-file", mqtt.getTlsCert());
}
