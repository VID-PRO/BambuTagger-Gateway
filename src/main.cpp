#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#endif
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <lwip/igmp.h>
#ifdef ESP32
#include <esp_wifi.h>
#endif

#include "config.h"
#include "ui/webconfig.h"
#include "mqtt_bridge.h"
#include "tcp_proxy.h"

MqttBridge mqtt;
TcpProxy camProxy("camera", CAM_LOCAL_PORT, PRINTER_HOST_DFLT, CAM_PRINTER_PORT, MAX_TCP_CLIENTS);
TcpProxy ftpsProxy("ftps", FTPS_LOCAL_PORT, PRINTER_HOST_DFLT, FTPS_PRINTER_PORT, MAX_TCP_CLIENTS);

// ── MQTT passthrough (port 8883) ─────────────────────────────────────────
// Transparent TCP relay: each downstream client gets its own upstream
// connection to the real printer. Bambu Studio does TLS end-to-end with
// the printer's certificate (BBL-signed) instead of the gateway's self-signed
// cert. Local MQTT tools use port 1883 (plain) via MqttBridge.
#define MAX_MQTT_PASSTHROUGH 4
struct {
  WiFiClient *down;
  WiFiClient *up;
  bool active;
  int deadCount;       // consecutive iterations with !connected
  bool relayed;        // true once we relayed at least one byte
} _mqttPtClients[MAX_MQTT_PASSTHROUGH];
static WiFiServer _mqttPassthrough(MQTT_PRINTER_PORT);

// SSDP (Simple Service Discovery Protocol) responder for Bambu Lab auto-detection.
// Bambu Studio discovers printers via SSDP on UDP port 2021.
// Real printers broadcast NOTIFY every ~10s to 239.255.255.250:2021.
// They also respond to M-SEARCH sent to multicast 239.255.255.250:2021.
static const uint16_t SSDP_PORT = 2021;
static WiFiUDP _ssdpUdp;
static bool _ssdpReady = false;
static unsigned long _ssdpLastNotify = 0;
char _displayName[32] = "BambuTagger-Gateway";

static void buildSsdpmessage(const GatewayConfig *cfg, char *buf, size_t len, bool isResponse) {
  IPAddress ipa = WiFi.localIP();
  char ip[16];
  snprintf(ip, sizeof(ip), "%d.%d.%d.%d", ipa[0], ipa[1], ipa[2], ipa[3]);
  const char *serial = cfg->gatewaySerial;
  if (isResponse) {
    snprintf(buf, len,
      "HTTP/1.1 200 OK\r\n"
      "Server: UPnP/1.0\r\n"
      "Date: Thu, 26 Jun 2025 00:00:00 GMT\r\n"
      "Location: %s\r\n"
      "ST: urn:bambulab-com:device:3dprinter:1\r\n"
      "EXT: \r\n"
      "USN: %s\r\n"
      "Cache-Control: max-age=1800\r\n"
      "DevModel.bambu.com: %s\r\n"
      "DevName.bambu.com: %s\r\n"
      "DevConnect.bambu.com: lan\r\n"
      "DevBind.bambu.com: free\r\n"
      "Devseclink.bambu.com: secure\r\n"
      "DevInf.bambu.com: wlan0\r\n"
      "DevVersion.bambu.com: 01.07.00.00\r\n"
      "DevCap.bambu.com: 1\r\n"
      "\r\n",
      ip, serial, cfg->printerModel, _displayName);
    } else {
      snprintf(buf, len,
        "NOTIFY * HTTP/1.1\r\n"
      "Host: 239.255.255.250:2021\r\n"
      "Server: UPnP/1.0\r\n"
      "Location: %s\r\n"
      "NT: urn:bambulab-com:device:3dprinter:1\r\n"
      "NTS: ssdp:alive\r\n"
      "USN: %s\r\n"
      "Cache-Control: max-age=1800\r\n"
      "DevModel.bambu.com: %s\r\n"
      "DevName.bambu.com: %s\r\n"
      "DevConnect.bambu.com: lan\r\n"
      "DevBind.bambu.com: free\r\n"
      "Devseclink.bambu.com: secure\r\n"
      "DevInf.bambu.com: wlan0\r\n"
      "DevVersion.bambu.com: 01.07.00.00\r\n"
      "DevCap.bambu.com: 1\r\n"
      "\r\n",
        ip, serial, cfg->printerModel, _displayName);
  }
}

