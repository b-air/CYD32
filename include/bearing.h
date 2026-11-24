// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Indicative bearing of a device, shared by the CYD and the StopWatch. There
// is NO direction finding: one omnidirectional antenna cannot tell where a
// signal comes from. The angle is a deterministic hash of the radio address,
// so the same address gets the same angle on every CYD32 device and across
// reboots, and the UI labels it "indicative".
//
// Hash: 32-bit FNV-1a (offset basis 0x811C9DC5, prime 0x01000193) over the 6
// address bytes in display order (aa:bb:cc:dd:ee:ff -> 0xaa first). BLE
// addresses arrive little-endian from NimBLE and are reversed by the scanner
// before they get here.
//   bearing = (hash * 360) >> 32          (0..359, uniform)
// Unlike privacy::macId() this hash is NOT salted (it must match across
// devices). It keeps about 8.5 bits: enough to spread devices around the
// scope, far too little to recover an address. The address itself is never
// stored.
// RULE: the bearing is display-only. It must never be written to any SD
// export, serial output, log or file (scripts/guard.sh checks output calls).
// (The former GRID cell, a second unsalted value, was removed in batch 6.)
// Randomized addresses (BLE resolvable private addresses, Wi-Fi probe MAC
// randomization) rotate every few minutes: the device then reappears at a new
// angle. isRandomized() flags locally administered addresses so the UI can say so.
#pragma once
#include <stdint.h>
namespace bearing {
uint32_t fnv1a(const uint8_t *p, int n);
uint16_t fromMac(const uint8_t mac[6]);   // 0..359
inline bool isRandomized(const uint8_t mac[6]) { return (mac[0] & 0x02) != 0; }  // locally administered bit
}
