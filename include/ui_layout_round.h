// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Provisional round HUD for the M5Stack StopWatch 466x466 AMOLED (CO5300).
// Chrome is inset so it stays inside the visible circle (corners of the
// 466x466 framebuffer are off-panel). Swap this header when a final mockup
// arrives; ui.cpp reads only these metrics.
#pragma once
#define CYD32_SCREEN_W      466
#define CYD32_SCREEN_H      466
#define CYD32_HEADER_X      109
#define CYD32_HEADER_Y      48
#define CYD32_HEADER_W      248
#define CYD32_HEADER_H      22
#define CYD32_CONTENT_X     78
#define CYD32_CONTENT_Y     74
#define CYD32_CONTENT_W     310
#define CYD32_CONTENT_H     282
#define CYD32_TAB_X         89
#define CYD32_TAB_Y         356
#define CYD32_TAB_W         288
#define CYD32_TAB_H         44
#define CYD32_TAB_BTN_W     72
#define CYD32_CREATURE_ZOOM 512
#define CYD32_HOLD_HINT     "Hold KEYA 3 s: unsigned update"
#define CYD32_UNSIGNED_BODY "KEYA held.\n\nArm UNSIGNED update mode?\nThe next update may install firmware\nwhose origin cannot be verified.\nValid for ONE update, this boot only."
