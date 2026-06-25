#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "ui/webconfig.h"
#include "mqtt_bridge.h"
#include "tcp_proxy.h"

MqttBridge mqtt;
TcpProxy camProxy("camera", CAM_LOCAL_PORT, PRINTER_HOST_DFLT, CAM_PRINTER_PORT, MAX_TCP_CLIENTS);
TcpProxy ftpsProxy("ftps", FTPS_LOCAL_PORT, PRINTER_HOST_DFLT, FTPS_PRINTER_PORT, MAX_TCP_CLIENTS);

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

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nBambuTagger Gateway " VERSION);

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

  if (MDNS.begin("BambuTagger-Gateway")) {
    MDNS.addService("mqtt", "tcp", MQTT_LOCAL_PORT);
    MDNS.addService("http", "tcp", 80);
  }

  // OTA update server
  ArduinoOTA.setHostname("BambuTagger-Gateway");
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
}
