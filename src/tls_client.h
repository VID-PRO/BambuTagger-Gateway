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
  bool handshakeDone() { return _hsDone; }
  void tcpSetNoDelay(bool nodelay) { if (_tcp) _tcp->setNoDelay(nodelay); }
  void feedData(uint8_t b) { _feedBuf[_feedLen++] = b; }

  void stop() override;
  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buf, size_t size) override;
  int available() override;
  int read() override;
  int read(uint8_t *buf, size_t size) override;
  int peek() override;
  void flush() override;
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
  uint16_t _dumpLen = 0;
  uint8_t _dumpBuf[128];
  uint16_t _sndDumpLen = 0;
  uint8_t _sndDumpBuf[512];
  uint8_t _feedBuf[64];
  uint8_t _feedLen = 0;
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
