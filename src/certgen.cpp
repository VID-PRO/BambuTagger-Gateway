#include "certgen.h"
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/oid.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <string.h>

static int writeOidValue(unsigned char **p, unsigned char *start,
                         size_t baseLen, const char *oid, size_t oidLen,
                         const char *val, size_t valLen, int valTag) {
  int ret;
  size_t len = baseLen;
  size_t before = len;
  // Content: OID + value
  if (valTag == MBEDTLS_ASN1_UTF8_STRING) {
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_utf8_string(p, start, val, valLen));
  } else {
    *p -= valLen; memcpy(*p, val, valLen); len += valLen;
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, valLen));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, valTag));
  }
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(p, start, oid, oidLen));
  // SEQUENCE wrapping
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len - before));
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start,
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));
  // SET wrapping
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len - before));
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start,
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET));
  return (int)(len - baseLen);
}

static int writeName(unsigned char **p, unsigned char *start,
                     const char *cn, size_t cnLen,
                     const char *org, size_t orgLen,
                     const char *country, size_t countryLen,
                     size_t baseLen) {
  int ret;
  size_t len = baseLen;
  size_t before = len;
  // RDNs in reverse (right-to-left building): C, O, CN
  MBEDTLS_ASN1_CHK_ADD(len, writeOidValue(p, start, len,
                       MBEDTLS_OID_AT_COUNTRY, strlen(MBEDTLS_OID_AT_COUNTRY),
                       country, countryLen, MBEDTLS_ASN1_PRINTABLE_STRING));
  MBEDTLS_ASN1_CHK_ADD(len, writeOidValue(p, start, len,
                       MBEDTLS_OID_AT_ORGANIZATION, strlen(MBEDTLS_OID_AT_ORGANIZATION),
                       org, orgLen, MBEDTLS_ASN1_UTF8_STRING));
  MBEDTLS_ASN1_CHK_ADD(len, writeOidValue(p, start, len,
                       MBEDTLS_OID_AT_CN, strlen(MBEDTLS_OID_AT_CN),
                       cn, cnLen, MBEDTLS_ASN1_UTF8_STRING));
  // Outer Name SEQUENCE
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len - before));
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start,
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));
  return (int)(len - baseLen);
}

static int writeUtcTime(unsigned char **p, unsigned char *start,
                        const char *timeStr) {
  int ret;
  size_t len = 0;
  size_t slen = strlen(timeStr);
  if (*p < start + slen) return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
  *p -= slen; memcpy(*p, timeStr, slen); len += slen;
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, slen));
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_UTC_TIME));
  return (int)len;
}

static int writeExtension(unsigned char **p, unsigned char *start,
                          size_t baseLen, const char *oid, size_t oidLen,
                          int critical, const uint8_t *val, size_t valLen) {
  int ret;
  size_t len = baseLen;
  size_t before = len;
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_octet_string(p, start, val, valLen));
  if (critical) {
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_bool(p, start, 1));
  }
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(p, start, oid, oidLen));
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len - before));
  MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start,
                       MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));
  return (int)(len - baseLen);
}

