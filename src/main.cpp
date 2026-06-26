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

// SSDP (Simple Service Discovery Protocol) responder for Bambu Lab auto-detection.
// Bambu Studio discovers printers via SSDP on UDP port 2021.
// Real printers broadcast NOTIFY every ~10s to 255.255.255.255:2021.
// They also respond to M-SEARCH sent to multicast 239.255.255.250:2021.
static const uint16_t SSDP_PORT = 2021;
static WiFiUDP _ssdpUdp;
static bool _ssdpReady = false;

static void buildSsdpmessage(const GatewayConfig *cfg, char *buf, size_t len, bool isResponse) {
  IPAddress ipa = WiFi.localIP();
  char ip[16];
  snprintf(ip, sizeof(ip), "%d.%d.%d.%d", ipa[0], ipa[1], ipa[2], ipa[3]);
  if (isResponse) {
    snprintf(buf, len,
      "HTTP/1.1 200 OK\r\n"
      "Server: UPnP/1.0\r\n"
       "Location: http://%s:2021/\r\n"
       "ST: urn:bambulab-com:device:3dprinter:1\r\n"
       "USN: %s\r\n"
      "Cache-Control: max-age=1800\r\n"
      "DevModel.bambu.com: %s\r\n"
      "DevName.bambu.com: BambuTagger-Gateway\r\n"
      "DevSignal.bambu.com: -44\r\n"
      "DevConnect.bambu.com: lan\r\n"
      "DevBind.bambu.com: free\r\n"
       "Devseclink.bambu.com: lan\r\n"
       "DevInf.bambu.com: eth0\r\n"
       "DevVersion.bambu.com: 01.07.00.00\r\n"
       "DevCap.bambu.com: 1\r\n"
       "\r\n",
       ip, cfg->gatewaySerial, cfg->printerModel);
   } else {
     snprintf(buf, len,
       "NOTIFY * HTTP/1.1\r\n"
       "Host: 239.255.255.250:2021\r\n"
      "Server: UPnP/1.0\r\n"
       "Location: http://%s:2021/\r\n"
       "NT: urn:bambulab-com:device:3dprinter:1\r\n"
       "NTS: ssdp:alive\r\n"
       "USN: %s\r\n"
      "Cache-Control: max-age=1800\r\n"
      "DevModel.bambu.com: %s\r\n"
      "DevName.bambu.com: BambuTagger-Gateway\r\n"
      "DevSignal.bambu.com: -44\r\n"
      "DevConnect.bambu.com: lan\r\n"
      "DevBind.bambu.com: free\r\n"
       "Devseclink.bambu.com: lan\r\n"
       "DevInf.bambu.com: eth0\r\n"
       "DevVersion.bambu.com: 01.07.00.00\r\n"
       "DevCap.bambu.com: 1\r\n"
       "\r\n",
       ip, cfg->gatewaySerial, cfg->printerModel);
 }
}

// Start SSDP responder: bind UDP port 2021, join the SSDP multicast group,
// and respond to M-SEARCH queries from Bambu Studio.
static void startSSDP(const GatewayConfig *cfg) {
  if (_ssdpReady) return;
  IPAddress local = WiFi.localIP();
  if (local == IPAddress(0, 0, 0, 0)) return;

  if (!_ssdpUdp.begin(SSDP_PORT)) {
    Serial.println("SSDP: failed to bind port 2021");
    return;
  }

  // Join the SSDP multicast group (239.255.255.250) so we hear M-SEARCH queries
  // from Bambu Studio.
  IPAddress mcast(239, 255, 255, 250);
  ip4_addr_t ifaddr;
  ifaddr.addr = (uint32_t)local;
  ip4_addr_t mcaddr;
  mcaddr.addr = (uint32_t)mcast;
  if (igmp_joingroup(&ifaddr, &mcaddr) == ERR_OK) {
    Serial.println("SSDP: joined multicast 239.255.255.250");
  } else {
    Serial.println("SSDP: WARNING multicast join failed — NOTIFY-only mode");
  }

  _ssdpReady = true;
  Serial.printf("SSDP: listening on UDP port %d\n", SSDP_PORT);
}

static void stopSSDP() {
  if (_ssdpReady) {
    _ssdpUdp.stop();
    _ssdpReady = false;
  }
}

// Handle incoming SSDP messages (M-SEARCH) and respond.
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
}

static void startAP(uint8_t mode = WIFI_AP) {
  WiFi.persistent(false);
  Serial.println("Resetting WiFi…");
  WiFi.mode(WIFI_OFF);
  delay(1500);
  WiFi.mode((WiFiMode_t)mode);
  delay(300);
  esp_wifi_set_ps(WIFI_PS_NONE);
  delay(100);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  delay(50);
  if (WiFi.softAP("BambuTagger-Gateway", NULL, 6, 0, 4)) {
    Serial.printf("AP started: ch6 IP %s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("AP FAILED to start");
  }
  Serial.printf("WiFi mode: %d\n", WiFi.getMode());
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
    WiFi.begin(cfg->stationSsid, cfg->stationPass);
  } else {
    startAP();
  }

  // Now start TCP servers (lwIP is initialized by WiFi in either mode)
  webconfigBegin();
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

  Serial.printf("Gateway AP: %s\n", GATEWAY_AP_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
  Serial.println("Ready.");
}

static void startStationServices(GatewayConfig *cfg) {
  Serial.printf("Station connected — IP: %s  AP stopped  heap: %u\n",
    WiFi.localIP().toString().c_str(), ESP.getFreeHeap());

  delay(500);
  startSSDP(cfg);

  Serial.printf("After SSDP — heap: %u\n", ESP.getFreeHeap());

  char mdnsName[64];
  strcpy(mdnsName, "BambuTagger-Gateway");
  if (MDNS.begin(mdnsName)) {
    MDNS.addService("mqtt", "tcp", MQTT_LOCAL_PORT);
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("bambu", "tcp", MQTT_PRINTER_PORT);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"serial", (const char *)cfg->printerSerial);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"model", (const char *)cfg->printerModel);
    MDNS.addServiceTxt((const char *)"bambu", (const char *)"tcp", (const char *)"version", (const char *)VERSION);
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

void loop() {
  webconfigLoop();
  manageAP();

  // Debug AP status every 10 seconds
  static unsigned long lastApDbg = 0;
  unsigned long now = millis();
  if (now - lastApDbg > 10000) {
    lastApDbg = now;
    Serial.printf("AP: mode=%d IP=%s stations=%d\n",
      WiFi.getMode(), WiFi.softAPIP().toString().c_str(),
      WiFi.softAPgetStationNum());
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
  camProxy.loop();
  ftpsProxy.loop();
  ArduinoOTA.handle();
  ledLoop();

  loopSSDP(cfg);
}
