#pragma once

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
<a href="/setup">Guide</a>
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

static void handleSettings() {
  _server.send(200, "text/html; charset=utf-8", buildSettings());
}
