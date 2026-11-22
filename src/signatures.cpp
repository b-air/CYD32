// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "signatures.h"
#include <ctype.h>
#include <string.h>
namespace {
// --- TRK: item trackers ---------------------------------------------------
// Bluetooth SIG 16-bit UUIDs: 0xFD5A Samsung SmartTag, 0xFEED / 0xFEEC /
// 0xFD84 Tile, 0xFCB2 DULT (IETF "Detecting Unwanted Location Trackers",
// cross-vendor). 0xFDAE is kept from v0.2; its source is not documented
// (listed as an open question in docs/SIGNATURES.md).
const uint16_t kTrkUuid[] = {0xFD5A, 0xFEED, 0xFEEC, 0xFD84, 0xFCB2, 0xFDAE};
// Apple (company 0x004C) Find My "offline finding" payload type 0x12
// (AirTag and Find My accessories away from their owner). Type 0x10 is
// Nearby Info, sent by every iPhone, and is NOT a tracker (v0.2 counted it).
constexpr uint16_t kApple = 0x004C;
constexpr uint8_t kFindMy = 0x12;
// --- META: camera glasses -------------------------------------------------
// 0xFD5F Meta Platforms Technologies (Ray-Ban Meta), 0xFEB7 / 0xFEB8 Meta
// Platforms. Company ids 0x01AB Meta Platforms, 0x0D53 Luxottica (Ray-Ban /
// Oakley Meta maker). A Quest headset also says 0x01AB: the badge reads
// "META", not "glasses".
const uint16_t kMetaUuid[] = {0xFD5F, 0xFEB7, 0xFEB8};
const uint16_t kMetaCompany[] = {0x01AB, 0x0D53};
// --- FLOCK: Flock Safety cameras -----------------------------------------
// IEEE MA-L B4:1E:52 is registered to Flock Safety. SSIDs "flock-..." (setup
// network) and "Flock Camera..." (camera hotspot). BLE: company 0x09C8
// (XUNTONG, the radio in Flock's battery packs) and the advertised names
// "Flock..." / "Penguin..." (battery packs). Lower confidence than the OUI.
const uint8_t kFlockOui[][3] = {{0xB4, 0x1E, 0x52}};
constexpr uint16_t kFlockCompany = 0x09C8;
bool startsWithNoCase(const char *s, size_t n, const char *prefix) {
  const size_t m = strlen(prefix);
  if (n < m) return false;
  for (size_t i = 0; i < m; i++) if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) return false;
  return true;
}
template <size_t N> bool inList(uint16_t v, const uint16_t (&l)[N]) {
  for (uint16_t x : l) if (x == v) return true;
  return false;
}
}  // namespace
namespace sig {
uint8_t ble(const uint8_t *mfg, size_t mfgLen, const uint16_t *uuids16, int nUuids, const char *name) {
  uint8_t b = 0;
  if (mfg && mfgLen >= 2) {
    const uint16_t company = (uint16_t)(mfg[0] | (mfg[1] << 8));
    if (company == kApple && mfgLen >= 3 && mfg[2] == kFindMy) b |= TRK;
    if (inList(company, kMetaCompany)) b |= META;
    if (company == kFlockCompany) b |= FLOCK;
  }
  for (int i = 0; i < nUuids; i++) {
    if (inList(uuids16[i], kTrkUuid)) b |= TRK;
    if (inList(uuids16[i], kMetaUuid)) b |= META;
  }
  if (name && name[0]) {
    const size_t n = strlen(name);
    if (startsWithNoCase(name, n, "flock") || startsWithNoCase(name, n, "penguin")) b |= FLOCK;
  }
  return b;
}
uint8_t wifi(const uint8_t mac[6], const uint8_t *ssid, int ssidLen) {
  uint8_t b = 0;
  for (const auto &o : kFlockOui) if (mac[0] == o[0] && mac[1] == o[1] && mac[2] == o[2]) b |= FLOCK;
  if (ssid && ssidLen > 0) {
    const char *s = reinterpret_cast<const char *>(ssid);
    if (startsWithNoCase(s, (size_t)ssidLen, "flock-") || startsWithNoCase(s, (size_t)ssidLen, "flock camera")) b |= FLOCK;
  }
  return b;
}
}
