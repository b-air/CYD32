// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Proximity beep (MENU > BEEP). Off at every boot, RAM only, receive-only:
// it only reads the detection table. Target: the strongest smoothed Wi-Fi /
// BLE contact visible in the current sonar mode, or the pinned contact
// (det::beepTarget). Cadence: beep_rules::intervalMs (prox_beep.h).
// Output: StopWatch built-in speaker (M5.Speaker, started only while ON);
// CYD "SPEAKER" connector (GPIO26, LEDC tone, driven only while ON).
#pragma once
#include <stdint.h>
namespace beep {
void setEnabled(bool on);
bool enabled();
void toggle();
// Main loop. hasPinned / pinnedId: the contact pinned on the sonar (TRACK).
void tick(uint32_t now, bool hasPinned, uint32_t pinnedId);
}
