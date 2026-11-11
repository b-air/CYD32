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
}
