#pragma once
#include <stdint.h>
#include <stddef.h>

bool generateCertChain(const char *cn,
                       uint8_t *certDer, size_t *certLen,
                       uint8_t *keyDer, size_t *keyLen,
                       uint8_t *caDer, size_t *caLen,
                       const uint8_t *ip = nullptr);
