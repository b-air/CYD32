// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "lv_assert_hook.h"
#include <Arduino.h>
#include "boot_log.h"
#include <stdlib.h>

extern "C" void cyd32_lv_assert_fail(const char *file, int line) {
  Serial.printf("[lvgl] ASSERT FAILED at %s:%d, rebooting\n", file, line);
  CYD32_BOOT_FLUSH();  // no flush on USB CDC (StopWatch): it could block with no host
  delay(200);
  abort();  // panic handler prints a backtrace, then the chip resets
}
