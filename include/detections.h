// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// In-RAM table of recent detections. Nothing is persisted (no flash, no SD).
#pragma once
#include <stdint.h>
namespace det {
enum Kind : uint8_t { WIFI = 0, BLE = 1, DRONE = 2 };
struct Entry {
  uint32_t id;        // pseudonymous id (privacy::macId), never a raw MAC
  int8_t rssi;
  uint8_t channel;    // Wi-Fi channel, 0 for BLE
  Kind kind;
  uint32_t lastSeen;  // millis()
};
constexpr int kMax = 96;
void begin();
// Thread-safe (called from Wi-Fi/BLE callbacks).
void report(Kind k, const uint8_t mac[6], int8_t rssi, uint8_t channel);
struct Counts { uint16_t wifi, ble, drone; uint32_t droneEver; };
Counts counts();                        // currently visible (seen in last 60 s)
int snapshot(Entry *out, int max);      // copy of visible entries, newest first
}
