#include "mqtt_bridge.h"
#ifdef ESP32
#include <ESPmDNS.h>
#include <mbedtls/base64.h>
#include <Preferences.h>
#endif

static char reportTopic[64];
static char requestTopic[64];
static char localReportTopic[64];
static char localRequestTopic[64];

// DER to PEM conversion using mbedTLS base64
static void derToPem(const uint8_t *der, size_t derLen, char *pem, size_t pemSize) {
  size_t olen = 0;
  mbedtls_base64_encode(NULL, 0, &olen, der, derLen);
  size_t b64Len = (olen / 64) * 65 + olen + 128;
  if (b64Len > pemSize) { pem[0] = 0; return; }
  size_t outLen;
  mbedtls_base64_encode((unsigned char *)pem, pemSize, &outLen, der, derLen);
  // Re-wrap with 64-char lines and PEM headers
  char tmp[3072];
  size_t pos = 0;
  pos += snprintf(tmp + pos, sizeof(tmp) - pos, "-----BEGIN CERTIFICATE-----\n");
  for (size_t off = 0; off < outLen; off += 64) {
    size_t chunk = (outLen - off > 64) ? 64 : (outLen - off);
    memcpy(tmp + pos, pem + off, chunk); pos += chunk;
    tmp[pos++] = '\n';
  }
  pos += snprintf(tmp + pos, sizeof(tmp) - pos, "-----END CERTIFICATE-----\n");
  tmp[pos] = 0;
  memcpy(pem, tmp, pos + 1);
}

