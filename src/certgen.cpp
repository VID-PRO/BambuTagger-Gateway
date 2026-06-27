#include "certgen.h"
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <string.h>
#include <stdio.h>

// Bambu custom OIDs: 1.3.6.1.4.1.39859.x (DER-encoded)
#define OID_BAMBU_1     "\x2B\x06\x01\x04\x01\x82\xB7\x33\x01"
#define OID_BAMBU_2     "\x2B\x06\x01\x04\x01\x82\xB7\x33\x02"
#define OID_BAMBU_1_3   "\x2B\x06\x01\x04\x01\x82\xB7\x33\x01\x03"
#define OID_BAMBU_1_4   "\x2B\x06\x01\x04\x01\x82\xB7\x33\x01\x04"
#define OID_BAMBU_1_999 "\x2B\x06\x01\x04\x01\x82\xB7\x33\x01\x87\x67"

bool generateCert(const char *cn, uint8_t *certDer, size_t *certLen,
                  uint8_t *keyDer, size_t *keyLen) {
  mbedtls_pk_context key;
  mbedtls_x509write_cert crt;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_entropy_context entropy;
  int ret;
  bool ok = false;

  mbedtls_pk_init(&key);
  mbedtls_x509write_crt_init(&crt);
  mbedtls_ctr_drbg_init(&drbg);
  mbedtls_entropy_init(&entropy);

  do {
    ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret) { printf("CERT: drbg seed failed -0x%x\n", -ret); break; }

    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret) { printf("CERT: pk setup failed -0x%x\n", -ret); break; }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key);
    ret = mbedtls_rsa_gen_key(rsa, mbedtls_ctr_drbg_random, &drbg, 2048, 65537);
    if (ret) { printf("CERT: rsa gen failed -0x%x\n", -ret); break; }

    {
      uint8_t keyTmp[2048];
      int wr = mbedtls_pk_write_key_der(&key, keyTmp, sizeof(keyTmp));
      if (wr <= 0) { printf("CERT: write key der failed %d\n", wr); break; }
      memcpy(keyDer, keyTmp + sizeof(keyTmp) - wr, wr);
      *keyLen = wr;
    }

    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);

    {
      mbedtls_mpi serial;
      mbedtls_mpi_init(&serial);
      mbedtls_mpi_lset(&serial, 1);
      ret = mbedtls_x509write_crt_set_serial(&crt, &serial);
      mbedtls_mpi_free(&serial);
      if (ret) { printf("CERT: set serial failed -0x%x\n", -ret); break; }
    }

    ret = mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20350101000000");
    if (ret) { printf("CERT: set validity failed -0x%x\n", -ret); break; }

    char nameStr[256];
    snprintf(nameStr, sizeof(nameStr),
             "CN=%s,O=BBL Technologies Co. Ltd,C=CN", cn);
    ret = mbedtls_x509write_crt_set_issuer_name(&crt, nameStr);
    if (ret) { printf("CERT: set issuer name failed -0x%x\n", -ret); break; }
    ret = mbedtls_x509write_crt_set_subject_name(&crt, nameStr);
    if (ret) { printf("CERT: set subject name failed -0x%x\n", -ret); break; }

    ret = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
    if (ret) { printf("CERT: set basic constraints failed -0x%x\n", -ret); break; }

    ret = mbedtls_x509write_crt_set_key_usage(&crt,
      MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_ENCIPHERMENT);
    if (ret) { printf("CERT: set key usage failed -0x%x\n", -ret); break; }

    ret = mbedtls_x509write_crt_set_subject_key_identifier(&crt);
    if (ret) { printf("CERT: set SKI failed -0x%x\n", -ret); break; }

    {
      static const uint8_t v1_val[] = { 0x0C, 0x02, 0x76, 0x31 };
      ret = mbedtls_x509write_crt_set_extension(&crt,
        OID_BAMBU_1, sizeof(OID_BAMBU_1) - 1, 0, v1_val, sizeof(v1_val));
      if (ret) { printf("CERT: set ext 39859.1 failed -0x%x\n", -ret); break; }

      static const uint8_t false_val[] = { 0x0C, 0x05, 0x66, 0x61, 0x6C, 0x73, 0x65 };
      ret = mbedtls_x509write_crt_set_extension(&crt,
        OID_BAMBU_2, sizeof(OID_BAMBU_2) - 1, 0, false_val, sizeof(false_val));
      if (ret) { printf("CERT: set ext 39859.2 failed -0x%x\n", -ret); break; }

      static const uint8_t printer_val[] = { 0x0C, 0x07, 0x50, 0x72, 0x69, 0x6E, 0x74, 0x65, 0x72 };
      ret = mbedtls_x509write_crt_set_extension(&crt,
        OID_BAMBU_1_3, sizeof(OID_BAMBU_1_3) - 1, 0, printer_val, sizeof(printer_val));
      if (ret) { printf("CERT: set ext 39859.1.3 failed -0x%x\n", -ret); break; }

      static const uint8_t n7v2_val[] = { 0x0C, 0x05, 0x4E, 0x37, 0x2D, 0x56, 0x32 };
      ret = mbedtls_x509write_crt_set_extension(&crt,
        OID_BAMBU_1_4, sizeof(OID_BAMBU_1_4) - 1, 0, n7v2_val, sizeof(n7v2_val));
      if (ret) { printf("CERT: set ext 39859.1.4 failed -0x%x\n", -ret); break; }

      static const uint8_t int0_val[] = { 0x02, 0x01, 0x00 };
      ret = mbedtls_x509write_crt_set_extension(&crt,
        OID_BAMBU_1_999, sizeof(OID_BAMBU_1_999) - 1, 0, int0_val, sizeof(int0_val));
      if (ret) { printf("CERT: set ext 39859.1.999 failed -0x%x\n", -ret); break; }
    }

    {
      uint8_t buf[2048];
      int len = mbedtls_x509write_crt_der(&crt, buf, sizeof(buf),
                                           mbedtls_ctr_drbg_random, &drbg);
      if (len <= 0) { printf("CERT: write der failed %d\n", len); break; }
      memcpy(certDer, buf + sizeof(buf) - len, len);
      *certLen = len;
      printf("CERT: generated OK (%d bytes)\n", len);
      ok = true;
    }
  } while (0);

  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&key);
  mbedtls_ctr_drbg_free(&drbg);
  mbedtls_entropy_free(&entropy);

  if (!ok) { *certLen = 0; *keyLen = 0; }
  return ok;
}
