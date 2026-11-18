/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* LVGL assert hook: print the location on the serial console and abort,
   instead of LVGL's default silent while(1). */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void cyd32_lv_assert_fail(const char *file, int line);
#ifdef __cplusplus
}
#endif
