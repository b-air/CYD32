// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Exact CYD 320x240 chrome. Values match the pre-split layout (header 18 px,
// tabs at y=200, content 320x182) so the rectangular HUD does not change.
#pragma once
#define CYD32_SCREEN_W      320
#define CYD32_SCREEN_H      240
#define CYD32_HEADER_X      0
#define CYD32_HEADER_Y      0
#define CYD32_HEADER_W      320
#define CYD32_HEADER_H      18
#define CYD32_CONTENT_X     0
#define CYD32_CONTENT_Y     18
#define CYD32_CONTENT_W     320
#define CYD32_CONTENT_H     182
#define CYD32_TAB_X         0
#define CYD32_TAB_Y         200
#define CYD32_TAB_W         320
#define CYD32_TAB_H         40
#define CYD32_TAB_BTN_W     80
#define CYD32_CREATURE_ZOOM 256
/* Creature stages 1..4 (egg = stage 1): LVGL zoom of the 64 px sprite,
   72 / 88 / 104 / 120 px. */
static const uint16_t CYD32_STAGE_ZOOM[4] = {288, 352, 416, 480};
#define CYD32_HOLD_HINT     "Hold BOOT 3 s: unsigned update"
#define CYD32_UNSIGNED_BODY "BOOT button held.\n\nArm UNSIGNED update mode?\nThe next update may install firmware\nwhose origin cannot be verified.\nValid for ONE update, this boot only."
