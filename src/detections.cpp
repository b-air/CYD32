// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "detections.h"
#include "bearing.h"
#include "creature_rules.h"
#include "cyd32_creature.h"
#include "privacy.h"
#include "recent_window.h"
#include "rssi_track.h"
#include "signatures.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace {
// Heap, allocated once in begin() (fixed size, never freed): keeps the CYD
// static DRAM segment under its limit.
det::Entry *table = nullptr;
int used = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
constexpr uint32_t kVisibleMs = 60000;
constexpr uint32_t kDedupMs = (uint32_t)CYD32_DEDUP_WINDOW_S * 1000u;
constexpr int kUniqueMax = 384;
struct __attribute__((packed)) UEntry {
  uint32_t id;
  uint32_t born;
  uint8_t flags;
};
constexpr uint8_t kFlagDrone = 1, kFlagLogged = 2;
UEntry *uniq = nullptr;
int nUniq = 0;
uint8_t seenTraits = 0;
// Creature window: distinct salted ids seen in the last 3 minutes. 192 x 8 B,
// heap (allocated once in begin()). 192 > 120 (the size-4 threshold), so
// saturation never hides growth.
RecentWindow<192> *recent = nullptr;
det::LogEvt logBuf[det::kLogMax];
int logHead = 0, logN = 0;
uint8_t filterMask = det::kFilterDefault;