MqttBridge::MqttBridge()
  : _localServer(MQTT_LOCAL_PORT),
    _tlsServer(MQTT_PRINTER_PORT),
    _lastReconnect(0), _cfg(nullptr) {
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    _clients[i].active = false;
    _clients[i].isTls = false;
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

void MqttBridge::rebind() {
  _localServer.close();
  _localServer.begin();
  _localServer.setNoDelay(true);
  WiFiClient drain = _localServer.accept();
  _tlsServer.close();
  _tlsServer.begin();
  _tlsServer.setNoDelay(true);
  WiFiClient drainTls = _tlsServer.accept();
  Serial.println("MQTT: servers rebound (drained)");
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

  _tlsServer.begin();
  _tlsServer.setNoDelay(true);

  // Generate or load self-signed device cert for TLS server (CA=FALSE, EKU+SAN)
#ifdef ESP32
  {
    // Free up old NVS namespaces
    // Clear legacy NVS namespaces — intentionally excludes "mqttg31" (current)
    for (const char *ns : {"mqttgate", "mqttg2", "mqttg3", "mqttg4", "mqttg5", "mqttg6", "mqttg7", "mqttg8", "mqttg9", "mqttg10", "mqttg11", "mqttg12", "mqttg13", "mqttg14", "mqttg15", "mqttg16", "mqttg17", "mqttg18", "mqttg19", "mqttg20", "mqttg21", "mqttg22", "mqttg23", "mqttg24", "mqttg25", "mqttg26", "mqttg27", "mqttg28", "mqttg29", "mqttg30"}) {
      Preferences oldPrefs;
      oldPrefs.begin(ns, false);
      oldPrefs.clear();
      oldPrefs.end();
    }

    Preferences prefs;
            prefs.begin("mqttg31", false);
    _certLen = prefs.getBytes("certDer", _certDer, sizeof(_certDer));
    _keyLen = prefs.getBytes("keyDer", _keyDer, sizeof(_keyDer));
    _caLen = prefs.getBytes("caDer", _caDer, sizeof(_caDer));
    bool loaded = (_certLen > 0 && _keyLen > 0 && _caLen > 0);
    prefs.end();

    if (!loaded) {
      // Wait for STA IP if station mode is configured
      if (strlen(_cfg->stationSsid) > 0) {
        unsigned long start = millis();
        while (!WiFi.isConnected() && millis() - start < 15000) delay(100);
      }
      IPAddress localIP = WiFi.isConnected() ? WiFi.localIP() : WiFi.softAPIP();
      Serial.printf("MQTT: generating cert with IP %s\n", localIP.toString().c_str());
      uint8_t ipBytes[4] = { localIP[0], localIP[1], localIP[2], localIP[3] };
      if (!generateCertChain(_cfg->gatewaySerial, _certDer, &_certLen,
                              _keyDer, &_keyLen,
                              _caDer, &_caLen,
                              ipBytes)) {
        Serial.println("MQTT: cert generation failed!");
      } else {
        prefs.begin("mqttg31", false);
        prefs.putBytes("certDer", _certDer, _certLen);
        prefs.putBytes("keyDer", _keyDer, _keyLen);
        prefs.putBytes("caDer", _caDer, _caLen);
        prefs.end();
        Serial.printf("MQTT: generated & saved cert chain (%d bytes) + CA (%d bytes) SN=%s\n",
                      _certLen, _caLen, _cfg->gatewaySerial);
      }
    } else {
      Serial.printf("MQTT: loaded cert chain from flash (%d bytes) + CA (%d bytes) SN=%s\n",
                    _certLen, _caLen, _cfg->gatewaySerial);
    }
    derToPem(_certDer, _certLen, _certPem, sizeof(_certPem));
    derToPem(_caDer, _caLen, _caPem, sizeof(_caPem));
  }
#endif

  Serial.println("MQTT: local server ready on port 1883");
}

const char *MqttBridge::getTlsCert() {
#ifdef ESP32
  if (_caPem[0]) return _caPem;
#endif
  return "-----BEGIN CERTIFICATE-----\n"
         "-----END CERTIFICATE-----\n";
}

void MqttBridge::loop() {
  if (!_upTcp || !_pubsub.connected()) {
    // Attempt upstream MQTT connection (only if printer is configured and WiFi is up)
    if (WiFi.isConnected() && _cfg && strlen(_cfg->printerHost) > 0) {
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
    }
  } else {
    _pubsub.loop();
  }

  int ac;
  while ((ac = acceptClient()) >= 0) {}
  if (ac < -1) Serial.printf("MQTT: acceptClient returned %d\n", ac);

  static unsigned long lastDiag = 0;
  unsigned long now = millis();
  int active = 0;
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!_clients[i].active) continue;
    active++;
  }
  if (active > 0 && now - lastDiag > 10000) {
    lastDiag = now;
    Serial.printf("MQTT: %d active client(s)\n", active);
  }

  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (!_clients[i].active) continue;

    // Continue TLS handshake for pending clients
    if (_clients[i].isTls) {
      TlsWiFiClient *tls = (TlsWiFiClient *)_clients[i].client;
      if (!tls->handshakeDone()) {
        int hs = tls->continueHandshake();
        if (hs == 0) continue;   // still handshaking
        if (hs < 0) {            // handshake failed
          Serial.printf("MQTT: TLS handshake failed for client %d\n", i);
          disconnectClient(i);
          continue;
        }
        Serial.printf("MQTT: TLS handshake done for client %d\n", i);
        // Fall through to handle MQTT once handshake completes
      }
    }

    if (!_clients[i].client->connected()) {
      disconnectClient(i);
      continue;
    }
    handleClient(i);
  }
}

bool MqttBridge::isConnected() {
  return _upTcp && _pubsub.connected();
}

