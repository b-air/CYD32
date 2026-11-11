// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "detections.h"
#include "cyd32_creature.h"
#include "privacy.h"
#include <Arduino.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace {
det::Entry table[det::kMax];
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
constexpr uint8_t kFlagDrone = 1;
UEntry *uniq = nullptr;
int nUniq = 0;
uint8_t seenTraits = 0;

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
void rememberLocked(uint32_t id, uint32_t now, bool drone) {
  if (!uniq) return;
  compactLocked(now);
  const int existing = findUniqueLocked(id);
  if (existing >= 0) {
    if (drone) uniq[existing].flags |= kFlagDrone;
    return;
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
}
uint16_t uniqueCountLocked() { return (uint16_t)nUniq; }
uint16_t droneCountLocked() {
  uint16_t n = 0;
  for (int i = 0; i < nUniq; i++) if (uniq[i].flags & kFlagDrone) n++;
  return n;
}
}
namespace det {
void begin() {
  used = 0; nUniq = 0; seenTraits = 0;
  if (!uniq) uniq = (UEntry *)malloc(sizeof(UEntry) * kUniqueMax);
}
void report(Kind k, const uint8_t mac[6], int8_t rssi, uint8_t ch, uint8_t traits) {
  const uint32_t id = privacy::macId(mac);  // hashing outside the critical section
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  seenTraits |= traits;
  int slot = -1, oldest = 0;
  for (int i = 0; i < used; i++) {
    if (table[i].id == id && table[i].kind == k) { slot = i; break; }
    if (table[i].lastSeen < table[oldest].lastSeen) oldest = i;
  }
  if (slot < 0) {
    slot = used < kMax ? used++ : oldest;
    table[slot].id = id; table[slot].kind = k;
    rememberLocked(id, now, k == DRONE);
  }
  table[slot].rssi = rssi; table[slot].channel = ch; table[slot].lastSeen = now;
  portEXIT_CRITICAL(&mux);
}
Counts counts() {
  Counts c{0, 0, 0, 0, 0, 0};
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < used; i++) {
    if (now - table[i].lastSeen > kVisibleMs) continue;
    if (table[i].kind == WIFI) c.wifi++; else if (table[i].kind == BLE) c.ble++; else c.drone++;
  }
  compactLocked(now);
  c.unique24h = uniqueCountLocked();
  c.droneEver = droneCountLocked();
  c.seenTraits = seenTraits;
  portEXIT_CRITICAL(&mux);
  return c;
}
int snapshot(Entry *out, int max) {
  const uint32_t now = millis();
  int n = 0;
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < used && n < max; i++)
    if (now - table[i].lastSeen <= kVisibleMs) out[n++] = table[i];
  portEXIT_CRITICAL(&mux);
  // newest first (insertion sort, n <= kMax)
  for (int i = 1; i < n; i++) { Entry e = out[i]; int j = i - 1;
    while (j >= 0 && out[j].lastSeen < e.lastSeen) { out[j + 1] = out[j]; j--; } out[j + 1] = e; }
  return n;
}
}
