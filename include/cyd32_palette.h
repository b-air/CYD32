/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Design assets, modified for CYD32. */
#pragma once
#include "cyd32_sonar_theme.h"
/* CYD32 Y3-d screen palette, RGB565 values + RGB888 hex, after the v9
   harmonisation (design README section 7): token swap only, no layout change.
   Background / panels / lines / muted labels move to the phosphor greens, the
   orange becomes the #FCEE0A charter yellow, drones are #FCEE0A, non-drone
   hits are G4, the tab buttons use the sonar nav style. Cyan, white and
   #40FC40 stay. The sonar and boot screens use cyd32_sonar_theme.h. */
#define CYD32_COL_BG        CYD32_SONAR_BG_CYD  /* #000A04 (was #081020) */
#define CYD32_COL_PANEL     CYD32_SONAR_G1      /* #02200E panels, cells, header strip (was #102038) */
#define CYD32_COL_GRID      CYD32_SONAR_G2      /* #0B5A2A lines, borders, selection (was #203048) */
#define CYD32_COL_AMBER     CYD32_SONAR_DRONE   /* #FCEE0A titles, axis labels (was #F8A000 orange) */
#define CYD32_COL_HITX      CYD32_SONAR_G4      /* #3CFF7A non-drone grid hits (was #F8A000) */
#define CYD32_COL_CYAN      0x07DF  /* #00F8F8 accent */
#define CYD32_COL_HIT       0xF908  /* #F82040 hit / drone */
#define CYD32_COL_WATER     CYD32_SONAR_G3      /* #19A04A muted labels (was #486888) */
#define CYD32_COL_OK        0x47E8  /* #40FC40 OK */
#define CYD32_COL_DRONE     CYD32_SONAR_DRONE   /* #FCEE0A drones only (grid hits, R counter) */
#define CYD32_COL_WHITE     0xFFFF  /* #F8FCF8 text */
/* creatures (shared ramps) */
#define CYD32_COL_OUTLINE   0x1864  /* #180C20 outline */
#define CYD32_COL_GLOW      0x079F  /* #00F0FF eye glow */
#define CYD32_COL_EYE_Y     0xFF40  /* #F8E800 cosmic eyes */
#define CYD32_COL_ABYSS     0x1088  /* #141040 abyss */
