#include "certgen.h"
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/oid.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <string.h>

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

    // Write private key DER (writes at end of buffer, returns length)
    *keyLen = mbedtls_pk_write_key_der(&pk, keyDer, 2048);
    if (*keyLen <= 0) break;

    // Build TBSCertificate DER using asn1write (right to left)
    uint8_t tbs[2048];
    unsigned char *p = tbs + sizeof(tbs);
    size_t cnLen = strlen(cn);

    // 1) SubjectPublicKeyInfo (innermost of TBSCertificate elements)
    int spkiLen = mbedtls_pk_write_pubkey_der(&pk, tbs, sizeof(tbs));
    if (spkiLen <= 0) break;
    p = tbs + sizeof(tbs) - spkiLen;

    // 2) Subject Name: SET { SEQUENCE { OID CN, UTF8String cn } }
    ret = mbedtls_asn1_write_utf8_string(&p, tbs, cn, cnLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_oid(&p, tbs, MBEDTLS_OID_AT_CN, strlen(MBEDTLS_OID_AT_CN));
    if (ret < 0) break;
    // SEQUENCE wrapping OID + UTF8String
    size_t atvLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, atvLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;
    // SET wrapping the SEQUENCE
    size_t rdnLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, rdnLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SET);
    if (ret < 0) break;

    // 3) Validity: SEQUENCE { UTCTime notBefore, UTCTime notAfter }
    // Use fixed dates: 2024-01-01 to 2034-01-01
    ret = mbedtls_asn1_write_printable_string(&p, tbs, "350101000000Z", 13);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_printable_string(&p, tbs, "240101000000Z", 13);
    if (ret < 0) break;
    size_t valLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, valLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;

    // 4) Issuer Name (same as Subject)
    ret = mbedtls_asn1_write_utf8_string(&p, tbs, cn, cnLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_oid(&p, tbs, MBEDTLS_OID_AT_CN, strlen(MBEDTLS_OID_AT_CN));
    if (ret < 0) break;
    atvLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, atvLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;
    rdnLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, rdnLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SET);
    if (ret < 0) break;

    // 5) Signature Algorithm: SEQUENCE { OID sha256WithRSA, NULL }
    ret = mbedtls_asn1_write_null(&p, tbs);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_oid(&p, tbs, MBEDTLS_OID_PKCS1_SHA256, strlen(MBEDTLS_OID_PKCS1_SHA256));
    if (ret < 0) break;
    size_t sigAlgLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, sigAlgLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;

    // 6) Serial Number: INTEGER 1
    ret = mbedtls_asn1_write_int(&p, tbs, 1);
    if (ret < 0) break;

    // 7) Version: [0] EXPLICIT INTEGER 2 (v3)
    ret = mbedtls_asn1_write_int(&p, tbs, 2);
    if (ret < 0) break;
    size_t verLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, verLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, 0xA0);
    if (ret < 0) break;

    // Wrap everything in TBSCertificate SEQUENCE
    size_t tbsLen = (tbs + sizeof(tbs)) - p;
    ret = mbedtls_asn1_write_len(&p, tbs, tbsLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&p, tbs, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;

    // tbs now contains the complete TBSCertificate DER at position p
    size_t finalTbsLen = (tbs + sizeof(tbs)) - p;
    uint8_t *tbsStart = p;

    // Hash TBSCertificate with SHA256
    uint8_t hash[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, tbsStart, finalTbsLen);
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);

    // Sign hash with RSA private key
    uint8_t sig[256];
    size_t sigLen = 0;
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, sig, &sigLen,
                           mbedtls_ctr_drbg_random, &drbg);
    if (ret || sigLen == 0) break;

    // Build final Certificate: SEQUENCE { TBS, SigAlg, BIT STRING sig }
    uint8_t *cp = certDer + 2048;

    // Signature BIT STRING
    // First byte of BIT STRING payload is unused bits count (0)
    ret = mbedtls_asn1_write_len(&cp, certDer, sigLen + 1);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&cp, certDer, MBEDTLS_ASN1_BIT_STRING);
    if (ret < 0) break;
    // Copy signature after BIT STRING header
    cp -= sigLen;
    if (cp < certDer) break;
    memcpy(cp, sig, sigLen);
    // Unused bits byte
    cp -= 1;
    if (cp < certDer) break;
    cp[0] = 0;

    // Signature Algorithm (outer)
    ret = mbedtls_asn1_write_null(&cp, certDer);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_oid(&cp, certDer, MBEDTLS_OID_PKCS1_SHA256,
                                  strlen(MBEDTLS_OID_PKCS1_SHA256));
    if (ret < 0) break;
    size_t outerSigLen = (certDer + 2048) - cp;
    ret = mbedtls_asn1_write_len(&cp, certDer, outerSigLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&cp, certDer, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;

    // TBSCertificate
    size_t tbsDerLen = finalTbsLen;
    cp -= tbsDerLen;
    if (cp < certDer) break;
    memcpy(cp, tbsStart, tbsDerLen);

    // Outer SEQUENCE wrapping everything
    size_t certTotalLen = (certDer + 2048) - cp;
    ret = mbedtls_asn1_write_len(&cp, certDer, certTotalLen);
    if (ret < 0) break;
    ret = mbedtls_asn1_write_tag(&cp, certDer, MBEDTLS_ASN1_SEQUENCE);
    if (ret < 0) break;

    *certLen = (certDer + 2048) - cp;
    memmove(certDer, cp, *certLen);
    ok = true;
  } while (0);

  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&drbg);
  mbedtls_entropy_free(&entropy);

  if (!ok) {
    *certLen = 0;
    *keyLen = 0;
  }
  return ok;
}
