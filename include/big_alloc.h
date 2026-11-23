/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Large RAM-only buffers. StopWatch (ESP32-S3R8, 8 MB PSRAM): PSRAM first, so
   internal RAM stays free for the Wi-Fi / BLE drivers; internal heap as the
   fallback. CYD (no PSRAM): internal heap. Zero-filled, never persisted. */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *cyd32_big_alloc(size_t size);
/* LVGL work pool (lv_conf.h LV_MEM_POOL_ALLOC, StopWatch only). */
void *cyd32_lv_pool_alloc(size_t size);
#ifdef __cplusplus
}
#endif
