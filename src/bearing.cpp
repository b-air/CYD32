// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "bearing.h"
namespace bearing {
uint32_t fnv1a(const uint8_t *p, int n) {
  uint32_t h = 0x811C9DC5u;
  for (int i = 0; i < n; i++) { h ^= p[i]; h *= 0x01000193u; }
  return h;
}
uint16_t fromMac(const uint8_t mac[6]) { return (uint16_t)(((uint64_t)fnv1a(mac, 6) * 360u) >> 32); }
uint8_t cellFromMac(const uint8_t mac[6]) { return (uint8_t)((fnv1a(mac, 6) >> 8) % 80u); }
}
