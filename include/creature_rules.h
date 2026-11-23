// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Creature growth rules (pure, host-tested in tools/hosttest).
// Input: the number of DISTINCT devices (salted per-boot ids, Wi-Fi + BLE +
// Remote ID together) seen in the last 3 minutes (det::recentCount()).
//   0 devices -> egg (until the first hatch), 1+ -> size 1 (hatched),
//   30+ -> size 2, 60+ -> size 3, 120+ -> size 4 (max).
// The count is a sliding window, so the creature shrinks again when fewer
// devices are around. Once hatched it never turns back into an egg this boot.
// A device that rotates a random address counts as a new device.
#pragma once
#include <stdint.h>
namespace creature_rules {
constexpr uint32_t kWindowMs = 180000;  // 3 minutes
constexpr uint16_t kHatchAt = 1;
constexpr uint16_t kSizeAt[3] = {30, 60, 120};  // sizes 2..4
constexpr uint8_t kMaxSize = 4;
// PET: forget the 25 oldest devices of the window (all when fewer), so the
// creature shrinks at once; it still never turns back into an egg.
constexpr int kPetForget = 25;
// 0 = egg, 1..4. 'hatched' keeps a hatched creature at size >= 1.
inline uint8_t sizeFor(uint16_t recent, bool hatched) {
  if (recent < kHatchAt) return hatched ? 1 : 0;
  uint8_t s = 1;
  for (uint8_t i = 0; i < 3; i++) if (recent >= kSizeAt[i]) s = (uint8_t)(i + 2);
  return s;
}
inline uint16_t nextAt(uint8_t size) { return size == 0 ? kHatchAt : size >= kMaxSize ? 0 : kSizeAt[size - 1]; }
inline uint16_t floorAt(uint8_t size) { return size <= 1 ? 0 : kSizeAt[size - 2]; }
}
