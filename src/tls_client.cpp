#include "tls_client.h"
#include <string.h>
#include <mbedtls/error.h>

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
  _hsRetries = 0;
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

  mbedtls_ssl_conf_max_version(&_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
  mbedtls_ssl_conf_min_version(&_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
  mbedtls_ssl_conf_authmode(&_conf, MBEDTLS_SSL_VERIFY_NONE);

  mbedtls_ssl_conf_rng(&_conf, mbedtls_ctr_drbg_random, &_drbg);
  mbedtls_ssl_conf_own_cert(&_conf, &_cert, &_pkey);

  r = mbedtls_ssl_setup(&_ssl, &_conf);
  if (r != 0) { Serial.printf("TLS: ssl setup failed: -0x%x\n", -r); return false; }

  mbedtls_ssl_set_bio(&_ssl, this, bio_send, bio_recv, NULL);

  _ok = true;
  return true;
}

// Return the DER length of a single ASN.1 SEQUENCE at buf, or 0 on error
static size_t der_seq_length(const uint8_t *buf, size_t maxlen) {
  if (maxlen < 2 || buf[0] != 0x30) return 0;
  size_t len;
  if (buf[1] < 0x80) {
    len = buf[1];
    return 2 + len;
  }
  size_t nbytes = buf[1] & 0x7f;
  if (nbytes == 0 || 2 + nbytes > maxlen) return 0;
  len = 0;
  for (size_t i = 0; i < nbytes; i++)
    len = (len << 8) | buf[2 + i];
  return 2 + nbytes + len;
}

bool TlsWiFiClient::beginDer(WiFiClient *tcp, const uint8_t *certDer, size_t certLen,
                             const uint8_t *keyDer, size_t keyLen) {
  _tcp = tcp;
  _hsDone = false;
  _hsRetries = 0;
  _hsStart = 0;
  _hsState = 0;
  _ok = false;

  int r;
  r = mbedtls_ctr_drbg_seed(&_drbg, mbedtls_entropy_func, &_entropy, (const uint8_t *)TAG, strlen(TAG));
  if (r != 0) { Serial.printf("TLS: drbg seed failed: -0x%x\n", -r); return false; }

  // Re-init cert/key in case constructor didn't fully reset
  mbedtls_x509_crt_free(&_cert);
  mbedtls_x509_crt_init(&_cert);
  mbedtls_pk_free(&_pkey);
  mbedtls_pk_init(&_pkey);

  // Parse chain of concatenated DER certs (leaf + intermediates)
  size_t off = 0;
  while (off < certLen) {
    size_t derLen = der_seq_length(certDer + off, certLen - off);
    if (derLen == 0) {
      Serial.printf("TLS: bad DER at offset %u\n", (unsigned)off);
      return false;
    }
    r = mbedtls_x509_crt_parse(&_cert, certDer + off, derLen);
    if (r != 0) {
      Serial.printf("TLS: cert parse failed at offset %u: -0x%x\n", (unsigned)off, -r);
      return false;
    }
    off += derLen;
  }

  r = mbedtls_pk_parse_key(&_pkey, keyDer, keyLen, NULL, 0);
  if (r != 0) { Serial.printf("TLS: key parse failed: -0x%x\n", -r); return false; }

  // ... remainder identical to begin()
  r = mbedtls_ssl_config_defaults(&_conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (r != 0) { Serial.printf("TLS: config defaults failed: -0x%x\n", -r); return false; }

  mbedtls_ssl_conf_max_version(&_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
  mbedtls_ssl_conf_min_version(&_conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
  mbedtls_ssl_conf_authmode(&_conf, MBEDTLS_SSL_VERIFY_NONE);

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

  if (_hsStart == 0) _hsStart = millis();

  // 30-second total timeout for entire handshake
  if (millis() - _hsStart > 30000) {
    Serial.printf("TLS: handshake timed out\n");
    _ok = false;
    return -1;
  }

  _hsRetries++;

  int r = mbedtls_ssl_handshake(&_ssl);
  if (r == 0) {
    _hsDone = true; _hsRetries = 0;
    const char *cs = mbedtls_ssl_get_ciphersuite(&_ssl);
    if (cs) Serial.printf("TLS: handshake done, cipher=%s\n", cs);
    return 1;
  }
  if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
    _hsState = _ssl.state;
    return 0;
  }
  _hsState = _ssl.state;
  char errbuf[128];
  mbedtls_strerror(r, errbuf, sizeof(errbuf));
  Serial.printf("TLS: handshake error: -0x%x (%s) state=%d\n", -r, errbuf, _hsState);
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
    delete _tcp;
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
  if (!_ok || !_hsDone) { return -1; }
  int r = mbedtls_ssl_read(&_ssl, buf, size);
  if (r < 0) {
    if (r == MBEDTLS_ERR_SSL_WANT_READ) return -1;
    if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) { return -1; }
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
  // WiFiClient::read() returns -1 (or 0) when the buffer is empty — not an
  // error.  Only return CONN_EOF if the TCP connection has actually closed.
  if (!c->_tcp->connected()) return MBEDTLS_ERR_SSL_CONN_EOF;
  return MBEDTLS_ERR_SSL_WANT_READ;
}

int TlsWiFiClient::bio_send(void *ctx, const unsigned char *buf, size_t len) {
  TlsWiFiClient *c = (TlsWiFiClient *)ctx;
  if (!c->_tcp || !c->_tcp->connected()) return MBEDTLS_ERR_SSL_CONN_EOF;
  int r = c->_tcp->write(buf, len);
  if (r > 0) return r;
  return MBEDTLS_ERR_SSL_WANT_WRITE;
}
