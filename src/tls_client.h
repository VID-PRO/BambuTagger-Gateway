#pragma once

#ifdef ESP32
#include <WiFi.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

class TlsWiFiClient : public WiFiClient {
public:
  TlsWiFiClient();
  ~TlsWiFiClient();

  bool begin(WiFiClient *tcp, const char *certPem, const char *keyPem);
  bool beginDer(WiFiClient *tcp, const uint8_t *certDer, size_t certLen,
                const uint8_t *keyDer, size_t keyLen);
  int continueHandshake();  // 1=done, 0=busy, -1=error
  bool isTls() { return _ok && _tcp; }
  bool isOk() { return _ok; }
  bool handshakeDone() { return _hsDone; }
  int rawAvailable() { return _tcp ? _tcp->available() : 0; }
  int rawRead(uint8_t *buf, size_t size) { return _tcp ? _tcp->read(buf, size) : 0; }
  int readWantCnt() { return _readWantCnt; }
  int readCloseNotifyCnt() { return _readCloseNotify; }
  void tcpSetNoDelay(bool nodelay) { if (_tcp) _tcp->setNoDelay(nodelay); }
  void clearKeyInHardware();
  void feedData(uint8_t b) { _feedBuf[_feedLen++] = b; }
  void bufferReadData(const uint8_t *data, size_t len) {
    // Only store if buffer is empty — otherwise data from read() takes priority
    if (_rlen == 0 && len > 0) {
      if (len > sizeof(_rbuf)) len = sizeof(_rbuf);
      memcpy(_rbuf, data, len);
      _rlen = len;
      _rpos = 0;
    }
  }
  size_t internalBufAvail() { return _rlen; }

  void stop() override;
  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buf, size_t size) override;
  int available() override;
  int read() override;
  int read(uint8_t *buf, size_t size) override;
  int peek() override;
  void flush() override;
  void flushWrites();
  uint8_t connected() override;
  operator bool() override;

  using WiFiClient::remoteIP;
  using WiFiClient::remotePort;

private:
  WiFiClient *_tcp = nullptr;
  bool _ok = false;
  bool _hsDone = false;
  int _hsRetries = 0;
  unsigned long _hsStart = 0;
  int _hsState = 0;
  int _readWantCnt = 0;
  int _readCloseNotify = 0;
  int _bioLogCnt = 0;
  uint16_t _dumpLen = 0;
  uint8_t _dumpBuf[128];
  uint16_t _sndDumpLen = 0;
  uint8_t _sndDumpBuf[512];
  uint8_t _feedBuf[64];
  uint8_t _feedLen = 0;
  // Internal read buffer — always fetches full TLS record (up to 512 bytes)
  // to work around mbedtls_ssl_read() returning WANT_READ with 1-byte buffers
  uint8_t _rbuf[512];
  size_t _rlen = 0;
  size_t _rpos = 0;
  mbedtls_ssl_context _ssl;
  mbedtls_ssl_config _conf;
  mbedtls_x509_crt _cert;
  mbedtls_pk_context _pkey;
  mbedtls_entropy_context _entropy;
  mbedtls_ctr_drbg_context _drbg;

  static int bio_recv(void *ctx, unsigned char *buf, size_t len);
  static int bio_send(void *ctx, const unsigned char *buf, size_t len);
};

#endif
