// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Signed-image trailer appended by scripts/sign.sh (96 bytes, fixed size):
//   off  0  8  magic "CYD32SG1"
//   off  8  1  key slot hint (0 or 1, informative only; both slots are tried)
//   off  9  1  DER signature length (<= 72)
//   off 10  6  zero
//   off 16 72  ECDSA P-256 / SHA-256 signature, DER, zero padded
//   off 88  8  magic "CYD32END"
// The signature covers every byte of the .bin before the trailer.
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace ota_sig {
constexpr size_t kTrailer = 96;
enum Result { OK = 0, NO_TRAILER, BAD_TRAILER, NO_KEY, BAD_SIG };
struct Verifier {
  void begin();
  // Feed image bytes (everything except the trailer).
  void update(const uint8_t *d, size_t n);
  // Verify against the build-time key slots. keySlot receives the slot used.
  Result finish(const uint8_t trailer[kTrailer], int &keySlot);
  uint8_t ctx_[128];  // opaque mbedtls_sha256_context storage
};
bool trailerPresent(const uint8_t trailer[kTrailer]);
const char *resultStr(Result r);
}
