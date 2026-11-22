// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// RSSI smoothing and RSSI-to-radius mapping for the sonar (both targets).
// Pure functions, host-tested (tools/hosttest).
//
// Smoothing: exponential moving average in 1/16 dB with a fast attack. A
// stronger sample (the device got closer, or we walked towards it) moves the
// average 10/16 of the way at once; a weaker one 5/16, which still follows a
// walk-away within ~3 samples but rejects single fades. After kResetMs of
// silence the average restarts from the new sample.
//
// Radius: linear in dBm (log-distance), so every range ring is a fixed RSSI:
//   r = rim * (-35 - dBm) / 60, clamped to [rMin, rMax]
//   ring 1 = -50 dBm (CLOSE), ring 2 = -65, ring 3 = -80, rim = -95 dBm.
// An extra dotted ring at -42 dBm marks VERY CLOSE. RSSI is only a rough
// proxy for distance (walls, bodies, antennas, transmit power).
#pragma once
#include <stdint.h>
namespace rssi {
constexpr int kCloseDbm = -50;      // ring 1: echoes inside get a highlight ring
constexpr int kVeryCloseDbm = -42;  // extra inner dotted ring
constexpr uint32_t kResetMs = 5000;
constexpr uint32_t kTrailMs = 700;  // trail sample period
constexpr int kTrailLen = 4;
inline int16_t emaStep(int16_t q4, bool fresh, int8_t dbm) {
  const int s = dbm * 16;
  if (fresh) return (int16_t)s;
  const int d = s - q4;
  return (int16_t)(q4 + (d > 0 ? (d * 10) / 16 : (d * 5) / 16));
}
inline int8_t fromQ4(int16_t q4) { return (int8_t)(q4 >= 0 ? (q4 + 8) / 16 : -((-q4 + 8) / 16)); }
inline int radius(int dbm, int rim, int rMin, int rMax) {
  int r = rim * (-35 - dbm) / 60;
  if (r < rMin) r = rMin;
  if (r > rMax) r = rMax;
  return r;
}
inline const char *zone(int dbm) { return dbm >= kVeryCloseDbm ? "VERY CLOSE" : dbm >= kCloseDbm ? "CLOSE" : dbm >= -65 ? "NEAR" : dbm >= -80 ? "MID" : "FAR"; }
}
