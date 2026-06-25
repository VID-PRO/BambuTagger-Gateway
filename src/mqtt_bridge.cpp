#include "mqtt_bridge.h"

static char reportTopic[64];
static char requestTopic[64];
static char localReportTopic[64];
static char localRequestTopic[64];

MqttBridge::MqttBridge()
  : _localServer(MQTT_LOCAL_PORT), _lastReconnect(0), _cfg(nullptr) {
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    _clients[i].active = false;
    _clients[i].client = nullptr;
    _clients[i].subCount = 0;
    _clients[i].lastPid = 0;
    _clients[i].lastActivity = 0;
  }

  snprintf(reportTopic, sizeof(reportTopic), MQTT_REPORT_TOPIC, PRINTER_SERIAL_DFLT);
  snprintf(requestTopic, sizeof(requestTopic), MQTT_REQUEST_TOPIC, PRINTER_SERIAL_DFLT);
  snprintf(localReportTopic, sizeof(localReportTopic), MQTT_REPORT_TOPIC, PRINTER_SERIAL_DFLT);
  snprintf(localRequestTopic, sizeof(localRequestTopic), MQTT_REQUEST_TOPIC, PRINTER_SERIAL_DFLT);

  _pubsub.setClient(*_upTcp);
  _pubsub.setBufferSize(MQTT_BUFFER_SIZE);
  _pubsub.setCallback([this](char *t, uint8_t *p, unsigned int l) {
    onUpstreamMessage(t, p, l);
  });
}

void MqttBridge::begin(GatewayConfig *cfg) {
  _cfg = cfg;

  // update topic buffers from runtime config
  snprintf(reportTopic, sizeof(reportTopic), MQTT_REPORT_TOPIC, _cfg->printerSerial);
  snprintf(requestTopic, sizeof(requestTopic), MQTT_REQUEST_TOPIC, _cfg->printerSerial);
  snprintf(localReportTopic, sizeof(localReportTopic), MQTT_REPORT_TOPIC, _cfg->gatewaySerial);
  snprintf(localRequestTopic, sizeof(localRequestTopic), MQTT_REQUEST_TOPIC, _cfg->gatewaySerial);

  _localServer.begin();
  _localServer.setNoDelay(true);
}

void MqttBridge::loop() {
  if (!_pubsub.connected()) {
    // Don't attempt upstream MQTT until printer is configured and station WiFi is up
    if (!WiFi.isConnected()) return;
    if (_cfg && strlen(_cfg->printerHost) == 0) return;

    unsigned long now = millis();
    if (now - _lastReconnect > 5000) {
      _lastReconnect = now;
      if (connectUpstream()) {
        for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
          if (_clients[i].active) {
            for (uint8_t s = 0; s < _clients[i].subCount; s++) {
              _pubsub.subscribe(_clients[i].subs[s].topic.c_str(),
                                _clients[i].subs[s].qos);
            }
          }
        }
      }
    }
  } else {
    _pubsub.loop();
  }

  while (acceptClient() >= 0) {}

  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!_clients[i].active) continue;
    if (!_clients[i].client->connected()) {
      disconnectClient(i);
      continue;
    }
    if (_clients[i].client->available()) {
      handleClient(i);
    }
  }
}

bool MqttBridge::isConnected() {
  return _pubsub.connected();
}

MqttStatus MqttBridge::getStatus() {
  if (_pubsub.connected()) return MQTT_UP;
  // Printer is on LAN — unreachable when station WiFi is down
  if (!WiFi.isConnected()) return MQTT_IDLE;
  if (_cfg && strlen(_cfg->printerHost) > 0) {
    return MQTT_TRYING;
  }
  return MQTT_IDLE;
}

// ------------------------------------------------------------------
// upstream
// ------------------------------------------------------------------
bool MqttBridge::connectUpstream() {
  if (!_cfg) return false;
  delete _upTcp;
  _upTcp = new WiFiClientSecure();
  _upTcp->setInsecure();
  _upTcp->setTimeout(10000);
  if (!_upTcp->connect(_cfg->printerHost, MQTT_PRINTER_PORT)) {
    delete _upTcp; _upTcp = nullptr;
    return false;
  }
  _pubsub.setClient(*_upTcp);

  char willTopic[64];
  snprintf(willTopic, sizeof(willTopic), "device/%s/status", _cfg->printerSerial);

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "gateway-%06x", ESP.getChipId());

  bool ok = _pubsub.connect(clientId, "bblp", _cfg->printerCode,
                            willTopic, 1, true, "offline");
  if (ok) {
    _pubsub.publish(willTopic, "online", true);
  }
  return ok;
}