void compactLocked(uint32_t now) {
  if (!uniq) { nUniq = 0; return; }
  int w = 0;
  for (int i = 0; i < nUniq; i++) {
    if ((uint32_t)(now - uniq[i].born) < kDedupMs) {
      if (w != i) uniq[w] = uniq[i];
      w++;
    }
  }
  nUniq = w;
}
int findUniqueLocked(uint32_t id) {
  for (int i = 0; i < nUniq; i++) if (uniq[i].id == id) return i;
  return -1;
}
// Returns the slot of id in the 24 h table (-1 without a table).
int rememberLocked(uint32_t id, uint32_t now, bool drone) {
  if (!uniq) return -1;
  compactLocked(now);
  const int existing = findUniqueLocked(id);
  if (existing >= 0) {
    if (drone) uniq[existing].flags |= kFlagDrone;
    uniq[existing].born = now;  // sliding 24 h window while the device stays visible
    return existing;
  }
  int slot;
  if (nUniq < kUniqueMax) {
    slot = nUniq++;
  } else {
    slot = 0;
    for (int i = 1; i < nUniq; i++) if (uniq[i].born < uniq[slot].born) slot = i;
  }
  uniq[slot].id = id;
  uniq[slot].born = now;
  uniq[slot].flags = drone ? kFlagDrone : 0;
  return slot;
}
uint16_t droneCountLocked() {
  uint16_t n = 0;
  for (int i = 0; i < nUniq; i++) if (uniq[i].flags & kFlagDrone) n++;
  return n;
}
void logLocked(const det::Entry &e, uint32_t now) {
  det::LogEvt &l = logBuf[logHead];
  l.ms = now; l.id = e.id; l.rssi = e.rssi; l.kind = e.kind; l.badges = e.badges; l.cell = e.cell;
  logHead = (logHead + 1) % det::kLogMax;
  if (logN < det::kLogMax) logN++;
}
}  // namespace
namespace det {
int8_t smoothed(const Entry &e) { return rssi::fromQ4(e.rssiQ4); }
void begin() {
  used = 0; nUniq = 0; seenTraits = 0; logHead = logN = 0;
  filterMask = kFilterDefault;
  if (!uniq) uniq = (UEntry *)malloc(sizeof(UEntry) * kUniqueMax);
  if (!table) table = (Entry *)calloc(kMax, sizeof(Entry));
  if (!recent) recent = new (std::nothrow) RecentWindow<192>();
  if (recent) recent->clear();
}
void report(Kind k, const uint8_t mac[6], int8_t dbm, uint8_t ch, uint8_t traits, uint8_t badges) {
  // Hashing outside the critical section. The address itself is not kept.
  const uint32_t id = privacy::macId(mac);
  const uint16_t brg = bearing::fromMac(mac);
  const uint8_t cell = bearing::cellFromMac(mac);
  const uint32_t now = millis();
  if (!table || !recent) return;  // begin() not called or out of memory at boot
  portENTER_CRITICAL(&mux);
  seenTraits |= traits;
  int slot = -1, oldest = 0;
  for (int i = 0; i < used; i++) {
    if (table[i].id == id && table[i].kind == k) { slot = i; break; }
    if (table[i].lastSeen < table[oldest].lastSeen) oldest = i;
  }
  Entry &e = table[slot < 0 ? (used < kMax ? used++ : oldest) : slot];
  const bool fresh = slot < 0 || (uint32_t)(now - e.lastSeen) > rssi::kResetMs;
  if (slot < 0) {
    memset(&e, 0, sizeof e);
    e.id = id; e.kind = k;
  }
  e.bearing = brg; e.cell = cell;
  e.badges |= badges;
  e.rssiQ4 = rssi::emaStep(e.rssiQ4, fresh, dbm);
  if (fresh) { e.trailN = 0; e.trailMs = now; }
  else if ((uint32_t)(now - e.trailMs) >= rssi::kTrailMs) {
    // Trail: the previous smoothed positions, sampled every kTrailMs.
    for (int i = kTrail - 1; i > 0; i--) e.trail[i] = e.trail[i - 1];
    e.trail[0] = rssi::fromQ4(e.rssiQ4);
    if (e.trailN < kTrail) e.trailN++;
    e.trailMs = now;
  }
  e.rssi = dbm; e.channel = ch; e.lastSeen = now;
  const int u = rememberLocked(id, now, k == DRONE);
  recent->add(id, now);
  const uint8_t tags = e.badges & (sig::TRK | sig::META | sig::FLOCK);
  if (u >= 0 && (k == DRONE || tags) && !(uniq[u].flags & kFlagLogged)) {
    uniq[u].flags |= kFlagLogged;
    logLocked(e, now);
  }
  portEXIT_CRITICAL(&mux);
}
Counts counts() {
  Counts c;
  memset(&c, 0, sizeof c);
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < used; i++) {
    const Entry &e = table[i];
    if (now - e.lastSeen > kVisibleMs) continue;
    if (e.kind == WIFI) c.wifi++; else if (e.kind == BLE) c.ble++; else c.drone++;
    if (e.badges & sig::TRK) c.trk++;
    if (e.badges & sig::META) c.meta++;
    if (e.badges & sig::FLOCK) c.flock++;
  }
  compactLocked(now);
  c.unique24h = (uint16_t)nUniq;
  c.droneEver = droneCountLocked();
  c.recent3m = recent ? (uint16_t)recent->count(now, creature_rules::kWindowMs) : 0;
  c.seenTraits = seenTraits;
  portEXIT_CRITICAL(&mux);
  return c;
}
int recentCount() {
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  const int n = recent ? recent->count(now, creature_rules::kWindowMs) : 0;
  portEXIT_CRITICAL(&mux);
  return n;
}
bool passes(const Entry &e, uint8_t f) {
  const uint8_t kindBit = e.kind == WIFI ? F_WIFI : e.kind == BLE ? F_BLE : F_RID;
  if (!(f & kindBit)) return false;
  if ((f & F_TAGGED_ONLY) && e.kind != DRONE && !(e.badges & (sig::TRK | sig::META | sig::FLOCK))) return false;
  return true;
}
uint8_t filters() { return filterMask; }
void setFilters(uint8_t f) { filterMask = f; }
int snapshot(Entry *out, int max, bool filtered) {
  const uint32_t now = millis();
  const uint8_t f = filterMask;
  int n = 0;
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < used && n < max; i++)
    if (now - table[i].lastSeen <= kVisibleMs && (!filtered || passes(table[i], f))) out[n++] = table[i];
  portEXIT_CRITICAL(&mux);
  // newest first (insertion sort, n <= kMax)
  for (int i = 1; i < n; i++) { Entry e = out[i]; int j = i - 1;
    while (j >= 0 && out[j].lastSeen < e.lastSeen) { out[j + 1] = out[j]; j--; } out[j + 1] = e; }
  return n;
}
int logSnapshot(LogEvt *out, int max) {
  int n = 0;
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < logN && n < max; i++) out[n++] = logBuf[(logHead - 1 - i + kLogMax) % kLogMax];
  portEXIT_CRITICAL(&mux);
  return n;
}
}
