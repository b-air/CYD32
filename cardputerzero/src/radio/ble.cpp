// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Passive LE advertising reports only. Scan type is 0 (passive): the helper
// never sends a SCAN_REQ and never opens a connection.
#include "radio.hpp"
#include <string.h>
#include "signatures.h"
namespace cz {
namespace {
constexpr uint8_t kHciCommand = 0x01;
constexpr uint8_t kHciEvent = 0x04;
constexpr uint8_t kLeMeta = 0x3E;
constexpr uint8_t kAdvReport = 0x02;
constexpr uint16_t kOpScanParams = 0x200B;
constexpr uint16_t kOpScanEnable = 0x200C;

HciCmd pack(uint16_t opcode, const uint8_t *data, uint8_t plen) {
  HciCmd c;
  c.bytes[0] = kHciCommand;
  c.bytes[1] = (uint8_t)(opcode & 0xff);
  c.bytes[2] = (uint8_t)(opcode >> 8);
  c.bytes[3] = plen;
  for (uint8_t i = 0; i < plen && 4u + i < sizeof c.bytes; i++) c.bytes[4 + i] = data[i];
  c.len = 4u + plen;
  return c;
}

void parse_ad(const uint8_t *ad, size_t n, AdvReport &r) {
  uint16_t uu[8];
  int nu = 0;
  const uint8_t *mfg = nullptr;
  size_t mfg_len = 0;
  char name[32];
  name[0] = 0;
  size_t i = 0;
  while (i + 1 < n) {
    const uint8_t len = ad[i];
    if (len == 0 || i + 1 + len > n) break;
    const uint8_t typ = ad[i + 1];
    const uint8_t *v = ad + i + 2;
    const size_t vl = len - 1;
    if ((typ == 0x02 || typ == 0x03) && vl >= 2) {
      for (size_t u = 0; u + 1 < vl && nu < 8; u += 2) uu[nu++] = (uint16_t)(v[u] | (v[u + 1] << 8));
    } else if (typ == 0x16 && vl >= 2) {
      const uint16_t uuid = (uint16_t)(v[0] | (v[1] << 8));
      if (nu < 8) uu[nu++] = uuid;
      if (uuid == 0xFFFA) r.remote_id = true;
    } else if (typ == 0xFF && vl >= 2 && !mfg) {
      mfg = v;
      mfg_len = vl;
    } else if ((typ == 0x08 || typ == 0x09) && vl && !name[0]) {
      const size_t c = vl < sizeof name - 1 ? vl : sizeof name - 1;
      memcpy(name, v, c);
      name[c] = 0;
    }
    i += 1u + len;
  }
  if (!r.remote_id) r.badges = sig::ble(mfg, mfg_len, uu, nu, name[0] ? name : nullptr);
  if (r.badges & sig::TRK) r.traits |= det::traitBit(det::TRK);
}
}  // namespace

HciCmd le_set_scan_params_passive() {
  // type 0 = passive, interval = window = 0x0060 (37.5 ms), public address, accept all.
  const uint8_t d[] = {0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 0x00};
  return pack(kOpScanParams, d, sizeof d);
}

HciCmd le_set_scan_enable(bool on) {
  const uint8_t d[] = {(uint8_t)(on ? 0x01 : 0x00), 0x00};
  return pack(kOpScanEnable, d, sizeof d);
}

bool parse_le_adv_report(const uint8_t *evt, size_t n, std::vector<AdvReport> &out) {
  out.clear();
  if (n < 5 || evt[0] != kHciEvent || evt[1] != kLeMeta) return false;
  const uint8_t plen = evt[2];
  if ((size_t)plen + 3 > n || plen < 2 || evt[3] != kAdvReport) return false;
  const uint8_t num = evt[4];
  size_t i = 5;
  const size_t end = 3u + plen;
  for (uint8_t r = 0; r < num; r++) {
    if (i + 10 > end) return false;
    AdvReport a;
    const uint8_t addr_type = evt[i + 1];
    // HCI address is little-endian, same reversal as the NimBLE scanner.
    for (int b = 0; b < 6; b++) a.mac[b] = evt[i + 2 + (5 - b)];
    const uint8_t dlen = evt[i + 8];
    if (i + 9u + dlen + 1u > end) return false;
    const uint8_t *data = evt + i + 9;
    a.rssi = (int8_t)evt[i + 9 + dlen];
    parse_ad(data, dlen, a);
    if (addr_type == 0x01 && (a.mac[0] >> 6) != 3) a.badges |= sig::RANDOM_ADDR;
    out.push_back(a);
    i += 10u + dlen;
  }
  return true;
}

void ingest_adv(const AdvReport &a) {
  det::report(a.remote_id ? det::DRONE : det::BLE, a.mac, a.rssi, 0, a.traits, a.badges);
}
}  // namespace cz
