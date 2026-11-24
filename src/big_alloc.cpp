// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "big_alloc.h"
#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>

extern "C" void *cyd32_big_alloc(size_t size) {
#ifdef CYD32_HW_M5STACK_STOPWATCH
  void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p) {
    memset(p, 0, size);
    return p;
  }
#endif
  return calloc(1, size);
}

extern "C" void *cyd32_lv_pool_alloc(size_t size) {
  // 128 KB LVGL pool: PSRAM (v0.2.0-test5 kept it in internal .bss, which
  // left too little internal RAM for the Wi-Fi driver on the StopWatch).
  void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p;
}
