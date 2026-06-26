#include "tls_client.h"
#include <string.h>
#include <mbedtls/ssl_ciphersuites.h>

#define TAG "TLS"

// Force RSA key exchange cipher suites only (avoid ECDHE curve issues)
static const int forced_ciphersuites[] = {
  MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_RSA_WITH_AES_256_GCM_SHA384,
  0
};

static void tls_debug(void *ctx, int level, const char *file, int line, const char *str) {
  Serial.printf("TLSDBG: %s\n", str);
}

TlsWiFiClient::TlsWiFiClient() {
  mbedtls_ssl_init(&_ssl);
  mbedtls_ssl_config_init(&_conf);
  mbedtls_x509_crt_init(&_cert);
  mbedtls_pk_init(&_pkey);
  mbedtls_entropy_init(&_entropy);
  mbedtls_ctr_drbg_init(&_drbg);
}

TlsWiFiClient::~TlsWiFiClient() {
  stop();
}

bool TlsWiFiClient::begin(WiFiClient *tcp, const char *certPem, const char *keyPem) {
  _tcp = tcp;
  _hsDone = false;
  _ok = false;

  int r;

  r = mbedtls_ctr_drbg_seed(&_drbg, mbedtls_entropy_func, &_entropy, (const uint8_t *)TAG, strlen(TAG));
  if (r != 0) { Serial.printf("TLS: drbg seed failed: -0x%x\n", -r); return false; }

  r = mbedtls_x509_crt_parse(&_cert, (const uint8_t *)certPem, strlen(certPem) + 1);
  if (r != 0) { Serial.printf("TLS: cert parse failed: -0x%x\n", -r); return false; }

  r = mbedtls_pk_parse_key(&_pkey, (const uint8_t *)keyPem, strlen(keyPem) + 1, NULL, 0);
  if (r != 0) { Serial.printf("TLS: key parse failed: -0x%x\n", -r); return false; }

  r = mbedtls_ssl_config_defaults(&_conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (r != 0) { Serial.printf("TLS: config defaults failed: -0x%x\n", -r); return false; }

  mbedtls_ssl_conf_dbg(&_conf, tls_debug, NULL);
  mbedtls_ssl_conf_rng(&_conf, mbedtls_ctr_drbg_random, &_drbg);
  mbedtls_ssl_conf_own_cert(&_conf, &_cert, &_pkey);

  r = mbedtls_ssl_setup(&_ssl, &_conf);
  if (r != 0) { Serial.printf("TLS: ssl setup failed: -0x%x\n", -r); return false; }

  mbedtls_ssl_set_bio(&_ssl, this, bio_send, bio_recv, NULL);

  _ok = true;
  return true;
}

bool TlsWiFiClient::beginDer(WiFiClient *tcp, const uint8_t *certDer, size_t certLen,
                             const uint8_t *keyDer, size_t keyLen) {
  _tcp = tcp;
  _hsDone = false;
  _ok = false;

  int r;

  r = mbedtls_ctr_drbg_seed(&_drbg, mbedtls_entropy_func, &_entropy, (const uint8_t *)TAG, strlen(TAG));
  if (r != 0) { Serial.printf("TLS: drbg seed failed: -0x%x\n", -r); return false; }

  // Re-init cert/key in case constructor didn't fully reset
  mbedtls_x509_crt_free(&_cert);
  mbedtls_x509_crt_init(&_cert);
  mbedtls_pk_free(&_pkey);
  mbedtls_pk_init(&_pkey);

  r = mbedtls_x509_crt_parse(&_cert, certDer, certLen);
  if (r != 0) { Serial.printf("TLS: cert parse failed: -0x%x\n", -r); return false; }

  r = mbedtls_pk_parse_key(&_pkey, keyDer, keyLen, NULL, 0);
  if (r != 0) { Serial.printf("TLS: key parse failed: -0x%x\n", -r); return false; }

  // ... remainder identical to begin()
  r = mbedtls_ssl_config_defaults(&_conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (r != 0) { Serial.printf("TLS: config defaults failed: -0x%x\n", -r); return false; }

  // Force TLS 1.2 minimum
  mbedtls_ssl_conf_min_version(&_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
  mbedtls_ssl_conf_max_version(&_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);

  mbedtls_ssl_conf_dbg(&_conf, tls_debug, NULL);
  mbedtls_ssl_conf_ciphersuites(&_conf, forced_ciphersuites);
  mbedtls_ssl_conf_rng(&_conf, mbedtls_ctr_drbg_random, &_drbg);
  mbedtls_ssl_conf_own_cert(&_conf, &_cert, &_pkey);

  r = mbedtls_ssl_setup(&_ssl, &_conf);
  if (r != 0) { Serial.printf("TLS: ssl setup failed: -0x%x\n", -r); return false; }

  mbedtls_ssl_set_bio(&_ssl, this, bio_send, bio_recv, NULL);

  _ok = true;
  return true;
}

int TlsWiFiClient::continueHandshake() {
  if (!_ok || _hsDone) return _hsDone ? 1 : -1;
  int r = mbedtls_ssl_handshake(&_ssl);
  if (r == 0) { _hsDone = true; return 1; }
  if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
  Serial.printf("TLS: handshake error: -0x%x\n", -r);
  _ok = false;
  return -1;
}

void TlsWiFiClient::stop() {
  if (_ok) {
    mbedtls_ssl_close_notify(&_ssl);
    _ok = false;
  }
  if (_tcp) {
    _tcp->stop();
    _tcp = nullptr;
  }
  mbedtls_ssl_free(&_ssl);
  mbedtls_ssl_config_free(&_conf);
  mbedtls_x509_crt_free(&_cert);
  mbedtls_pk_free(&_pkey);
  mbedtls_ctr_drbg_free(&_drbg);
  mbedtls_entropy_free(&_entropy);
  // Re-init for potential reuse
  mbedtls_ssl_init(&_ssl);
  mbedtls_ssl_config_init(&_conf);
  mbedtls_x509_crt_init(&_cert);
  mbedtls_pk_init(&_pkey);
  mbedtls_entropy_init(&_entropy);
  mbedtls_ctr_drbg_init(&_drbg);
  _hsDone = false;
}

size_t TlsWiFiClient::write(uint8_t b) { return write(&b, 1); }
size_t TlsWiFiClient::write(const uint8_t *buf, size_t size) {
  if (!_ok || !_hsDone) return 0;
  int r = mbedtls_ssl_write(&_ssl, buf, size);
  if (r < 0) { if (r != MBEDTLS_ERR_SSL_WANT_WRITE) _ok = false; return 0; }
  return r;
}

int TlsWiFiClient::available() {
  if (!_ok || !_tcp) return 0;
  int avail = mbedtls_ssl_get_bytes_avail(&_ssl);
  if (avail > 0) return avail;
  // Only fetch from TCP after handshake is done
  if (_hsDone && _tcp->available() > 0) return 1;
  return 0;
}

int TlsWiFiClient::read() {
  uint8_t b;
  if (read(&b, 1) == 1) return b;
  return -1;
}

int TlsWiFiClient::read(uint8_t *buf, size_t size) {
  if (!_ok || !_hsDone) return -1;
  int r = mbedtls_ssl_read(&_ssl, buf, size);
  if (r < 0) {
    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return -1;
    _ok = false;
    return -1;
  }
  return r;
}

int TlsWiFiClient::peek() { return -1; }
void TlsWiFiClient::flush() {}
uint8_t TlsWiFiClient::connected() {
  if (!_ok && _tcp) return _tcp->connected();
  return _ok && _tcp && _tcp->connected();
}

TlsWiFiClient::operator bool() { return _ok && _tcp && (*_tcp); }

int TlsWiFiClient::bio_recv(void *ctx, unsigned char *buf, size_t len) {
  TlsWiFiClient *c = (TlsWiFiClient *)ctx;
  if (!c->_tcp || !c->_tcp->connected()) return MBEDTLS_ERR_SSL_CONN_EOF;
  int r = c->_tcp->read(buf, len);
  if (!c->_hsDone) {
    if (r > 0) {
      // Show first 64 bytes of ClientHello (first recv=header 0x16, second=body 0x01)
      bool isHello = (buf[0] == 0x01) || (buf[0] == 0x16);
      int show = isHello ? (r > 80 ? 80 : r) : (r > 16 ? 16 : r);
      Serial.printf("TLS-RECV(ask=%d got=%d):", len, r);
      for (int i = 0; i < show; i++) Serial.printf(" %02x", (unsigned)buf[i]);
      if (r > show) Serial.printf(" ...");
      Serial.printf("\n");
    } else if (r == 0) {
      Serial.printf("TLS-RECV: WANT_READ (ask=%d)\n", len);
    } else {
      Serial.printf("TLS-RECV: EOF (ask=%d)\n", len);
    }
  }
  if (r > 0) return r;
  if (r == 0) return MBEDTLS_ERR_SSL_WANT_READ;
  return MBEDTLS_ERR_SSL_CONN_EOF;
}

int TlsWiFiClient::bio_send(void *ctx, const unsigned char *buf, size_t len) {
  TlsWiFiClient *c = (TlsWiFiClient *)ctx;
  if (!c->_tcp || !c->_tcp->connected()) return MBEDTLS_ERR_SSL_CONN_EOF;
  int r = c->_tcp->write(buf, len);
  if (r > 0) {
    if (!c->_hsDone) {
      int show = r > 16 ? 16 : r;
      Serial.printf("TLS-SEND(ask=%d wrote=%d):", len, r);
      for (int i = 0; i < show; i++) Serial.printf(" %02x", (unsigned)buf[i]);
      Serial.printf("\n");
    }
    return r;
  }
  return MBEDTLS_ERR_SSL_WANT_WRITE;
}