MqttStatus MqttBridge::getStatus() {
  if (_upTcp && _pubsub.connected()) return MQTT_UP;
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

  const char *host = _cfg->printerHost;
  char resolved[64];
#ifdef ESP32
  size_t hl = strlen(host);
  if (hl > 6 && strcmp(host + hl - 6, ".local") == 0 && WiFi.isConnected()) {
    char name[64];
    size_t nl = hl - 6;
    memcpy(name, host, nl);
    name[nl] = '\0';
    IPAddress ip = MDNS.queryHost(name, 3000);
    if (ip != IPAddress(0, 0, 0, 0)) {
      snprintf(resolved, sizeof(resolved), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      host = resolved;
      Serial.printf("mqtt: resolved %s.local -> %s\n", name, host);
    }
  }
#endif
  if (!_upTcp->connect(host, MQTT_PRINTER_PORT)) {
    delete _upTcp; _upTcp = nullptr;
    return false;
  }
  _pubsub.setClient(*_upTcp);

  char willTopic[64];
  snprintf(willTopic, sizeof(willTopic), "device/%s/status", _cfg->printerSerial);

  char clientId[48];
  snprintf(clientId, sizeof(clientId), "BambuTagger-%s", _cfg->printerSerial);

  bool ok = _pubsub.connect(clientId, "bblp", _cfg->printerCode,
                            willTopic, 1, true, "offline");
  if (ok) {
    _pubsub.publish(willTopic, "online", true);
  }
  return ok;
}

static bool topicMatchesSub(const String &topic, const String &sub) {
  int ti = 0, si = 0;
  int tl = topic.length(), sl = sub.length();

  while (ti < tl && si < sl) {
    if (sub[si] == '#') return true;
    if (sub[si] == '+') {
      // skip this level in topic
      while (ti < tl && topic[ti] != '/') ti++;
      si++;
      // skip '/' delimiter in both
      if (ti < tl && topic[ti] == '/') ti++;
      if (si < sl && sub[si] == '/') si++;
      continue;
    }
    if (topic[ti] != sub[si]) return false;
    ti++;
    si++;
  }

  // both exhausted
  if (ti == tl && si == sl) return true;
  // sub has trailing "/#"  (e.g. "device/#")
  if (si <= sl - 2 && sub.substring(si) == "/#") return true;
  // sub has bare "#" at current position
  if (si < sl && sub[si] == '#') return true;

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
  // Try TLS server (port 8883) first
  {
    WiFiClient raw = _tlsServer.accept();
    if (raw) {
      if (!raw.connected()) { raw.stop(); return -1; }
      raw.setNoDelay(true);

      WiFiClient *rawTcp = new WiFiClient(raw);
      TlsWiFiClient *tls = new TlsWiFiClient();
      if (!tls->beginDer(rawTcp, _certDer, _certLen, _keyDer, _keyLen)) {
        Serial.println("MQTT: TLS beginDer failed");
        delete tls;  // ~TlsWiFiClient deletes rawTcp
        return -1;
      }

      for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (!_clients[i].active) {
          _clients[i].client = tls;
          _clients[i].active = true;
          _clients[i].isTls = true;
          _clients[i].subCount = 0;
          _clients[i].lastPid = 0;
          _clients[i].lastActivity = millis();
          Serial.printf("MQTT: TLS client %d from %s\n", i, raw.remoteIP().toString().c_str());
          return i;
        }
      }
      // No slots — clean up
      delete tls;  // ~TlsWiFiClient handles all cleanup
      return -2;
    }
  }

  // Try plain server (port 1883)
  {
    WiFiClient plain = _localServer.accept();
    if (!plain) return -1;

    WiFiClient *c = new WiFiClient(plain);
    if (!c || !c->connected()) {
      delete c;
      return -1;
    }
    c->setNoDelay(true);
    Serial.printf("MQTT: plain client from %s\n", c->remoteIP().toString().c_str());

    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
      if (!_clients[i].active) {
        _clients[i].client = c;
        _clients[i].active = true;
        _clients[i].isTls = false;
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
  _clients[idx].isTls = false;
}

// ------------------------------------------------------------------
// downstream MQTT packet handling
// ------------------------------------------------------------------
void MqttBridge::handleClient(int idx) {
  MqttClientCtx &cl = _clients[idx];
  WiFiClient &c = *cl.client;

  uint8_t header;
  if (!readByte(c, header)) {
    Serial.printf("MQTT: client %d read failed, disconnecting\n", idx);
    disconnectClient(idx);
    return;
  }
  cl.lastActivity = millis();

  // Log first MQTT packet type from each client
  static bool logged[8] = {};
  if (idx >= 0 && idx < 8 && !logged[idx]) {
    logged[idx] = true;
    uint8_t type = (header >> 4) & 0x0F;
    Serial.printf("MQTT: client %d first pkt type=%d\n", idx, type);
  }

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
      Serial.printf("MQTT: ConnAck sent to client %d\n", idx);
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

      // Fake printer responses when no real printer is connected.
      if (!_pubsub.connected()) {
        int tlen = strlen(topic);
        static const char reqSuffix[] = "/request";
        static const int slen = sizeof(reqSuffix) - 1;
        if (tlen > slen && strcmp(topic + tlen - slen, reqSuffix) == 0) {
          char respTopic[128];
          memcpy(respTopic, topic, tlen - slen);
          memcpy(respTopic + tlen - slen, "/report", 7);
          respTopic[tlen - slen + 7] = '\0';

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
            const char *model = _cfg ? _cfg->printerModel : PRINTER_MODEL_DFLT;
            const char *serial = _cfg ? _cfg->printerSerial : PRINTER_SERIAL_DFLT;
            int rlen2 = snprintf(resp, sizeof(resp),
              "{\"info\":{\"command\":\"get_version\",\"sequence_id\":\"%s\","
              "\"version\":\"" VERSION "\",\"module\":\"%s\",\"model\":\"%s\","
              "\"serial\":\"%s\",\"online\":\"true\"}}",
              seq, model, model, serial);
            sendPublish(c, respTopic, (uint8_t *)resp, rlen2, 0);
          }

          if (strstr((const char *)payload, "\"pushall\"")) {
            char resp[512];
            const char *serial = _cfg ? _cfg->printerSerial : PRINTER_SERIAL_DFLT;
            int rlen2 = snprintf(resp, sizeof(resp),
              "{\"print\":{\"command\":\"pushall\",\"sequence_id\":\"0\","
              "\"serial\":\"%s\","
              "\"gcode_state\":\"IDLE\",\"subtask_name\":\"\",\"project_id\":\"\","
              "\"gcode_file\":\"\",\"mc_percent\":0,\"mc_remaining_time\":0,"
              "\"layer_num\":0,\"total_layer_num\":0,"
              "\"nozzle_temper\":25.1,\"nozzle_target_temper\":0,"
              "\"bed_temper\":25.2,\"bed_target_temper\":0,"
              "\"chamber_temper\":25.0,\"spd_mag\":100,"
              "\"wifi_signal\":\"-50dBm\",\"sdcard\":true}}",
              serial);
            sendPublish(c, respTopic, (uint8_t *)resp, rlen2, 0);
          }
        }
      }

      _pubsub.publish(requestTopic, payload, toRead, false);

      if (qos == 1) {
        uint8_t ackBuf[2] = {(uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF)};
        c.write((uint8_t)0x40);
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

      // In fake printer mode (upstream disconnected), send printer identity
      // immediately after subscribe so Bambu Studio auto-detects serial/model.
      if (!_pubsub.connected() && _cfg) {
        static const char reportPrefix[] = "device/";
        String r1 = String(reportPrefix) + _cfg->gatewaySerial + "/report";
        String r2 = String(reportPrefix) + _cfg->printerSerial + "/report";
        for (uint8_t s = 0; s < cl.subCount; s++) {
          String rt;
          if (topicMatchesSub(r1, cl.subs[s].topic))
            rt = r1;
          else if (topicMatchesSub(r2, cl.subs[s].topic))
            rt = r2;
          if (rt.length() > 0) {
            char buf[384];
            int n = snprintf(buf, sizeof(buf),
              "{\"info\":{\"command\":\"get_version\",\"sequence_id\":\"0\","
              "\"version\":\"" VERSION "\",\"module\":\"%s\",\"model\":\"%s\","
              "\"serial\":\"%s\",\"online\":\"true\"}}",
              _cfg->printerModel, _cfg->printerModel, _cfg->printerSerial);
            sendPublish(c, rt, (uint8_t *)buf, n, 0);
            break;
          }
        }
      }

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
  c.write((uint8_t)0x20);
  writeRemainingLength(c, 2);
  c.write(vh, 2);
}

void MqttBridge::sendSubAck(WiFiClient &c, uint16_t pid, uint8_t count) {
  uint8_t vh[2] = {(uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF)};
  c.write((uint8_t)0x90);
  writeRemainingLength(c, 2 + count);
  c.write(vh, 2);
  for (uint8_t i = 0; i < count; i++) {
    c.write((uint8_t)0x00);
  }
}

void MqttBridge::sendUnsubAck(WiFiClient &c, uint16_t pid) {
  uint8_t vh[2] = {(uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF)};
  c.write((uint8_t)0xB0);
  writeRemainingLength(c, 2);
  c.write(vh, 2);
}

void MqttBridge::sendPingResp(WiFiClient &c) {
  c.write((uint8_t)0xD0);
  writeRemainingLength(c, 0);
}

void MqttBridge::sendPublish(WiFiClient &c, const String &topic,
                             const uint8_t *payload, uint32_t len,
                             uint8_t qos) {
  uint16_t tlen = topic.length();
  uint32_t totalRemaining = 2 + tlen + len;
  c.write((uint8_t)((3 << 4) | (qos << 1)));
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
