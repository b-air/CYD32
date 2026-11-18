/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* LVGL 9.2 configuration for CYD32 (only non-default values matter). */
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#if defined(CYD32_HW_M5STACK_STOPWATCH)
#define LV_MEM_SIZE (128U * 1024U)
#else
#define LV_MEM_SIZE (48U * 1024U)
#endif
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 130
#define LV_USE_OS LV_OS_NONE
#define LV_DRAW_SW_SUPPORT_I4 1   /* v9 glitch logo / title tiles are I4 */
#define LV_USE_LOG 1
#if defined(CYD32_HW_M5STACK_STOPWATCH)
/* printf goes to UART0, not the USB CDC console: main.cpp registers a print
   callback to Serial instead. WARN shows "Allocating layer buffer failed". */
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0
#else
#define LV_LOG_LEVEL LV_LOG_LEVEL_ERROR
#define LV_LOG_PRINTF 1
#endif
#define LV_ASSERT_HANDLER_INCLUDE "lv_assert_hook.h"
#define LV_ASSERT_HANDLER cyd32_lv_assert_fail(__FILE__, __LINE__);
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#if defined(CYD32_HW_M5STACK_STOPWATCH)
/* StopWatch: larger text on the 466 px round panel (user request, 18 Nov 2026). */
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#endif
#define LV_FONT_UNSCII_8 1
#define LV_FONT_UNSCII_16 1   /* v9 sonar: bearings, IDs, counts */
#define LV_FONT_DEFAULT &lv_font_montserrat_12
#define LV_USE_QRCODE 1
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 1
#define LV_USE_LOTTIE 0
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#endif
#endif
