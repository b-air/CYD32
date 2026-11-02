// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// MAC pseudonymisation: SHA-256(per-boot random salt || MAC), truncated.
// The salt lives only in RAM and is regenerated at every boot, so IDs cannot be
// linked across reboots. Raw MAC addresses are never stored or displayed.
#pragma once
#include <stdint.h>
namespace privacy {
void begin();                                  // draws a fresh 32-byte salt from the hardware RNG
uint32_t macId(const uint8_t mac[6]);          // 32-bit pseudonymous id
void idToStr(uint32_t id, char out[9]);        // "a1b2c3d4"
}
