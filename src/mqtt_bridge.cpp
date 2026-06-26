#include "mqtt_bridge.h"

static char reportTopic[64];
static char requestTopic[64];
static char localReportTopic[64];
static char localRequestTopic[64];

// Self-signed cert + key for local TLS MQTT broker (port 8883).
// Bambu Studio requires TLS on port 8883 to connect to a printer.
// CN=BambuTagger-Gateway, O=BBL Technologies Co., Ltd, C=CN
// v3 with keyUsage=digitalSignature,keyEncipherment,keyCertSign; extendedKeyUsage=serverAuth
static const char tls_cert[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
MIID/TCCAuWgAwIBAgIJAI1cw5dxX/X+MA0GCSqGSIb3DQEBCwUAME8xHDAaBgNV
BAMME0JhbWJ1VGFnZ2VyLUdhdGV3YXkxIjAgBgNVBAoMGUJCTCBUZWNobm9sb2dp
ZXMgQ28uLCBMdGQxCzAJBgNVBAYTAkNOMCAXDTI2MDYyNTIzNTMzM1oYDzIxMjYw
NjAxMjM1MzMzWjBPMRwwGgYDVQQDDBNCYW1idVRhZ2dlci1HYXRld2F5MSIwIAYD
VQQKDBlCQkwgVGVjaG5vbG9naWVzIENvLiwgTHRkMQswCQYDVQQGEwJDTjCCASIw
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMYmKeA0FL97aQnhtcSjau40pTK/
OJOF8aCJXhlMguVIQngsQS6bHOgZlfhrOD288kTHgxfrAcXEWifwjlQ1OqzVApyO
5K3t7G5hv8kmIhi34ymRYuy7QaD7iWwllupP4Epj8RkuwOl4BUas9sIo8E8kzpYE
1w10s6nwwkePBSuC7LIVcsPBCOFowgpxI0HYEd+eTx7DstlW4kw2zSqSWk8DAOzh
ICSRn1KaheeNrtMwMBusBdgIhnzjSJluQjhoYSadfDgKHtEKy/qxr5f0V/JfKIVG
x9u4mRTP2JtU5CJVvfiPl/Tosk4zU+FOUpELudQZ0ci0iFY/4z1s+SIG+8ECAwEA
AaOB2TCB1jAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwICpDATBgNVHSUE
DDAKBggrBgEFBQcDATAdBgNVHQ4EFgQUz0/03xvJMUkx66EeOfudkD5hwR8wfwYD
VR0jBHgwdoAUz0/03xvJMUkx66EeOfudkD5hwR+hU6RRME8xHDAaBgNVBAMME0Jh
bWJ1VGFnZ2VyLUdhdGV3YXkxIjAgBgNVBAoMGUJCTCBUZWNobm9sb2dpZXMgQ28u
LCBMdGQxCzAJBgNVBAYTAkNOggkAjVzDl3Ff9f4wDQYJKoZIhvcNAQELBQADggEB
AMIAZT7MHfMB8iJAoLW1GkZEO4PW+boVajpZggDAKU9CRmg22BvDBeJspl3oKGVi
w1bxNsPBAb+NptK+aDrnmN0w+G1dJgWVEp81iOp929EH4az6P9AQnMsGDWOjaD18
SCviuNLzk/nzMue+mQ+NINZAYXJNfc3CJbaIvJcmFVK04fD0tDhLn80y7RX3TPSH
l4Du5XBUv5cdxDEy0fBe2ZYC5mV/sOKBWFO2qnKnC2Dn0P8hCfJM9KhGsKcTftDu
Z3W7/cOpYBv+Yc1lUDhBfdMLlhI3eWUNANg8LjPRWO27yYiFNE6Oo+ErwCro81iy
owe+zlAvYyFvO5lvhbA8EOo=
-----END CERTIFICATE-----
)KEY";

