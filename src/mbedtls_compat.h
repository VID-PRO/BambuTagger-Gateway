#pragma once

// ESP-IDF v4.x / mbedTLS 2.x
#if __has_include(<mbedtls/ssl_internal.h>)
#include <mbedtls/ssl_internal.h>

// ESP-IDF v5.x / mbedTLS 3.x
#elif __has_include(<mbedtls/ssl_misc.h>)
#include <mbedtls/ssl_misc.h>

// Internal headers not in include path — define ssl_transform ourselves.
#else

#include <mbedtls/cipher.h>
#include <mbedtls/md.h>

// Must match the mbedTLS 2.x / ESP-IDF v4.4 layout exactly.
// This is the only version where internal headers are missing.
struct mbedtls_ssl_transform {
    size_t minlen;
    size_t ivlen;
    size_t fixed_ivlen;
    size_t maclen;
    size_t taglen;
    unsigned char iv_enc[16];
    unsigned char iv_dec[16];

#if defined(MBEDTLS_SSL_SOME_MODES_USE_MAC)
#if defined(MBEDTLS_SSL_PROTO_SSL3)
    unsigned char mac_enc[20];
    unsigned char mac_dec[20];
#endif
    mbedtls_md_context_t md_ctx_enc;
    mbedtls_md_context_t md_ctx_dec;
#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
    int encrypt_then_mac;
#endif
#endif

    mbedtls_cipher_context_t cipher_ctx_enc;
    mbedtls_cipher_context_t cipher_ctx_dec;
    int minor_ver;
};

#endif
