#pragma once

#include <ESP8266WiFi.h>
#include "config.h"

struct ProxyClient {
  WiFiClient *local;
  bool active;
};

class TcpProxy {
public:
  TcpProxy(const char *name, uint16_t localPort, const char *remoteHost,
           uint16_t remotePort, uint8_t maxClients);
  void begin();
  void loop();
  void setRemote(const char *host, uint16_t port);

private:
  const char *_name;
  uint16_t _localPort, _remotePort;
  const char *_remoteHost;
  uint8_t _maxClients;
  WiFiClient _upstream;
  WiFiServer _server;
  ProxyClient *_clients;
  unsigned long _lastReconnect;

  bool _disconnected;
  bool connectUpstream();
  int acceptClient();
  void disconnectClient(int idx);
  void disconnectAll();
  void forwardUpToDown();
  void forwardDownToUp(int idx);
};
