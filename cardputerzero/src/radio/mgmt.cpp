// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Userspace twin of the ESP32 management filter (src/scanner.cpp): radiotap +
// 802.11, then beacons, probe responses and probe requests only. Data, control
// and other management subtypes are dropped. Nothing here transmits.
#include "radio.hpp"
#include "bearing.h"
#include "privacy.h"
#include "signatures.h"
#include <string.h>
namespace cz {
namespace {
uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool has_remote_id(const uint8_t *ies, int len) {
  int p = 0;
  while (p + 2 <= len) {
    const uint8_t id = ies[p], l = ies[p + 1];
    if (p + 2 + l > len) break;
    if (id == 221 && l >= 4 && ies[p + 2] == 0xFA && ies[p + 3] == 0x0B && ies[p + 4] == 0xBC && ies[p + 5] == 0x0D) return true;
    p += 2 + l;
  }
  return false;
}

uint8_t classify_ies(const uint8_t *ies, int len, uint16_t cap, bool ap) {
  uint8_t bits = 0;
  bool hidden = false, has_rsn = false, has_wpa = false, hot = false;
  int p = 0;
  while (p + 2 <= len) {
    const uint8_t id = ies[p], l = ies[p + 1];
    if (p + 2 + l > len) break;
    const uint8_t *d = ies + p + 2;
    if (id == 0 && l == 0) hidden = true;
    else if (id == 48) has_rsn = true;
    else if (id == 107) hot = true;
    else if (id == 221 && l >= 3) {
      if (d[0] == 0x00 && d[1] == 0x50 && d[2] == 0xF2 && l >= 4 && d[3] == 0x01) has_wpa = true;
      if (d[0] == 0x50 && d[1] == 0x6F && d[2] == 0x9A) hot = true;
      if (d[0] == 0x00 && d[1] == 0x17 && d[2] == 0xF2) hot = true;
    } else if (id == 255 && l >= 1) {
      const uint8_t ext = d[0];
      if (ext == 35 || ext == 36) bits |= det::traitBit(det::WIFI6);
      if (ext == 106 || ext == 107 || ext == 108) bits |= det::traitBit(det::WIFI7);
    }
    p += 2 + l;
  }
  if (ap) {
    bits |= det::traitBit(det::AP);
    if (hot || hidden) bits |= det::traitBit(det::HOT);
    if (!(cap & 0x0010) && !has_rsn && !has_wpa) bits |= det::traitBit(det::OPEN);
  }
  return bits;
}

uint8_t band_bit(uint16_t mhz) {
  if (mhz >= 2400 && mhz < 2500) return det::traitBit(det::G24);
  if (mhz >= 5000 && mhz < 5925) return det::traitBit(det::G5);
  return 0;  // 6 GHz has no trait bit in the shared table
}

uint8_t channel_of(uint16_t mhz) {
  if (mhz == 2484) return 14;
  if (mhz >= 2412 && mhz <= 2472) return (uint8_t)((mhz - 2407) / 5);
  if (mhz >= 5000 && mhz < 5925) return (uint8_t)((mhz - 5000) / 5);
  if (mhz >= 5955 && mhz < 7120) return (uint8_t)((mhz - 5950) / 5);
  return 0;
}

struct RadioMeta {
  uint16_t mhz = 0;
  int8_t rssi = 0;
  bool fcs = false;
  size_t skip = 0;
};

bool radiotap(const uint8_t *p, size_t n, RadioMeta &m) {
  if (n < 8 || p[0] != 0) return false;
  const uint16_t it_len = rd16(p + 2);
  if (it_len < 8 || it_len > n) return false;
  const uint32_t first = rd32(p + 4);
  size_t data = 8;
  uint32_t w = first;
  while (w & 0x80000000u) {
    if (data + 4 > it_len) return false;
    w = rd32(p + data);
    data += 4;
  }
  size_t off = data;
  auto align = [&](size_t a) { off = (off + a - 1) & ~(a - 1); };
  if (first & (1u << 0)) { align(8); off += 8; }                  // TSFT
  if (first & (1u << 1)) {                                        // FLAGS
    if (off < it_len && (p[off] & 0x10)) m.fcs = true;
    off += 1;
  }
  if (first & (1u << 2)) off += 1;                                // RATE
  if (first & (1u << 3)) {                                        // CHANNEL
    align(2);
    if (off + 2 <= it_len) m.mhz = rd16(p + off);
    off += 4;
  }
  if (first & (1u << 4)) { align(2); off += 2; }                  // FHSS
  if (first & (1u << 5)) {                                        // DBM_ANTSIGNAL
    if (off < it_len) m.rssi = (int8_t)p[off];
  }
  m.skip = it_len;
  return true;
}

void take_ssid(const uint8_t *ies, int len, const uint8_t *&ssid, int &ssid_len) {
  ssid = nullptr;
  ssid_len = 0;
  if (len >= 2 && ies[0] == 0 && ies[1] <= 32 && 2 + ies[1] <= len) {
    ssid = ies + 2;
    ssid_len = ies[1];
  }
}
}  // namespace

bool parse_monitor_frame(const uint8_t *p, size_t n, RxFrame &out) {
  out = RxFrame{};
  RadioMeta meta;
  if (!radiotap(p, n, meta)) return false;
  out.ok = true;
  out.mhz = meta.mhz;
  out.rssi = meta.rssi;
  out.fcs = meta.fcs;
  out.channel = channel_of(meta.mhz);
  const uint8_t *f = p + meta.skip;
  size_t len = n - meta.skip;
  if (meta.fcs) {
    if (len < 4) return true;
    len -= 4;
  }
  if (len < 24) return true;
  const uint8_t fc0 = f[0];
  const uint8_t type = (fc0 >> 2) & 0x03;
  const uint8_t subtype = (fc0 >> 4) & 0x0f;
  if (type != 0) return true;  // not management
  const uint8_t *mac = f + 10;
  memcpy(out.mac, mac, 6);
  const uint8_t band = band_bit(meta.mhz);
  if (subtype == 8 || subtype == 5) {
    const int ie_len = (int)len > 36 ? (int)len - 36 : 0;
    const bool rid = subtype == 8 && ie_len > 0 && has_remote_id(f + 36, ie_len);
    uint16_t cap = 0;
    if (len >= 36) cap = rd16(f + 34);
    uint8_t traits = band;
    if (!rid && ie_len > 0) traits |= classify_ies(f + 36, ie_len, cap, true);
    const uint8_t *ssid = nullptr;
    int ssid_len = 0;
    if (ie_len > 0) take_ssid(f + 36, ie_len, ssid, ssid_len);
    uint8_t badges = sig::wifi(mac, ssid, ssid_len);
    if (bearing::isRandomized(mac)) badges |= sig::RANDOM_ADDR;
    out.accepted = true;
    out.kind = rid ? det::DRONE : det::WIFI;
    out.traits = traits;
    out.badges = badges;
  } else if (subtype == 4) {
    const int ie_len = (int)len > 24 ? (int)len - 24 : 0;
    uint8_t traits = band;
    if (ie_len > 0) traits |= classify_ies(f + 24, ie_len, 0, false);
    uint8_t badges = sig::wifi(mac, nullptr, 0);
    if (bearing::isRandomized(mac)) badges |= sig::RANDOM_ADDR;
    out.accepted = true;
    out.kind = det::WIFI;
    out.traits = traits;
    out.badges = badges;
  }
  return true;
}

namespace {
struct Note { uint32_t id; uint16_t mhz; };
Note notes[128];
int note_i = 0;
}  // namespace

void ingest_frame(const RxFrame &f) {
  if (!f.accepted) return;
  const uint32_t id = privacy::macId(f.mac);
  notes[note_i++ % 128] = Note{id, f.mhz};
  det::report(f.kind, f.mac, f.rssi, f.channel, f.traits, f.badges);
}

uint16_t lookup_mhz(uint32_t id) {
  for (const auto &n : notes)
    if (n.id == id && n.mhz) return n.mhz;
  return 0;
}
}  // namespace cz
