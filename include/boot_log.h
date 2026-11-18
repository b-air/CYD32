// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// One line per boot step on the serial console (115200 baud on CYD, USB CDC
// on StopWatch), so a hang on real hardware shows the last step reached.
#pragma once
#include <Arduino.h>

#ifdef CYD32_HW_M5STACK_STOPWATCH
// USB CDC: never block on flush when no host is listening.
#define CYD32_BOOT_FLUSH() do {} while (0)
#else
#define CYD32_BOOT_FLUSH() Serial.flush()
#endif

#define BOOT_LOG(fmt, ...)                                                  \
  do {                                                                      \
    Serial.printf("[boot %6lu] " fmt "\n", (unsigned long)millis(), ##__VA_ARGS__); \
    CYD32_BOOT_FLUSH();                                                     \
  } while (0)

#define BOOT_STEP(name, expr)          \
  do {                                 \
    BOOT_LOG("%s ...", name);          \
    expr;                              \
    BOOT_LOG("%s ok", name);           \
  } while (0)
