// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Board support: display, backlight, touch and the unsigned-mode hold button.
// CYD (ESP32-2432S028): LovyanGFX ILI9341 + XPT2046, hold = GPIO0 (BOOT).
// StopWatch (ESP32-S3R8): M5Unified/M5GFX CO5300 + CST820B, hold = KEYA (GPIO2).
#pragma once
#include <stdint.h>
namespace hw {
void begin();                       // display, backlight, touch
bool touch(int16_t &x, int16_t &y); // true while pressed (screen coords)
void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *rgb565);
bool bootButtonPressed();           // unsigned-mode hold button, active low
[[noreturn]] void fatal(const char *msg);  // message on screen + serial, then halt (no reset loop)
// Proximity beep output (beep.h). beepPower(true) prepares the output (StopWatch:
// M5.Speaker.begin(); CYD: LEDC on GPIO26), beepPower(false) releases it, so
// nothing is driven and no I2S buffer is held while the beep is OFF.
void beepPower(bool on);
void beepTone(uint16_t hz, uint16_t ms);  // short tone, no-op while off
#ifdef CYD32_HW_M5STACK_STOPWATCH
bool btnAClicked();                 // KEYA short press (NEXT)
bool btnBClicked();                 // KEYB short press (SELECT)
bool btnAHeld();                    // KEYA long press, >= 500 ms (BACK)
bool btnBHeld();                    // KEYB long press, >= 500 ms (SONAR home)
void clearButtons();                // drop latched presses (boot splash)
bool btnAPressed();
bool btnBPressed();
int batteryPct();                   // 0..100, hardware-dependent
void touchHit(const char *what);    // serial "touch: hit ..." line (UI hit target)
#endif
}
