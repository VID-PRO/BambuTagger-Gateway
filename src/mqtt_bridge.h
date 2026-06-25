#pragma once

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

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
  unsigned long lastActivity;
};

enum MqttStatus { MQTT_IDLE, MQTT_TRYING, MQTT_UP };

class MqttBridge {
public:
  MqttBridge();
  void begin(GatewayConfig *cfg);
  void loop();
  bool isConnected();
  MqttStatus getStatus();

private:
  WiFiClientSecure *_upTcp = nullptr;
  PubSubClient _pubsub;
  WiFiServer _localServer;
  MqttClientCtx _clients[MAX_MQTT_CLIENTS];
  unsigned long _lastReconnect;
  GatewayConfig *_cfg;

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

  // MQTT packet construction
  void sendConnAck(WiFiClient &c, bool sp, uint8_t rc);
  void sendSubAck(WiFiClient &c, uint16_t pid, uint8_t count);
  void sendUnsubAck(WiFiClient &c, uint16_t pid);
  void sendPingResp(WiFiClient &c);
  void sendPublish(WiFiClient &c, const String &topic, const uint8_t *payload,
                   uint32_t len, uint8_t qos);

};
