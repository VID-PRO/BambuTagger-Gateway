#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <lwip/igmp.h>

#include "config.h"
#include "ui/webconfig.h"
#include "mqtt_bridge.h"
#include "tcp_proxy.h"

MqttBridge mqtt;
TcpProxy camProxy("camera", CAM_LOCAL_PORT, PRINTER_HOST_DFLT, CAM_PRINTER_PORT, MAX_TCP_CLIENTS);
TcpProxy ftpsProxy("ftps", FTPS_LOCAL_PORT, PRINTER_HOST_DFLT, FTPS_PRINTER_PORT, MAX_TCP_CLIENTS);

// SSDP (Simple Service Discovery Protocol) responder for Bambu Lab auto-detection.
// Bambu Studio discovers printers via SSDP on UDP port 2021.
// Real printers broadcast NOTIFY every ~10s to 255.255.255.255:2021.
// They also respond to M-SEARCH sent to multicast 239.255.255.250:2021.
static const uint16_t SSDP_PORT = 2021;
static WiFiUDP _ssdpUdp;
static bool _ssdpReady = false;
static unsigned long _lastNotify = 0;
static const unsigned long NOTIFY_INTERVAL = 10000;

static void buildSsdpmessage(const GatewayConfig *cfg, char *buf, size_t len, bool isResponse) {
  IPAddress ipa = WiFi.localIP();
  char ip[16];
  snprintf(ip, sizeof(ip), "%d.%d.%d.%d", ipa[0], ipa[1], ipa[2], ipa[3]);
  if (isResponse) {
    snprintf(buf, len,
      "HTTP/1.1 200 OK\r\n"
      "Server: UPnP/1.0\r\n"
      "Location: %s\r\n"
      "ST: urn:bambulab-com:device:3dprinter:1\r\n"
      "USN: %s\r\n"
      "Cache-Control: max-age=1800\r\n"
      "DevModel.bambu.com: %s\r\n"
      "DevName.bambu.com: BambuTagger-Gateway\r\n"
      "DevSignal.bambu.com: -44\r\n"
      "DevConnect.bambu.com: lan\r\n"
      "DevBind.bambu.com: free\r\n"
      "Devseclink.bambu.com: secure\r\n"
      "DevInf.bambu.com: eth0\r\n"
      "DevVersion.bambu.com: 01.07.00.00\r\n"
      "DevCap.bambu.com: 1\r\n"
      "\r\n",
      ip, cfg->gatewaySerial, cfg->printerModel);
  } else {
    snprintf(buf, len,
      "NOTIFY * HTTP/1.1\r\n"
      "Host: 239.255.255.250:1990\r\n"
      "Server: UPnP/1.0\r\n"
      "Location: %s\r\n"
      "NT: urn:bambulab-com:device:3dprinter:1\r\n"
      "NTS: ssdp:alive\r\n"
      "USN: %s\r\n"
      "Cache-Control: max-age=1800\r\n"
      "DevModel.bambu.com: %s\r\n"
      "DevName.bambu.com: BambuTagger-Gateway\r\n"
      "DevSignal.bambu.com: -44\r\n"
      "DevConnect.bambu.com: lan\r\n"
      "DevBind.bambu.com: free\r\n"
      "Devseclink.bambu.com: secure\r\n"
      "DevInf.bambu.com: eth0\r\n"
      "DevVersion.bambu.com: 01.07.00.00\r\n"
      "DevCap.bambu.com: 1\r\n"
      "\r\n",
      ip, cfg->gatewaySerial, cfg->printerModel);
  }
}

// Send SSDP NOTIFY alive announcement via broadcast.
static void sendSSDPNotify(const GatewayConfig *cfg) {
  if (!_ssdpReady) return;
  char msg[512];
  buildSsdpmessage(cfg, msg, sizeof(msg), false);
  IPAddress bcast(255, 255, 255, 255);
  _ssdpUdp.beginPacket(bcast, SSDP_PORT);
  _ssdpUdp.write((const uint8_t *)msg, strlen(msg));
  _ssdpUdp.endPacket();
  IPAddress ipa = WiFi.localIP();
  Serial.printf("SSDP: NOTIFY %s (%s) on %d.%d.%d.%d\n",
    cfg->gatewaySerial, cfg->printerModel, ipa[0], ipa[1], ipa[2], ipa[3]);
}

// Start SSDP responder: bind UDP port 2021, join the SSDP multicast group,
// and send initial NOTIFY broadcasts for quick discovery.
static void startSSDP(const GatewayConfig *cfg) {
  if (_ssdpReady) return;
  IPAddress local = WiFi.localIP();
  if (local == IPAddress(0, 0, 0, 0)) return;

  if (!_ssdpUdp.begin(SSDP_PORT)) {
    Serial.println("SSDP: failed to bind port 2021");
    return;
  }

  // Join the SSDP multicast group (239.255.255.250) so we hear M-SEARCH queries
  // from Bambu Studio.  This mirrors the approach used by the ESP8266SSDP library.
  IPAddress mcast(239, 255, 255, 250);
  if (igmp_joingroup(local, mcast) == ERR_OK) {
    Serial.println("SSDP: joined multicast 239.255.255.250");
  } else {
    Serial.println("SSDP: WARNING multicast join failed — NOTIFY-only mode");
  }

  _ssdpReady = true;
  Serial.printf("SSDP: listening on UDP port %d\n", SSDP_PORT);

  // Rapid initial NOTIFY bursts for quick discovery
  for (int i = 0; i < 2; i++) {
    sendSSDPNotify(cfg);
    delay(200);
  }
  _lastNotify = millis();
}