static bool topicMatchesSub(const String &topic, const String &sub) {
  if (sub == topic || sub == "#") return true;
  if (sub.endsWith("/#")) {
    String prefix = sub.substring(0, sub.length() - 2);
    if (topic == prefix) return true;
    if (topic.startsWith(prefix + "/")) return true;
  }
  if (sub.endsWith("/+")) {
    String prefix = sub.substring(0, sub.length() - 2);
    String suffix;
    if (prefix.length() == 0) {
      suffix = topic;
    } else {
      if (!topic.startsWith(prefix + "/")) return false;
      suffix = topic.substring(prefix.length() + 1);
    }
    if (suffix.indexOf('/') == -1 && suffix.length() > 0) return true;
  }
  return false;
}

void MqttBridge::onUpstreamMessage(char *topic, uint8_t *payload, unsigned int len) {
  // Translate upstream topic (printer serial) to local topic (gateway serial)
  String t = topic;
  if (_cfg && strcmp(_cfg->printerSerial, _cfg->gatewaySerial) != 0) {
    String prefix = String("device/") + _cfg->printerSerial;
    if (t.startsWith(prefix)) {
      t = String("device/") + _cfg->gatewaySerial + t.substring(prefix.length());
    }
  }
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!_clients[i].active) continue;
    for (uint8_t s = 0; s < _clients[i].subCount; s++) {
      if (topicMatchesSub(t, _clients[i].subs[s].topic)) {
        sendPublish(*_clients[i].client, t, payload, len, 0);
        break;
      }
    }
  }
}

// ------------------------------------------------------------------
// downstream client management
// ------------------------------------------------------------------
int MqttBridge::acceptClient() {
  WiFiClient *c = new WiFiClient(_localServer.accept());
  if (!c || !c->connected()) {
    delete c;
    return -1;
  }
  c->setNoDelay(true);

  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!_clients[i].active) {
      _clients[i].client = c;
      _clients[i].active = true;
      _clients[i].subCount = 0;
      _clients[i].lastPid = 0;
      _clients[i].lastActivity = millis();
      return i;
    }
  }
  c->stop();
  delete c;
  return -2;
}

void MqttBridge::disconnectClient(int idx) {
  if (!_clients[idx].active) return;
  _clients[idx].subCount = 0;
  if (_clients[idx].client) {
    _clients[idx].client->stop();
    delete _clients[idx].client;
    _clients[idx].client = nullptr;
  }
  _clients[idx].active = false;
}

