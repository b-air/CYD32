/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Just enough of the mbed TLS SHA-256 API for src/privacy.cpp. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mbedtls_sha256_context {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t buffer[64];
  uint32_t buflen;
} mbedtls_sha256_context;
void mbedtls_sha256_init(mbedtls_sha256_context *ctx);
int mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int is224);
int mbedtls_sha256_update_ret(mbedtls_sha256_context *ctx, const unsigned char *input, size_t ilen);
int mbedtls_sha256_finish_ret(mbedtls_sha256_context *ctx, unsigned char output[32]);
void mbedtls_sha256_free(mbedtls_sha256_context *ctx);
#ifdef __cplusplus
}
#endif