// Start SSDP responder: bind UDP port 2021, join the SSDP multicast group,
// and respond to M-SEARCH queries from Bambu Studio.
static void startSSDP(const GatewayConfig *cfg) {
  if (_ssdpReady) return;
  if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) return;

  IPAddress mcast(239, 255, 255, 250);
  if (_ssdpUdp.beginMulticast(mcast, SSDP_PORT)) {
    Serial.println("SSDP: bound to port 2021, joined multicast 239.255.255.250");
  } else {
    // fallback: bind unicast only, send NOTIFY on broadcast
    if (!_ssdpUdp.begin(SSDP_PORT)) {
      Serial.println("SSDP: failed to bind port 2021");
      return;
    }
    Serial.println("SSDP: WARNING multicast join failed — NOTIFY-only mode");
  }

  _ssdpReady = true;
  _ssdpLastNotify = 0;
  Serial.printf("SSDP: listening on UDP port %d\n", SSDP_PORT);
}

static void stopSSDP() {
  if (_ssdpReady) {
    _ssdpUdp.stop();
    _ssdpReady = false;
  }
}

// Handle incoming SSDP messages (M-SEARCH) and send periodic NOTIFY.
static void loopSSDP(const GatewayConfig *cfg) {
  if (!_ssdpReady) return;

  unsigned long now = millis();

  // Periodic NOTIFY broadcast (real printers send every ~10s)
  if (now - _ssdpLastNotify > 10000) {
    _ssdpLastNotify = now;
    char buf[512];
    buildSsdpmessage(cfg, buf, sizeof(buf), false);
    _ssdpUdp.beginPacket(IPAddress(255, 255, 255, 255), SSDP_PORT);
    _ssdpUdp.write((const uint8_t *)buf, strlen(buf));
    _ssdpUdp.endPacket();
  }

  // Check for incoming M-SEARCH packets
  int packetSize = _ssdpUdp.parsePacket();
  if (packetSize > 0) {
    char buf[512];
    int len = _ssdpUdp.read(buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      String msg(buf);

      // Respond if this is an M-SEARCH for Bambu printers or ssdp:all
      if (msg.indexOf("M-SEARCH") >= 0 &&
          (msg.indexOf("bambu") >= 0 ||
           msg.indexOf("ssdp:all") >= 0 ||
           msg.indexOf("upnp:rootdevice") >= 0)) {
        char resp[512];
        buildSsdpmessage(cfg, resp, sizeof(resp), true);
        _ssdpUdp.beginPacket(_ssdpUdp.remoteIP(), _ssdpUdp.remotePort());
        _ssdpUdp.write((const uint8_t *)resp, strlen(resp));
        _ssdpUdp.endPacket();
        Serial.printf("SSDP: responded to M-SEARCH from %s\n",
                      _ssdpUdp.remoteIP().toString().c_str());
      }
    }
  }
}

static void startAP(uint8_t mode = WIFI_AP_STA) {
  WiFi.persistent(false);
  WiFi.mode((WiFiMode_t)mode);
  delay(100);
  WiFi.setSleep(false);
  delay(100);
  if (!WiFi.softAP("BambuTagger-Gateway")) {
    Serial.println("AP FAILED");
    return;
  }
  Serial.printf("AP: %s  IP: %s  mode: %d\n",
    "BambuTagger-Gateway",
    WiFi.softAPIP().toString().c_str(),
    WiFi.getMode());
}

static void manageAP() {
  bool staConnected = WiFi.isConnected();
  auto mode = WiFi.getMode();

  if (staConnected && mode != WIFI_STA) {
    WiFi.mode(WIFI_STA);
  } else if (!staConnected && mode != WIFI_AP_STA && strlen(webconfigGetConfig()->stationSsid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    startAP();
    Serial.println("Station lost — AP restarted");
  }
}

// XIAO ESP32-S3: built-in LED on GPIO21, active LOW (LED_BUILTIN from variant).
#ifndef LED_BUILTIN
#define LED_BUILTIN 21
#endif

static void ledSetup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);  // on during boot
}

