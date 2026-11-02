// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "detections.h"
#include "privacy.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace {
det::Entry table[det::kMax];
int used = 0;
uint32_t droneEver = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
constexpr uint32_t kVisibleMs = 60000;
}
namespace det {
void begin() { used = 0; droneEver = 0; }
void report(Kind k, const uint8_t mac[6], int8_t rssi, uint8_t ch) {
  const uint32_t id = privacy::macId(mac);  // hashing outside the critical section
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  int slot = -1, oldest = 0;
  for (int i = 0; i < used; i++) {
    if (table[i].id == id && table[i].kind == k) { slot = i; break; }
    if (table[i].lastSeen < table[oldest].lastSeen) oldest = i;
  }
  if (slot < 0) {
    if (k == DRONE) droneEver++;
    slot = used < kMax ? used++ : oldest;
    table[slot].id = id; table[slot].kind = k;
  }
  table[slot].rssi = rssi; table[slot].channel = ch; table[slot].lastSeen = now;
  portEXIT_CRITICAL(&mux);
}
Counts counts() {
  Counts c{0, 0, 0, 0};
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < used; i++) {
    if (now - table[i].lastSeen > kVisibleMs) continue;
    if (table[i].kind == WIFI) c.wifi++; else if (table[i].kind == BLE) c.ble++; else c.drone++;
  }
  c.droneEver = droneEver;
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