// ------------------------------------------------------------------
// downstream MQTT packet handling
// ------------------------------------------------------------------
void MqttBridge::handleClient(int idx) {
  MqttClientCtx &cl = _clients[idx];
  WiFiClient &c = *cl.client;

  uint8_t header;
  if (!readByte(c, header)) return;
  cl.lastActivity = millis();

  uint8_t type = (header >> 4) & 0x0F;
  uint32_t remaining = 0;
  if (!readRemainingLength(c, remaining)) return;

  switch (type) {
    case 1: { // CONNECT
      uint8_t buf[32];
      while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        if (!readBytes(c, buf, chunk)) return;
        remaining -= chunk;
      }
      sendConnAck(c, false, 0);
      break;
    }

    case 3: { // PUBLISH
      uint8_t tlenBuf[2];
      if (!readBytes(c, tlenBuf, 2)) return;
      uint16_t tlen = (tlenBuf[0] << 8) | tlenBuf[1];
      char topic[128];
      uint16_t rlen = (tlen < sizeof(topic) - 1) ? tlen : sizeof(topic) - 1;
      for (uint16_t i = 0; i < rlen; i++) {
        uint8_t ch;
        if (!readByte(c, ch)) return;
        topic[i] = (char)ch;
      }
      topic[rlen] = 0;
      for (uint16_t i = rlen; i < tlen; i++) {
        uint8_t ch;
        if (!readByte(c, ch)) return;
      }

      uint8_t qos = (header >> 1) & 0x03;
      uint16_t pid = 0;
      if (qos > 0) {
        uint8_t pidBuf[2];
        if (!readBytes(c, pidBuf, 2)) return;
        pid = (pidBuf[0] << 8) | pidBuf[1];
      }

      uint32_t payloadLen = remaining - 2 - tlen - (qos > 0 ? 2 : 0);
      uint8_t payload[MQTT_BUFFER_SIZE];
      uint32_t toRead = (payloadLen > MQTT_BUFFER_SIZE) ? MQTT_BUFFER_SIZE : payloadLen;
      if (toRead > 0 && !readBytes(c, payload, toRead)) return;

      // Fake printer responses when no real printer is connected
      if (!_pubsub.connected() && strcmp(topic, localRequestTopic) == 0) {
        if (strstr((const char *)payload, "\"get_version\"")) {
          char seq[16] = "0";
          const char *s = strstr((const char *)payload, "\"sequence_id\"");
          if (s) {
            s = strchr(s, ':');
            if (s) {
              s++;
              while (*s == ' ' || *s == '\"') s++;
              const char *e = s;
              while (*e && *e != '\"' && *e != ' ' && *e != ',' && *e != '}') e++;
              size_t slen = e - s;
              if (slen > 0 && slen < sizeof(seq)) {
                memcpy(seq, s, slen);
                seq[slen] = 0;
              }
            }
          }

          char resp[384];
          int rlen2 = snprintf(resp, sizeof(resp),
            "{\"info\":{\"command\":\"get_version\",\"sequence_id\":\"%s\","
            "\"version\":\"" VERSION "\",\"model\":\"%s\",\"online\":\"true\"}}",
            seq, _cfg ? _cfg->printerModel : PRINTER_MODEL_DFLT);
          sendPublish(c, localReportTopic, (uint8_t *)resp, rlen2, 0);
        }

        if (strstr((const char *)payload, "\"pushall\"")) {
          char resp[512];
          int rlen2 = snprintf(resp, sizeof(resp),
            "{\"print\":{\"command\":\"pushall\",\"sequence_id\":\"0\","
            "\"gcode_state\":\"IDLE\",\"subtask_name\":\"\",\"project_id\":\"\","
            "\"gcode_file\":\"\",\"mc_percent\":0,\"mc_remaining_time\":0,"
            "\"layer_num\":0,\"total_layer_num\":0,"
            "\"nozzle_temper\":25.1,\"nozzle_target_temper\":0,"
            "\"bed_temper\":25.2,\"bed_target_temper\":0,"
            "\"chamber_temper\":25.0,\"spd_mag\":100,"
            "\"wifi_signal\":\"-50dBm\",\"sdcard\":true}}");
          sendPublish(c, localReportTopic, (uint8_t *)resp, rlen2, 0);
        }
      }

      _pubsub.publish(requestTopic, payload, toRead, false);

      if (qos == 1) {
        uint8_t ackBuf[2] = {(uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF)};
        c.write(0x40);
        writeRemainingLength(c, 2);
        c.write(ackBuf, 2);
      }
      break;
    }

    case 8: { // SUBSCRIBE
      uint8_t pidBuf[2];
      if (!readBytes(c, pidBuf, 2)) return;
      uint16_t pid = (pidBuf[0] << 8) | pidBuf[1];

      uint8_t topicCount = 0;
      uint32_t rp = remaining - 2;
      while (rp > 0) {
        uint8_t tlenBuf[2];
        if (!readBytes(c, tlenBuf, 2)) return;
        uint16_t tlen = (tlenBuf[0] << 8) | tlenBuf[1];
        String subTopic;
        subTopic.reserve(tlen + 1);
        for (uint16_t i = 0; i < tlen; i++) {
          uint8_t ch;
          if (!readByte(c, ch)) return;
          subTopic += (char)ch;
        }
        uint8_t subOpts;
        if (!readByte(c, subOpts)) return;
        rp -= 3 + tlen;
        topicCount++;

        if (cl.subCount < MAX_MQTT_CLIENTS) {
          cl.subs[cl.subCount].topic = subTopic;
          cl.subs[cl.subCount].qos = subOpts & 0x03;
          cl.subCount++;
        }
        _pubsub.subscribe(subTopic.c_str(), subOpts & 0x03);
      }

      sendSubAck(c, pid, topicCount);
      break;
    }

    case 10: { // UNSUBSCRIBE
      uint8_t pidBuf[2];
      if (!readBytes(c, pidBuf, 2)) return;
      uint16_t pid = (pidBuf[0] << 8) | pidBuf[1];

      uint32_t rp = remaining - 2;
      while (rp > 0) {
        uint8_t tlenBuf[2];
        if (!readBytes(c, tlenBuf, 2)) return;
        uint16_t tlen = (tlenBuf[0] << 8) | tlenBuf[1];
        String unsubTopic;
        unsubTopic.reserve(tlen + 1);
        for (uint16_t i = 0; i < tlen; i++) {
          uint8_t ch;
          if (!readByte(c, ch)) return;
          unsubTopic += (char)ch;
        }
        rp -= 2 + tlen;

        for (uint8_t s = 0; s < cl.subCount; s++) {
          if (cl.subs[s].topic == unsubTopic) {
            for (uint8_t k = s; k < cl.subCount - 1; k++) cl.subs[k] = cl.subs[k + 1];
            cl.subCount--;
            break;
          }
        }

        bool stillWanted = false;
        for (int j = 0; j < MAX_MQTT_CLIENTS && !stillWanted; j++) {
          if (!_clients[j].active || j == idx) continue;
          for (uint8_t s = 0; s < _clients[j].subCount; s++) {
            if (_clients[j].subs[s].topic == unsubTopic) {
              stillWanted = true;
              break;
            }
          }
        }
        if (!stillWanted) {
          _pubsub.unsubscribe(unsubTopic.c_str());
        }
      }

      sendUnsubAck(c, pid);
      break;
    }

    case 12: // PINGREQ
      sendPingResp(c);
      break;

    case 14: // DISCONNECT
      disconnectClient(idx);
      break;

    default:
      uint8_t buf[32];
      while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        if (!readBytes(c, buf, chunk)) return;
        remaining -= chunk;
      }
      break;
  }
}