bool generateCert(const char *cn, uint8_t *certDer, size_t *certLen,
                  uint8_t *keyDer, size_t *keyLen) {
  mbedtls_pk_context pk;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_entropy_context entropy;
  int ret;
  bool ok = false;

  mbedtls_pk_init(&pk);
  mbedtls_ctr_drbg_init(&drbg);
  mbedtls_entropy_init(&entropy);

  do {
    ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret) break;

    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret) break;

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);
    ret = mbedtls_rsa_gen_key(rsa, mbedtls_ctr_drbg_random, &drbg, 2048, 65537);
    if (ret) break;

    {
      uint8_t keyTmp[2048];
      int wr = mbedtls_pk_write_key_der(&pk, keyTmp, sizeof(keyTmp));
      if (wr <= 0) break;
      memcpy(keyDer, keyTmp + sizeof(keyTmp) - wr, wr);
      *keyLen = wr;
    }

    // ----- Build TBSCertificate DER (right to left) -----
    uint8_t tbs[2048];
    unsigned char *p = tbs + sizeof(tbs);
    size_t cnLen = strlen(cn);
    const char *org = "BBL Technologies Co., Ltd";
    size_t orgLen = strlen(org);
    const char *country = "CN";
    size_t countryLen = strlen(country);
    size_t len = 0;

    // 7) Extensions: [3] { SEQUENCE { BasicConstraints, KeyUsage } }
    // Written FIRST (rightmost) since all mbedtls_asn1_write_* prepend.
    {
      size_t extBefore = 0;
      uint8_t keyUsageVal[] = { 0x03, 0x02, 0x05, 0xA0 };
      MBEDTLS_ASN1_CHK_ADD(extBefore, writeExtension(&p, tbs, extBefore,
                           MBEDTLS_OID_KEY_USAGE, strlen(MBEDTLS_OID_KEY_USAGE),
                           1, keyUsageVal, sizeof(keyUsageVal)));
      uint8_t basicConVal[] = { 0x30, 0x00 };
      MBEDTLS_ASN1_CHK_ADD(extBefore, writeExtension(&p, tbs, extBefore,
                           MBEDTLS_OID_BASIC_CONSTRAINTS, strlen(MBEDTLS_OID_BASIC_CONSTRAINTS),
                           1, basicConVal, sizeof(basicConVal)));
      MBEDTLS_ASN1_CHK_ADD(extBefore, mbedtls_asn1_write_len(&p, tbs, extBefore));
      MBEDTLS_ASN1_CHK_ADD(extBefore, mbedtls_asn1_write_tag(&p, tbs,
                           MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));
      MBEDTLS_ASN1_CHK_ADD(extBefore, mbedtls_asn1_write_len(&p, tbs, extBefore));
      MBEDTLS_ASN1_CHK_ADD(extBefore, mbedtls_asn1_write_tag(&p, tbs, 0xA3));
      len = extBefore;
    }

    // 7b) SubjectPublicKeyInfo — prepend just before Extensions (field 7 in TBS order)
    {
      uint8_t spkiBuf[1024];
      int spkiLen = mbedtls_pk_write_pubkey_der(&pk, spkiBuf, sizeof(spkiBuf));
      if (spkiLen <= 0) break;
      size_t spkiLenU = spkiLen;
      p -= spkiLenU;
      memmove(p, spkiBuf + sizeof(spkiBuf) - spkiLenU, spkiLenU);
      len += spkiLenU;
    }

    // 6) Subject Name
    MBEDTLS_ASN1_CHK_ADD(len, writeName(&p, tbs, cn, cnLen, org, orgLen, country, countryLen, len));

    // 5) Validity
    size_t valBefore = len;
    MBEDTLS_ASN1_CHK_ADD(len, writeUtcTime(&p, tbs, "350101000000Z"));
    MBEDTLS_ASN1_CHK_ADD(len, writeUtcTime(&p, tbs, "240101000000Z"));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&p, tbs, len - valBefore));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&p, tbs,
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    // 4) Issuer Name
    MBEDTLS_ASN1_CHK_ADD(len, writeName(&p, tbs, cn, cnLen, org, orgLen, country, countryLen, len));

    // 3) Signature Algorithm
    size_t sigAlgBefore = len;
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_null(&p, tbs));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(&p, tbs,
                         MBEDTLS_OID_PKCS1_SHA256, strlen(MBEDTLS_OID_PKCS1_SHA256)));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&p, tbs, len - sigAlgBefore));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&p, tbs,
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    // 2) Serial Number
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_int(&p, tbs, 1));

    // 1) Version: [0] EXPLICIT INTEGER 2 (v3)
    size_t verBefore = len;
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_int(&p, tbs, 2));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&p, tbs, len - verBefore));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&p, tbs, 0xA0));

    // TBSCertificate SEQUENCE wrapping
    size_t tbsPayload = (tbs + sizeof(tbs)) - p;
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&p, tbs, tbsPayload));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&p, tbs,
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    size_t tbsDerLen = (tbs + sizeof(tbs)) - p;
    uint8_t *tbsStart = p;

    // Hash TBSCertificate
    uint8_t hash[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, tbsStart, tbsDerLen);
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);

    // Sign hash
    uint8_t sig[256];
    size_t sigLen = 0;
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, sig, &sigLen,
                           mbedtls_ctr_drbg_random, &drbg);
    if (ret || sigLen == 0) break;

    // ----- Build final Certificate: { TBS, SigAlg, BIT STRING sig } -----
    uint8_t *cp = certDer + 2048;
    len = 0;

    // BIT STRING for signature
    cp -= sigLen; memcpy(cp, sig, sigLen); len += sigLen;
    cp -= 1; cp[0] = 0; len += 1;
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&cp, certDer, sigLen + 1));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&cp, certDer, MBEDTLS_ASN1_BIT_STRING));
    unsigned char *bitstringStart = cp;

    // SignatureAlgorithm SEQUENCE { OID sha256WithRSA, NULL }
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_null(&cp, certDer));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(&cp, certDer,
                         MBEDTLS_OID_PKCS1_SHA256, strlen(MBEDTLS_OID_PKCS1_SHA256)));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&cp, certDer, bitstringStart - cp));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&cp, certDer,
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    // TBS bytes
    cp -= tbsDerLen; memcpy(cp, tbsStart, tbsDerLen); len += tbsDerLen;

    // Outer Certificate SEQUENCE
    size_t certPayload = (certDer + 2048) - cp;
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(&cp, certDer, certPayload));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(&cp, certDer,
                         MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    *certLen = (certDer + 2048) - cp;
    memmove(certDer, cp, *certLen);
    ok = true;
  } while (0);

  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&drbg);
  mbedtls_entropy_free(&entropy);

  if (!ok) { *certLen = 0; *keyLen = 0; }
  return ok;
}
