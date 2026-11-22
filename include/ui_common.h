// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Helpers shared by the CYD (ui.cpp) and StopWatch (ui_round.cpp) UIs: the
// 8-entry menu tree, sonar contacts from detections, and text formatting.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "detections.h"
#include "ui.h"
#include "ui_radar.h"
namespace uic {
// The menu tree, identical on both targets (MENU tab).
constexpr int kMenuN = 8;
extern const ui::Screen kMenuScr[kMenuN];
extern const char *const kMenuTxt[kMenuN];
// Sonar contact for a detection: radius from the smoothed RSSI, angle = the
// indicative bearing (bearing.h), trail = previous smoothed positions, CLOSE
// flag at >= rssi::kCloseDbm, STALE after 15 s without a packet.
void toContact(const radar::Style &s, const det::Entry &e, uint32_t now, radar::Contact &c);
void cellName(uint8_t cell, char out[3]);       // "E4"
const char *kindName(det::Kind k);              // "WIFI" / "BLE" / "DRONE"
void badgeStr(uint8_t badges, char *out, size_t n, bool withRandom = false);  // "TRK META FLOCK" or ""
void mmss(uint32_t ms, char out[8]);            // time since boot "mm:ss" (no RTC)
// Next entry clockwise (by indicative bearing) after the one with id 'cur';
// the first one when cur is not in the list. -1 for an empty list.
int nextClockwise(const det::Entry *e, int n, uint32_t cur, bool haveCur);
}