static void ledLoop() {
  static unsigned long last = 0;
  unsigned long now = millis();
  unsigned long interval;

  bool staConfig = strlen(webconfigGetConfig()->stationSsid) > 0;
  bool staUp = WiFi.isConnected();
  MqttStatus st = ::mqtt.getStatus();

  if (!staUp && staConfig) {
    interval = 500;     // slow blink — WiFi connecting
  } else if (staUp && st != MQTT_UP) {
    interval = 100;     // fast blink — MQTT connecting
  } else {
    digitalWrite(LED_BUILTIN, HIGH);  // off — everything up
    return;
  }

  if (now - last >= interval) {
    last = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void setup() {
  Serial.begin(115200);
  // ESP32-S3 native USB CDC needs time to enumerate before any output
  delay(3000);
  Serial.println("\n\nBambuTagger Gateway " VERSION);

  ledSetup();

  // Load config first (EEPROM only, no lwIP needed)
  webconfigInit();
  GatewayConfig *cfg = webconfigGetConfig();

  // Start WiFi in AP or AP+STA mode depending on whether station is configured.
  if (strlen(cfg->stationSsid) > 0) {
    startAP(WIFI_AP_STA);
    Serial.printf("Connecting to WiFi: %s\n", cfg->stationSsid);
    WiFi.begin(cfg->stationSsid, cfg->stationPass);
  } else {
    startAP();
    Serial.println("No station configured — AP only");
  }

  // Now start TCP servers (lwIP is initialized by WiFi in either mode)
  webconfigBegin();
  delay(1000);

  // apply runtime config to all bridges
  mqtt.begin(cfg);
  camProxy.setRemote(cfg->printerHost, CAM_PRINTER_PORT);
  ftpsProxy.setRemote(cfg->printerHost, FTPS_PRINTER_PORT);

  Serial.printf("Web UI at http://%s (initial AP, moves to STA)\n", WiFi.softAPIP().toString().c_str());
  Serial.print("Printer: ");
  Serial.print(cfg->printerHost);
  Serial.print("  Serial: ");
  Serial.println(cfg->printerSerial);

  camProxy.begin();
  ftpsProxy.begin();
  _mqttPassthrough.begin();
  _mqttPassthrough.setNoDelay(true);
  Serial.printf("MQTT passthrough: TCP relay on port %d\n", MQTT_PRINTER_PORT);

  Serial.printf("Gateway AP: %s\n", GATEWAY_AP_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
  Serial.println("Ready.");
}

static void startStationServices(GatewayConfig *cfg) {
  Serial.printf("Station connected — IP: %s  heap: %u\n",
    WiFi.localIP().toString().c_str(), ESP.getFreeHeap());
  Serial.printf("Web UI at http://%s\n", WiFi.localIP().toString().c_str());

  delay(500);
  // Rebind servers after WiFi mode switch (AP → STA) ensures listeners
  // are bound to the active station interface.
  mqtt.rebind();
  _mqttPassthrough.close();
  _mqttPassthrough.begin();
  _mqttPassthrough.setNoDelay(true);

  // Verify the MQTT server is listening on the station IP
  delay(100);
  WiFiClient selfTest;
  IPAddress local = WiFi.localIP();
  if (selfTest.connect(local, MQTT_LOCAL_PORT)) {
    Serial.printf("MQTT: self-test OK — connected to %s:%d\n",
      local.toString().c_str(), MQTT_LOCAL_PORT);
    selfTest.stop();
  } else {
    Serial.printf("MQTT: WARNING self-test FAILED — %s:%d unreachable\n",
      local.toString().c_str(), MQTT_LOCAL_PORT);
  }

  startSSDP(cfg);

  Serial.printf("After SSDP — heap: %u\n", ESP.getFreeHeap());

  char mdnsName[64];
  // Printer-like hostname: 3DP-<first 3 of gateway id>-<last 3 of gateway id>
  size_t gwl = strlen(cfg->gatewaySerial);
  snprintf(mdnsName, sizeof(mdnsName), "3DP-%.3s-%.3s",
    cfg->gatewaySerial,
    gwl >= 3 ? cfg->gatewaySerial + gwl - 3 : cfg->gatewaySerial);
  strlcpy(_displayName, mdnsName, sizeof(_displayName));
  if (MDNS.begin(mdnsName)) {
    MDNS.addService("bambu", "tcp", MQTT_PRINTER_PORT);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"product", (const char *)cfg->printerModel);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"serial", (const char *)cfg->printerSerial);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"fw_version", (const char *)VERSION);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"cloud", (const char *)"disable");
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"usb", (const char *)"no");
  }

  ArduinoOTA.setHostname(mdnsName);
  ArduinoOTA.onStart([]() { Serial.println("OTA: start"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA: end"); });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("OTA: error %d\n", err);
  });
  ArduinoOTA.begin();

  Serial.printf("After MDNS+OTA — heap: %u\n", ESP.getFreeHeap());
}

