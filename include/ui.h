// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// LVGL user interface (sonar theme). Hardware independent: it only reads the
// detection table, creature, firmware status and OTA portal state, so the same
// code also runs in the host screenshot renderer (tools/sim).
#pragma once
#include <stdint.h>
namespace ui {
enum Screen : uint8_t { BOOT, SONAR, CREATURE, MENU, DETAILS, WARDRIVING, UPDATE_CONFIRM, OTA, ABOUT, UNSIGNED_CONFIRM, COUNTERS, EVOLUTION, BEEP };
// COUNTERS is the dashboard: counters overview plus one list at a time
// (WIFI, BLE, RID, TAGGED = TRK / META / FLOCK, LOG). DETAILS is the
// single-device view. BEEP is not a screen: the MENU entry toggles the
// proximity beep (beep.h) and stays on the menu.
// Screen shown after boot, where BACK ends up and where a long B press goes
// (StopWatch): the SONAR. The creature hatches on its own at the first detection.
constexpr Screen HOME = SONAR;
void begin();                 // builds the boot screen
void show(Screen s);
void refresh();               // call ~4x per second
bool takeUpdateRequest();     // user confirmed "Start update mode"
int takeUnsignedDecision();   // -1 none, 0 cancelled, 1 confirmed
void setUnsignedBanner(bool on);
void setBootProgress(int pct, const char *msg);
// Contact pinned on the sonar (TRACK): the proximity beep follows it first.
bool pinnedTarget(uint32_t &id);
}