// ------------------------------------------------------------------
// MQTT packet writers
// ------------------------------------------------------------------
void MqttBridge::sendConnAck(WiFiClient &c, bool sp, uint8_t rc) {
  uint8_t vh[2] = {(uint8_t)(sp ? 1 : 0), rc};
  c.write(0x20);
  writeRemainingLength(c, 2);
  c.write(vh, 2);
}

void MqttBridge::sendSubAck(WiFiClient &c, uint16_t pid, uint8_t count) {
  uint8_t vh[2] = {(uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF)};
  c.write(0x90);
  writeRemainingLength(c, 2 + count);
  c.write(vh, 2);
  for (uint8_t i = 0; i < count; i++) {
    c.write(0x00);
  }
}

void MqttBridge::sendUnsubAck(WiFiClient &c, uint16_t pid) {
  uint8_t vh[2] = {(uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF)};
  c.write(0xB0);
  writeRemainingLength(c, 2);
  c.write(vh, 2);
}

void MqttBridge::sendPingResp(WiFiClient &c) {
  c.write(0xD0);
  writeRemainingLength(c, 0);
}

void MqttBridge::sendPublish(WiFiClient &c, const String &topic,
                             const uint8_t *payload, uint32_t len,
                             uint8_t qos) {
  uint16_t tlen = topic.length();
  uint32_t totalRemaining = 2 + tlen + len;
  c.write((3 << 4) | (qos << 1));
  writeRemainingLength(c, totalRemaining);
  c.write((uint8_t)(tlen >> 8));
  c.write((uint8_t)(tlen & 0xFF));
  for (uint16_t i = 0; i < tlen; i++) {
    c.write((uint8_t)topic[i]);
  }
  if (len > 0) c.write(payload, len);
}

// ------------------------------------------------------------------
// wire-level helpers
// ------------------------------------------------------------------
bool MqttBridge::readByte(WiFiClient &c, uint8_t &b) {
  int v = c.read();
  if (v < 0) return false;
  b = (uint8_t)v;
  return true;
}

bool MqttBridge::readRemainingLength(WiFiClient &c, uint32_t &len) {
  len = 0;
  uint32_t multiplier = 1;
  for (int i = 0; i < 4; i++) {
    uint8_t b;
    if (!readByte(c, b)) return false;
    len += (b & 0x7F) * multiplier;
    multiplier *= 128;
    if (!(b & 0x80)) return true;
  }
  return false;
}

bool MqttBridge::readBytes(WiFiClient &c, uint8_t *buf, uint32_t len) {
  while (len > 0) {
    int got = c.read(buf, len);
    if (got <= 0) return false;
    buf += got;
    len -= got;
  }
  return true;
}

void MqttBridge::writeRemainingLength(WiFiClient &c, uint32_t len) {
  do {
    uint8_t b = len % 128;
    len /= 128;
    if (len > 0) b |= 0x80;
    c.write(b);
  } while (len > 0);
}
