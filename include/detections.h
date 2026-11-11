// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// In-RAM table of recent detections. Nothing is persisted (no flash, no SD).
#pragma once
#include <stdint.h>
namespace det {
enum Kind : uint8_t { WIFI = 0, BLE = 1, DRONE = 2 };
// First-seen source types (sticky this boot). Order matches cyd32_trait_t[8].
enum Trait : uint8_t {
  TRK = 0,    // BLE item tracker
  AP = 1,     // Wi-Fi access point (beacon / probe response)
  HOT = 2,    // mobile / public hotspot heuristic
  OPEN = 3,   // open network (no privacy bit, no RSN/WPA)
  G24 = 4,    // 2.4 GHz
  G5 = 5,     // 5 GHz
  WIFI6 = 6,  // HE capabilities (802.11ax)
  WIFI7 = 7   // EHT / multi-link (802.11be)
};
constexpr uint8_t traitBit(Trait t) { return (uint8_t)(1u << (uint8_t)t); }
struct Entry {
  uint32_t id;        // pseudonymous id (privacy::macId), never a raw MAC
  int8_t rssi;
  uint8_t channel;    // Wi-Fi channel, 0 for BLE
  Kind kind;
  uint32_t lastSeen;  // millis()
};
constexpr int kMax = 96;
void begin();
// Thread-safe (called from Wi-Fi/BLE callbacks). traits is a bitmask of Trait.
void report(Kind k, const uint8_t mac[6], int8_t rssi, uint8_t channel, uint8_t traits = 0);
struct Counts {
  uint16_t wifi, ble, drone;  // currently visible (seen in last 60 s)
  uint32_t droneEver;         // unique Remote ID addresses in the 24 h window
  uint16_t unique24h;         // unique addresses in the 24 h window
  uint8_t seenTraits;         // first-seen source-type bits (sticky this boot)
};
Counts counts();                        // currently visible (seen in last 60 s)
int snapshot(Entry *out, int max);      // copy of visible entries, newest first
}
