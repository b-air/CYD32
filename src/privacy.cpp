// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "privacy.h"
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>
namespace {
uint8_t salt[32];
}
namespace privacy {
void begin() { esp_fill_random(salt, sizeof salt); }
uint32_t macId(const uint8_t mac[6]) {
  uint8_t h[32];
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  mbedtls_sha256_starts_ret(&c, 0);
  mbedtls_sha256_update_ret(&c, salt, sizeof salt);
  mbedtls_sha256_update_ret(&c, mac, 6);
  mbedtls_sha256_finish_ret(&c, h);
  mbedtls_sha256_free(&c);
  uint32_t id; memcpy(&id, h, 4);
  return id;
}
void idToStr(uint32_t id, char out[9]) { snprintf(out, 9, "%08lx", (unsigned long)id); }
}
