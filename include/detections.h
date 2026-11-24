// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// In-RAM table of recent detections. Nothing is persisted (no flash, no SD).
#pragma once
#include <stdint.h>
#include "signatures.h"
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
  uint16_t bearing;   // indicative bearing 0..359 (bearing::fromMac): display only, never output
  int8_t rssi;        // last raw RSSI
  uint8_t channel;    // Wi-Fi channel, 0 for BLE
  Kind kind;
  uint8_t badges;     // sig::Badge bits (TRK / META / FLOCK / RANDOM_ADDR)
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
// Creature PET: forget the n oldest sightings of the 3-minute window (all of
// them when fewer than n). Returns how many were forgotten. RAM only.
int forgetOldest(int n);

// Single display category (one at a time), shared by the sonar modes and the
// dashboard lists. RAM only, default ALL. Display / beep only: scanning and
// counters never stop.
enum Cat : uint8_t { CAT_ALL = 0, CAT_WIFI, CAT_BLE, CAT_RID, CAT_FLOCK, CAT_TRK, CAT_META, CAT_COUNT };
constexpr uint8_t kTagBits = sig::TRK | sig::META | sig::FLOCK;
// TAGGED (dashboard): a Wi-Fi / BLE device with a TRK, META or FLOCK badge.
inline bool isTagged(const Entry &e) { return e.kind != DRONE && (e.badges & kTagBits) != 0; }
inline bool inCat(const Entry &e, Cat c) {
  switch (c) {
    case CAT_ALL: return true;
    case CAT_WIFI: return e.kind == WIFI;
    case CAT_BLE: return e.kind == BLE;
    case CAT_RID: return e.kind == DRONE;
    case CAT_FLOCK: return (e.badges & sig::FLOCK) != 0;
    case CAT_TRK: return (e.badges & sig::TRK) != 0;
    case CAT_META: return (e.badges & sig::META) != 0;
    default: return false;
  }
}
inline Cat nextCat(Cat c, int step) { return (Cat)(((int)c + step % (int)CAT_COUNT + (int)CAT_COUNT) % (int)CAT_COUNT); }
const char *catName(Cat c);  // "ALL" / "WIFI" / "BLE" / "RID" / "FLOCK" / "TRK" / "META"
Cat sonarCat();              // current sonar mode (RAM only)
void setSonarCat(Cat c);
// Copy of visible entries in category c, newest first.
int snapshot(Entry *out, int max, Cat c = CAT_ALL);
// Strongest smoothed RSSI among the visible Wi-Fi / BLE entries in category c;
// a visible pinned id (any kind) in c wins. false when nothing matches.
bool beepTarget(Cat c, bool hasPinned, uint32_t pinnedId, int8_t &dbm);
// Simple RAM log: first sighting this boot of a drone or of a tagged device
// (TRK / META / FLOCK). Newest first.
struct LogEvt {
  uint32_t ms;
  uint32_t id;
  int8_t rssi;
  Kind kind;
  uint8_t badges;
};
constexpr int kLogMax = 24;
int logSnapshot(LogEvt *out, int max);
}