static void stopSSDP() {
  if (_ssdpReady) {
    _ssdpUdp.stop();
    _ssdpReady = false;
  }
}

// Handle incoming SSDP messages (M-SEARCH) and send periodic NOTIFY broadcasts.
static void loopSSDP(const GatewayConfig *cfg) {
  if (!_ssdpReady) return;

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
          (msg.indexOf("urn:bambulab-com:device:3dprinter:1") >= 0 ||
           msg.indexOf("ssdp:all") >= 0)) {
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

  // Send periodic NOTIFY broadcasts
  unsigned long now = millis();
  if (now - _lastNotify >= NOTIFY_INTERVAL) {
    _lastNotify = now;
    sendSSDPNotify(cfg);
  }
}

static void startAP() {
  WiFi.softAPConfig(GATEWAY_AP_IP, GATEWAY_AP_IP, GATEWAY_AP_SUBNET);
  WiFi.softAP(GATEWAY_AP_SSID, GATEWAY_AP_PASS, GATEWAY_AP_CHANNEL);
}

static void setupWiFi(GatewayConfig *cfg) {
  WiFi.mode(WIFI_AP_STA);
  startAP();

  if (strlen(cfg->stationSsid) > 0) {
    WiFi.begin(cfg->stationSsid, cfg->stationPass);
  }
}

static void manageAP() {
  bool staConnected = WiFi.isConnected();
  WiFiMode_t mode = WiFi.getMode();

  if (staConnected && mode != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    Serial.print("Station connected — IP: ");
    Serial.print(WiFi.localIP());
    Serial.println("  AP stopped");
  } else if (!staConnected && mode != WIFI_AP_STA && strlen(webconfigGetConfig()->stationSsid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    startAP();
    Serial.println("Station lost — AP restarted");
  }
}

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
  delay(500);
  Serial.println("\n\nBambuTagger Gateway " VERSION);

  ledSetup();

  // load runtime config from EEPROM
  webconfigBegin();
  GatewayConfig *cfg = webconfigGetConfig();

  setupWiFi(cfg);
  delay(1000);

  // apply runtime config to all bridges
  mqtt.begin(cfg);
  camProxy.setRemote(cfg->printerHost, CAM_PRINTER_PORT);
  ftpsProxy.setRemote(cfg->printerHost, FTPS_PRINTER_PORT);

  Serial.print("Web UI at http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Printer: ");
  Serial.print(cfg->printerHost);
  Serial.print("  Serial: ");
  Serial.println(cfg->printerSerial);

  camProxy.begin();
  ftpsProxy.begin();

  char mdnsName[64];
  strcpy(mdnsName, "BambuTagger-Gateway");
  if (MDNS.begin(mdnsName)) {
    MDNS.addService("mqtt", "tcp", MQTT_LOCAL_PORT);
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("bambu", "tcp", MQTT_PRINTER_PORT);
    MDNS.addServiceTxt("bambu", "tcp", "serial", cfg->printerSerial);
    MDNS.addServiceTxt("bambu", "tcp", "model", cfg->printerModel);
    MDNS.addServiceTxt("bambu", "tcp", "version", VERSION);
  }

  // OTA update server
  ArduinoOTA.setHostname(mdnsName);
  ArduinoOTA.onStart([]() { Serial.println("OTA: start"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA: end"); });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("OTA: error %d\n", err);
  });
  ArduinoOTA.begin();

  Serial.print("Gateway AP: ");
  Serial.println(GATEWAY_AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  if (WiFi.isConnected()) {
    Serial.print("Station IP: ");
    Serial.println(WiFi.localIP());
    startSSDP(cfg);
  }
  Serial.println("Ready.");
}

void loop() {
  webconfigLoop();
  manageAP();
  mqtt.loop();
  camProxy.loop();
  ftpsProxy.loop();
  ArduinoOTA.handle();
  MDNS.update();
  ledLoop();

  GatewayConfig *cfg = webconfigGetConfig();

  // Manage SSDP lifecycle: start on station connect, stop on disconnect.
  // Small delay to let the WiFi stack settle after (re)connecting.
  static bool wasStaUp = false;
  bool staUp = WiFi.isConnected();
  if (staUp && !wasStaUp) {
    delay(500);
    startSSDP(cfg);
  } else if (!staUp && wasStaUp) {
    stopSSDP();
  }
  wasStaUp = staUp;

  loopSSDP(cfg);
}
