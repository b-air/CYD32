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
constexpr int kTrail = 4;
struct Entry {
  uint32_t id;        // pseudonymous id (privacy::macId, salted per boot), never a raw MAC
  uint32_t lastSeen;  // millis()
  uint32_t trailMs;   // time of the newest trail sample
  int16_t rssiQ4;     // smoothed RSSI in 1/16 dB (rssi::emaStep, fast attack)
  uint16_t bearing;   // indicative bearing 0..359 (bearing::fromMac, same on every device)
  int8_t rssi;        // last raw RSSI
  uint8_t channel;    // Wi-Fi channel, 0 for BLE
  Kind kind;
  uint8_t badges;     // sig::Badge bits (TRK / META / FLOCK / RANDOM_ADDR)
  uint8_t cell;       // GRID cell 0..79 (bearing::cellFromMac, same on every device)
  uint8_t trailN;
  int8_t trail[kTrail];  // previous smoothed RSSI samples, newest first
};
int8_t smoothed(const Entry &e);
constexpr int kMax = 96;
void begin();
// Thread-safe (called from Wi-Fi/BLE callbacks). traits: Trait bits,
// badges: sig::Badge bits (the scanner sets RANDOM_ADDR). mac is in display order.
void report(Kind k, const uint8_t mac[6], int8_t rssi, uint8_t channel, uint8_t traits = 0, uint8_t badges = 0);
struct Counts {
  uint16_t wifi, ble, drone;  // currently visible (seen in last 60 s)
  uint16_t trk, meta, flock;  // currently visible devices with that badge
  uint32_t droneEver;         // unique Remote ID addresses in the 24 h window
  uint16_t unique24h;         // unique addresses in the 24 h window
  uint16_t recent3m;          // distinct devices seen in the last 3 minutes (creature)
  uint8_t seenTraits;         // first-seen source-type bits (sticky this boot)
};
Counts counts();
int recentCount();  // distinct devices in the last creature_rules::kWindowMs
// Copy of visible entries, newest first. filtered: apply the display filters.
int snapshot(Entry *out, int max, bool filtered = false);
// Display filters (RAM only, default: everything shown). They hide entries
// from the sonar, grid and lists; scanning and counters are not affected.
enum Filter : uint8_t { F_WIFI = 0x01, F_BLE = 0x02, F_RID = 0x04, F_TAGGED_ONLY = 0x08 };
constexpr uint8_t kFilterDefault = F_WIFI | F_BLE | F_RID;
uint8_t filters();
void setFilters(uint8_t f);
bool passes(const Entry &e, uint8_t f);
// Simple RAM log: first sighting this boot of a drone or of a tagged device
// (TRK / META / FLOCK). Newest first.
struct LogEvt {
  uint32_t ms;
  uint32_t id;
  int8_t rssi;
  Kind kind;
  uint8_t badges;
  uint8_t cell;
};
constexpr int kLogMax = 24;
int logSnapshot(LogEvt *out, int max);
}
