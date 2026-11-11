// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// LVGL user interface (sonar theme). Hardware independent: it only reads the
// detection table, creature, firmware status and OTA portal state, so the same
// code also runs in the host screenshot renderer (tools/sim).
#pragma once
#include <stdint.h>
namespace ui {
enum Screen : uint8_t { BOOT, SONAR, GRID, CREATURE, MENU, DETAILS, TRACKERS, DRONES, WARDRIVING, UPDATE_CONFIRM, OTA, ABOUT, TORPEDO, UNSIGNED_CONFIRM, COUNTERS, EVOLUTION };
void begin();                 // builds the boot screen
void show(Screen s);
void refresh();               // call ~4x per second
bool takeUpdateRequest();     // user confirmed "Start update mode"
int takeUnsignedDecision();   // -1 none, 0 cancelled, 1 confirmed
void setUnsignedBanner(bool on);
void setBootProgress(int pct, const char *msg);
}
