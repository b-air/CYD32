// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "ui_common.h"
#include <stdio.h>
#include <string.h>
#include "rssi_track.h"
#include "signatures.h"
namespace uic {
const ui::Screen kMenuScr[kMenuN] = {ui::SONAR, ui::GRID, ui::TRACKERS, ui::DRONES,
                                     ui::WARDRIVING, ui::CREATURE, ui::COUNTERS, ui::UPDATE_CONFIRM};
const char *const kMenuTxt[kMenuN] = {"SONAR", "GRID", "TRACKERS", "DRONES", "WARDRIVE", "CREATURE", "COUNTERS", "OTA"};
void toContact(const radar::Style &s, const det::Entry &e, uint32_t now, radar::Contact &c) {
  memset(&c, 0, sizeof c);
  const int sm = det::smoothed(e);
  radar::placeRssi(s, sm, e.bearing, c.dx, c.dy);
  c.kind = e.kind == det::DRONE ? radar::DRONE : e.kind == det::BLE ? radar::BLE : radar::WIFI;
  c.seed = e.id;
  if (sm >= rssi::kCloseDbm) c.flags |= radar::CLOSE;
  if ((uint32_t)(now - e.lastSeen) > 15000) c.flags |= radar::STALE;
  const int rNow = radar::rssiRadius(s, sm);
  for (int t = 0; t < e.trailN && c.tn < radar::kTrailPts; t++) {
    const int r = radar::rssiRadius(s, e.trail[t]);
    if (r - rNow < 3 && rNow - r < 3) continue;  // hidden under the echo anyway
    radar::polar(r, e.bearing, c.tdx[c.tn], c.tdy[c.tn]);
    c.tn++;
  }
}
void cellName(uint8_t cell, char out[3]) {
  out[0] = (char)('A' + cell % 10);
  out[1] = (char)('1' + (cell / 10) % 8);
  out[2] = 0;
}
const char *kindName(det::Kind k) { return k == det::DRONE ? "DRONE" : k == det::BLE ? "BLE" : "WIFI"; }
void badgeStr(uint8_t b, char *out, size_t n, bool withRandom) {
  if (!n) return;
  out[0] = 0;
  const struct { uint8_t bit; const char *t; } tags[4] = {{sig::TRK, "TRK"}, {sig::META, "META"}, {sig::FLOCK, "FLOCK"}, {sig::RANDOM_ADDR, "RND"}};
  for (const auto &t : tags) {
    if (!(b & t.bit) || (t.bit == sig::RANDOM_ADDR && !withRandom)) continue;
    if (out[0]) strncat(out, " ", n - strlen(out) - 1);
    strncat(out, t.t, n - strlen(out) - 1);
  }
}
void mmss(uint32_t ms, char out[8]) {
  const uint32_t s = ms / 1000;
  snprintf(out, 8, "%02lu:%02lu", (unsigned long)((s / 60) % 100), (unsigned long)(s % 60));
}
int nextClockwise(const det::Entry *e, int n, uint32_t cur, bool haveCur) {
  if (n <= 0) return -1;
  int curBrg = -1;
  if (haveCur) for (int i = 0; i < n; i++) if (e[i].id == cur) { curBrg = e[i].bearing * 1000 + (int)(e[i].id % 1000); break; }
  int best = -1, bestKey = 0x7fffffff, first = -1, firstKey = 0x7fffffff;
  for (int i = 0; i < n; i++) {
    const int key = e[i].bearing * 1000 + (int)(e[i].id % 1000);  // ties broken by id
    if (key < firstKey) { firstKey = key; first = i; }
    if (curBrg >= 0 && key > curBrg && key < bestKey) { bestKey = key; best = i; }
  }
  return best >= 0 ? best : first;
}
}
