// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "ota_sig.h"
#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <string.h>
#include "cyd32_build_gen.h"

static_assert(sizeof(mbedtls_sha256_context) <= 128, "ctx storage too small");
namespace ota_sig {
namespace {
mbedtls_sha256_context *C(uint8_t *p) { return reinterpret_cast<mbedtls_sha256_context *>(p); }
bool verifyWith(const uint8_t key[65], const uint8_t hash[32], const uint8_t *sig, size_t sl) {
  mbedtls_ecdsa_context e;
  mbedtls_ecdsa_init(&e);
  bool ok = mbedtls_ecp_group_load(&e.grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
            mbedtls_ecp_point_read_binary(&e.grp, &e.Q, key, 65) == 0 &&
            mbedtls_ecp_check_pubkey(&e.grp, &e.Q) == 0 &&
            mbedtls_ecdsa_read_signature(&e, hash, 32, sig, sl) == 0;
  mbedtls_ecdsa_free(&e);
  return ok;
}
}  // namespace
void Verifier::begin() { mbedtls_sha256_init(C(ctx_)); mbedtls_sha256_starts_ret(C(ctx_), 0); }
void Verifier::update(const uint8_t *d, size_t n) { mbedtls_sha256_update_ret(C(ctx_), d, n); }
bool trailerPresent(const uint8_t t[kTrailer]) {
  return memcmp(t, "CYD32SG1", 8) == 0 && memcmp(t + 88, "CYD32END", 8) == 0;
}
Result Verifier::finish(const uint8_t t[kTrailer], int &slot) {
  uint8_t hash[32];
  mbedtls_sha256_finish_ret(C(ctx_), hash);
  mbedtls_sha256_free(C(ctx_));
  slot = -1;
  if (!trailerPresent(t)) return NO_TRAILER;
  const size_t sl = t[9];
  if (sl < 8 || sl > 72) return BAD_TRAILER;
  for (int i = 10; i < 16; i++) if (t[i]) return BAD_TRAILER;          // reserved bytes
  for (size_t i = 16 + sl; i < 88; i++) if (t[i]) return BAD_TRAILER;  // zero padding after DER
  if (t[8] > 1) return BAD_TRAILER;
  if (!CYD32_KEY0_PRESENT && !CYD32_KEY1_PRESENT) return NO_KEY;
  if (CYD32_KEY0_PRESENT && verifyWith(cyd32_key0, hash, t + 16, sl)) { slot = 0; return OK; }
  if (CYD32_KEY1_PRESENT && verifyWith(cyd32_key1, hash, t + 16, sl)) { slot = 1; return OK; }
  return BAD_SIG;
}
const char *resultStr(Result r) {
  switch (r) {
    case OK: return "signature OK";
    case NO_TRAILER: return "file is not signed";
    case BAD_TRAILER: return "invalid signature block";
    case NO_KEY: return "no public key in this firmware";
    default: return "invalid signature";
  }
}
}
