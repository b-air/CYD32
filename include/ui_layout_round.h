// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// M5Stack StopWatch 466x466 AMOLED (CO5300) round HUD metrics (v8 design).
// The whole 466 px circle is active: rings, tabs and labels reach r = 227
// (r = 233 is the glass edge), see SW_SAFE_R in cyd32_sonar_theme.h.
#pragma once
#define CYD32_SCREEN_W      466
#define CYD32_SCREEN_H      466
#define CYD32_CX            233
#define CYD32_CY            233
#define CYD32_R_CLIP        233
#define CYD32_R_SAFE        229
#define CYD32_R_BEZEL       4
#define CYD32_TOUCH_MIN     48
#define CYD32_CREATURE_NN   3
#define CYD32_CREATURE_ZOOM 768
/* Creature stages 1..4 (egg = stage 1): LVGL zoom of the 192 px (3x nearest
   neighbour) buffer, 192 / 224 / 256 / 288 px. */
static const uint16_t CYD32_STAGE_ZOOM[4] = {256, 299, 341, 384};
#define CYD32_HOLD_HINT     "Hold KEYA 3 s: unsigned update"
#define CYD32_UNSIGNED_BODY "KEYA held.\n\nArm UNSIGNED update mode?\nThe next update may install firmware\nwhose origin cannot be verified.\nValid for ONE update, this boot only."
