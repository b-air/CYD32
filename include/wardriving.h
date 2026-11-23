// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// StopWatch RAM-only wardriving log. Records salted detection IDs already
// produced by privacy::macId (the same scheme as the live table). Nothing is
// written to NVS, flash, serial or any network path. The table is allocated
// once in PSRAM at boot (RAM only) and is gone at reboot. Compiled in only on
// the StopWatch target.
// RULE: the indicative bearing (unsalted FNV-1a, bearing.h) is display-only and must never be written to any SD export, serial output, log or file (scripts/guard.sh checks output calls).
#pragma once
#include <stdint.h>
#include "detections.h"
namespace wardriving {
// Fixed cap: unique salted IDs kept this boot. When full, the oldest
// first-seen record is dropped to make room.
constexpr int kCap = 1024;
struct Rec {
  uint32_t id;       // salted id (never a raw MAC / BSSID)
  uint32_t firstMs;
  uint32_t lastMs;
  int8_t rssi;
  uint8_t channel;
  uint8_t kind;      // det::Kind
  uint16_t bearing;  // indicative bearing (det::Entry::bearing), NOT a direction; display only, never exported
};
void begin();                // allocates the RAM table (PSRAM), empty log
void start();                // open / resume a session (first visit to the screen)
void loop();                 // poll det::snapshot() and upsert; silent
bool active();
uint32_t sessionMs();        // 0 if the session has not started
int count();
int countKind(det::Kind k);
const Rec *records();
}
