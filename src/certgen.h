#pragma once
#include <stdint.h>
#include <stddef.h>

bool generateCert(const char *cn, uint8_t *certDer, size_t *certLen,
                  uint8_t *keyDer, size_t *keyLen);
