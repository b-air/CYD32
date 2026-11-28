// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "Arduino.h"
#include <chrono>
extern "C" uint32_t millis(void) {
  using clock = std::chrono::steady_clock;
  static const auto t0 = clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count();
}
