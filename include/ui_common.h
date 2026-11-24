// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Helpers shared by the CYD (ui.cpp) and StopWatch (ui_round.cpp) UIs: the
// 6-entry menu, sonar contacts from detections, and text formatting.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "detections.h"
#include "ui.h"
#include "ui_radar.h"
namespace uic {
// The menu tree, identical on both targets (MENU tab).
constexpr int kMenuN = 6;  // SONAR, COUNTERS, WARDRIVE, CREATURE, BEEP, OTA
extern const ui::Screen kMenuScr[kMenuN];
extern const char *const kMenuTxt[kMenuN];
// Sonar contact for a detection: radius from the smoothed RSSI, angle = the
// indicative bearing (bearing.h), trail = previous smoothed positions, CLOSE
// flag at >= rssi::kCloseDbm, STALE after 15 s without a packet.
void toContact(const radar::Style &s, const det::Entry &e, uint32_t now, radar::Contact &c);
// Short label from the salted per-boot id (privacy::macId): its first 2 or 4
// hex digits, e.g. "3F" / "3F2A". Not derived from the address or the bearing.
void tag2(uint32_t id, char out[3]);
void tag4(uint32_t id, char out[5]);
// Dashboard lists (one at a time), shared by both targets.
enum View : uint8_t { V_WIFI, V_BLE, V_RID, V_TAGGED, V_LOG, V_COUNT };
extern const char *const kViewTxt[V_COUNT];
bool inView(const det::Entry &e, View v);  // V_LOG: false (log has its own source)
// Creature PET: forget the 25 oldest devices of the 3-minute window and
// recompute the stage at once (never back to the egg). Returns how many.
int petForget();
const char *kindName(det::Kind k);              // "WIFI" / "BLE" / "DRONE"
void badgeStr(uint8_t badges, char *out, size_t n, bool withRandom = false);  // "TRK META FLOCK" or ""
void mmss(uint32_t ms, char out[8]);            // time since boot "mm:ss" (no RTC)
// Next entry clockwise (by indicative bearing) after the one with id 'cur';
// the first one when cur is not in the list. -1 for an empty list.
int nextClockwise(const det::Entry *e, int n, uint32_t cur, bool haveCur);
}
