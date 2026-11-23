// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Proximity beep cadence (pure, host-tested in tools/hosttest). Input: the
// smoothed RSSI of the beep target (det::beepTarget). Silent below CLOSE
// (-50 dBm); from -50 to VERY CLOSE (-42 dBm) the interval falls linearly
// from 1000 ms to 150 ms, and stays at 150 ms above -42 dBm. RSSI is only a
// rough proxy for distance (walls, bodies, antennas, transmit power).
#pragma once
#include <stdint.h>
#include "rssi_track.h"
namespace beep_rules {
constexpr uint32_t kSlowMs = 1000, kFastMs = 150;
constexpr uint16_t kToneHz = 2500, kToneMs = 30;
// 0 = silent.
inline uint32_t intervalMs(int dbm) {
  if (dbm < rssi::kCloseDbm) return 0;
  if (dbm >= rssi::kVeryCloseDbm) return kFastMs;
  const int span = rssi::kVeryCloseDbm - rssi::kCloseDbm;  // 8 dB
  return kSlowMs - (uint32_t)((dbm - rssi::kCloseDbm) * (int)(kSlowMs - kFastMs) / span);
}
}