static void handleMqttPassthrough() {
  GatewayConfig *cfg = webconfigGetConfig();
  bool hasRemote = strlen(cfg->printerHost) > 0;

  // Accept new connections on port 8883
  WiFiClient *down = new WiFiClient(_mqttPassthrough.accept());
  if (down && *down && down->connected()) {
    if (!hasRemote) { down->stop(); delete down; return; }
    WiFiClient *up = new WiFiClient;
    up->setTimeout(5000);
    if (up->connect(cfg->printerHost, MQTT_PRINTER_PORT)) {
      up->setNoDelay(true);
      for (int i = 0; i < MAX_MQTT_PASSTHROUGH; i++) {
        if (!_mqttPtClients[i].active) {
          _mqttPtClients[i].down = down;
          _mqttPtClients[i].up = up;
          _mqttPtClients[i].active = true;
          _mqttPtClients[i].deadCount = 0;
          _mqttPtClients[i].relayed = false;
          Serial.printf("PASSTHROUGH: client %d from %s\n", i,
            down->remoteIP().toString().c_str());
          break;
        }
      }
    } else {
      down->stop(); delete down;
      delete up;
    }
  } else {
    delete down;
  }

  // Relay bytes bidirectionally for each active client
  for (int i = 0; i < MAX_MQTT_PASSTHROUGH; i++) {
    if (!_mqttPtClients[i].active) continue;
    auto &c = _mqttPtClients[i];
    const char *why = nullptr;

    // Forward downstream → upstream
    size_t davail = c.down->available();
    if (davail > 0) {
      uint8_t buf[512];
      int len = c.down->read(buf, davail > sizeof(buf) ? sizeof(buf) : davail);
      if (len > 0) {
        if (!c.relayed) {
          Serial.printf("PASSTHROUGH: cl%d DOWN first %d bytes:", i, len);
          for (int j = 0; j < len && j < 32; j++) Serial.printf(" %02x", buf[j]);
          Serial.println();
        }
        c.relayed = true;
        c.up->write(buf, len);
      }
    }

    // Forward upstream → downstream
    size_t uavail = c.up->available();
    if (uavail > 0) {
      uint8_t buf[512];
      int len = c.up->read(buf, uavail > sizeof(buf) ? sizeof(buf) : uavail);
      if (len > 0) {
        c.relayed = true;
        c.down->write(buf, len);
      }
    }

    // Cleanup: idle timeout (no data for 60s) or connection errors
    if (davail == 0 && uavail == 0) {
      if (!c.down->connected()) {
        c.deadCount++;
        if (c.deadCount >= 3) why = "down closed";
      }
    } else {
      c.deadCount = 0;
    }

    if (why) {
      Serial.printf("PASSTHROUGH: client %d disconnected (%s) relayed=%d\n",
        i, why, c.relayed);
      c.down->stop(); delete c.down; c.down = nullptr;
      c.up->stop(); delete c.up; c.up = nullptr;
      c.active = false;
    }
  }
}

void loop() {
  webconfigLoop();
  manageAP();

  // Debug AP status every 2 seconds
  static int lastStaCount = -1;
  static int lastMode = -1;
  static int lastStatus = -1;
  static unsigned long lastDbg = 0;
  unsigned long now = millis();
  int sc = WiFi.softAPgetStationNum();
  int mo = WiFi.getMode();
  int ws = WiFi.status();
  if ((sc != lastStaCount || mo != lastMode || ws != lastStatus || now - lastDbg > 5000) && now - lastDbg > 500) {
    lastStaCount = sc;
    lastMode = mo;
    lastStatus = ws;
    lastDbg = now;
    // Quiet once station is stably connected with AP off
    if (mo != 1 || ws != 3) {
      Serial.printf("mode=%d AP=%s sta=%d staCon=%d wl=%d heap=%u\n",
        mo, (mo == 2 || mo == 3) ? WiFi.softAPIP().toString().c_str() : "-",
        sc, WiFi.isConnected(), ws, ESP.getFreeHeap());
    }
  }

  // Start station services (MDNS, OTA, SSDP) on first station connection
  // Must happen before bridge loops so MDNS is available for hostname resolution
  GatewayConfig *cfg = webconfigGetConfig();
  static bool staServicesStarted = false;
  if (!staServicesStarted && WiFi.isConnected()) {
    staServicesStarted = true;
    startStationServices(cfg);
  }

  mqtt.loop();
  handleMqttPassthrough();
  camProxy.loop();
  ftpsProxy.loop();
  ArduinoOTA.handle();
  ledLoop();

  loopSSDP(cfg);
}
