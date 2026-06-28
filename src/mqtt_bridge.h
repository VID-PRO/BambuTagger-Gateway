#pragma once

#ifdef ESP32
#include <WiFi.h>
#include <WiFiClientSecure.h>
#else
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#endif
#include <PubSubClient.h>
#include "config.h"
#ifdef ESP32
#include "certgen.h"
#include "tls_client.h"
#endif

struct MqttSub {
  String topic;
  uint8_t qos;
};

struct MqttClientCtx {
  WiFiClient *client;
  String clientId;
  MqttSub subs[MAX_MQTT_CLIENTS];
  uint8_t subCount;
  uint16_t lastPid;
  bool active;
  bool isTls;        // client is TlsWiFiClient with pending/ongoing TLS
  unsigned long lastActivity;
};

enum MqttStatus { MQTT_IDLE, MQTT_TRYING, MQTT_UP };

class MqttBridge {
public:
  MqttBridge();
  void begin(GatewayConfig *cfg);
  void rebind();
  void loop();
  bool isConnected();
  MqttStatus getStatus();

  // TLS certificate PEM (for web UI download)
  const char *getTlsCert();
  const uint8_t *getCertDer() const { return _certDer; }
  size_t getCertLen() const { return _certLen; }
  const uint8_t *getKeyDer() const { return _keyDer; }
  size_t getKeyLen() const { return _keyLen; }
  const uint8_t *getCaDer() const { return _caDer; }
  size_t getCaLen() const { return _caLen; }

private:
  WiFiClientSecure *_upTcp = nullptr;
  PubSubClient _pubsub;
  WiFiServer _localServer;
  WiFiServer _tlsServer;
  MqttClientCtx _clients[MAX_MQTT_CLIENTS];
  unsigned long _lastReconnect;
  GatewayConfig *_cfg;

#ifdef ESP32
  uint8_t _certDer[3072];  // concatenated server + CA DER chain
  uint8_t _keyDer[2048];   // server private key DER
  uint8_t _caDer[2048];    // CA cert DER (for printer.cer)
  size_t _certLen = 0;
  size_t _keyLen = 0;
  size_t _caLen = 0;
  char _certPem[4096];     // chain PEM
  char _caPem[3072];       // CA PEM (for /ca.pem / printer.cer)
#endif

  // upstream
  bool connectUpstream();
  void onUpstreamMessage(char *topic, uint8_t *payload, unsigned int len);

  // downstream
  int acceptClient();
  void disconnectClient(int idx);
  void handleClient(int idx);

  // MQTT wire helpers
  static bool readByte(WiFiClient &c, uint8_t &b);
  static bool readRemainingLength(WiFiClient &c, uint32_t &len);
  static bool readBytes(WiFiClient &c, uint8_t *buf, uint32_t len);
  static void writeRemainingLength(WiFiClient &c, uint32_t len);

  void sendConnAck(WiFiClient &c, bool sp, uint8_t rc);
  void sendSubAck(WiFiClient &c, uint16_t pid, uint8_t count);
  void sendUnsubAck(WiFiClient &c, uint16_t pid);
  void sendPingResp(WiFiClient &c);
  void sendPublish(WiFiClient &c, const String &topic, const uint8_t *payload,
                   uint32_t len, uint8_t qos);

};
