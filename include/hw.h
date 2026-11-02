// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Board support: Cheap Yellow Display ESP32-2432S028 (ILI9341 + XPT2046).
#pragma once
#include <stdint.h>
namespace hw {
void begin();                       // display, backlight, touch
bool touch(int16_t &x, int16_t &y); // true while pressed (screen coords)
void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *rgb565);
bool bootButtonPressed();           // GPIO0, active low
}