static const char tls_key[] PROGMEM = R"KEY(
-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDGJingNBS/e2kJ
4bXEo2ruNKUyvziThfGgiV4ZTILlSEJ4LEEumxzoGZX4azg9vPJEx4MX6wHFxFon
8I5UNTqs1QKcjuSt7exuYb/JJiIYt+MpkWLsu0Gg+4lsJZbqT+BKY/EZLsDpeAVG
rPbCKPBPJM6WBNcNdLOp8MJHjwUrguyyFXLDwQjhaMIKcSNB2BHfnk8ew7LZVuJM
Ns0qklpPAwDs4SAkkZ9SmoXnja7TMDAbrAXYCIZ840iZbkI4aGEmnXw4Ch7RCsv6
sa+X9FfyXyiFRsfbuJkUz9ibVOQiVb34j5f06LJOM1PhTlKRC7nUGdHItIhWP+M9
bPkiBvvBAgMBAAECggEAO3TeIeFezGoqhYWNtjhW8K0pWMXaIyIQ89vkOXEk4cnB
8C9PS73NebObtZPup0/X3l2Db5zbxkz5xHxBKPFj7tJn2zRhV/NJe4GnO6NOnd4n
sqRma6Rwt+5iOOo6k4puQcQlZyoJRsT1yFREItSH7yebOZawNOBsvLR6h2BZ68jx
Hcax4Cpz7Fl3x+wgPIyF9CoFZt7sXpVjTzC4pgp9JC5EF0d+kLrZMP1ODTydlmtH
XZiy//JGoL9WLL3aBIJT5IRvZajrrvFri35IevZh++mWcMT9zqfffdXrZxoBQeV0
wbJMl/MDB+n9FLKKu7JWzDZlU6dULB/6d74sHsj+xQKBgQDprWjgJItF6VrOpqWw
k6m715W2O0rM2flBx1Typ+IuhOl2TGgGeKDHxN/0afmPe/1dUAGvRfVe+rNuJNEq
e4t/eSqv3xKRVqow3/dtDOaPFFj0xAm1rxSTTQNKRv9csE2Wzbw3KCK1IF/y5atL
1bw9LDbVpGDNJrGEQ8H1sFA0HwKBgQDZE+hegzDdm0G4yl17f3MGmWtlmU3e4hC0
aoiGmWcjLQFxCg4KtgLpfp8S211vlVyJO3ACcIYR+fQWpmTy8awtAIMpvOBc73yF
84GlD4Op7rtiANom/q2aGVNk093cywboZGq1jqSzu2w8pAETfHwD5/wq95n97GTf
74tu9fnUHwKBgHA8wGDYbKS5vsn/NRoo8p+sntYWiIj4MUas7VpX1MWvRUtyy4xA
KEmLgF4vAJUwYrONGCINohtqowBGYsja6lfh5OTwakSwsbIkAP258ovKpCd8eYVw
gJt3pBrrGwB0FfBXBQ4hEvqYgD10nuAf2vgu4m+fMneXHDCBMwpFE2DVAoGAVZYa
xMC20Hi5JdFroBh00oJErK8P27OH4IosP91Vo7HH4riTJrfyV/sbXsTshuT9sgGk
POH+ijHhgdii7oJIXwnXrOoSD7JAh1Olpt2CDMraSF6LpFo/OgWIMrWxwK6vj4qf
4+tUlqRrnVEQN42aG7QoYQx0Q4AjmYMJl3sVwAMCgYEAh5e0wacJgrPrnW+uNWKw
WxOt96UbeAgRxTH5VDeld0WD+PvsEA53fZDo23tGn9kcQRlC8o6Oihc0JmMizW6D
iej768hVc0sC89dEkYs2hYhd5W65u+meLEh/mPzKpshHQWMlvRWFe981fd8UPqC+
jdY9rChFu/pNLg811ar1550=
-----END PRIVATE KEY-----
)KEY";

MqttBridge::MqttBridge()
  : _localServer(MQTT_LOCAL_PORT), _tlsServer(MQTT_PRINTER_PORT),
    _lastReconnect(0), _cfg(nullptr) {
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

  // TLS server on 8883 — Bambu Studio requires TLS on this port
  static BearSSL::X509List cert(tls_cert);
  static BearSSL::PrivateKey key(tls_key);
  _tlsServer.setRSACert(&cert, &key);
  _tlsServer.begin();
  _tlsServer.setNoDelay(true);
  Serial.println("TLS: server ready on port 8883");
}

const char *MqttBridge::getTlsCert() {
  return tls_cert;
}

void MqttBridge::loop() {
  if (!_upTcp || !_pubsub.connected()) {
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
  if (!_upTcp->connect(_cfg->printerHost, MQTT_PRINTER_PORT)) {
    delete _upTcp; _upTcp = nullptr;
    return false;
  }
  _pubsub.setClient(*_upTcp);

  char willTopic[64];
  snprintf(willTopic, sizeof(willTopic), "device/%s/status", _cfg->printerSerial);

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "%s", _cfg->printerSerial);

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
  WiFiClient *c = nullptr;

  // Check plain TCP server (port 1883)
  WiFiClient plain = _localServer.accept();
  if (plain) {
    c = new WiFiClient(plain);
  } else {
    // Check TLS server (port 8883) for Bambu Studio clients
    WiFiClientSecure tls = _tlsServer.accept();
    if (tls) {
      c = new WiFiClientSecure(tls);
    }
  }

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

      // Fake printer responses when no real printer is connected.
      // Respond to any device/X/request topic so Bambu Studio
      // auto-detects the printer regardless of which serial is used.
      if (!_pubsub.connected()) {
        int tlen = strlen(topic);
        static const char reqSuffix[] = "/request";
        static const int slen = sizeof(reqSuffix) - 1;
        if (tlen > slen && strcmp(topic + tlen - slen, reqSuffix) == 0) {
          // Derive report topic from the request
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
