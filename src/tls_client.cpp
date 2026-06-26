#include "tls_client.h"
#include <string.h>

#define TAG "TLS"

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
    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
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
  if (r > 0) return r;
  if (r == 0) return MBEDTLS_ERR_SSL_WANT_READ;
  return MBEDTLS_ERR_SSL_CONN_EOF;
}

int TlsWiFiClient::bio_send(void *ctx, const unsigned char *buf, size_t len) {
  TlsWiFiClient *c = (TlsWiFiClient *)ctx;
  if (!c->_tcp || !c->_tcp->connected()) return MBEDTLS_ERR_SSL_CONN_EOF;
  int r = c->_tcp->write(buf, len);
  if (r > 0) return r;
  return MBEDTLS_ERR_SSL_WANT_WRITE;
}
