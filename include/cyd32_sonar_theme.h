/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Design assets (v9 phosphor sonar), unmodified below this line. */
/*
 * cyd32_sonar_theme.h — v9 phosphor sonar theme: palette, geometry, sweep/decay constants.
 * Targets: CYD 320x240 (ILI9341) and M5Stack StopWatch 466x466 round AMOLED (CO5300).
 * Design spec only. UNTESTED ON HARDWARE. Values shown on reference renders are EXAMPLE DATA.
 */
#ifndef CYD32_SONAR_THEME_H
#define CYD32_SONAR_THEME_H

#include <stdint.h>

/* ---------- palette: RGB565 (for LovyanGFX/M5GFX) and 24-bit hex (for lv_color_hex) ---------- */
#define CYD32_SONAR_BG_CYD         0x0040u  /* #000A04 */
#define CYD32_SONAR_BG_CYD_HEX     0x000A04u
#define CYD32_SONAR_BG_SW          0x0000u  /* #000000 */
#define CYD32_SONAR_BG_SW_HEX      0x000000u
#define CYD32_SONAR_G1             0x0101u  /* #02200E */
#define CYD32_SONAR_G1_HEX         0x02200Eu
#define CYD32_SONAR_G2             0x0AC5u  /* #0B5A2A */
#define CYD32_SONAR_G2_HEX         0x0B5A2Au
#define CYD32_SONAR_G3             0x1D09u  /* #19A04A */
#define CYD32_SONAR_G3_HEX         0x19A04Au
#define CYD32_SONAR_G4             0x3FEFu  /* #3CFF7A */
#define CYD32_SONAR_G4_HEX         0x3CFF7Au
#define CYD32_SONAR_G5             0xBFFAu  /* #B8FFD0 */
#define CYD32_SONAR_G5_HEX         0xB8FFD0u
#define CYD32_SONAR_DRONE          0xFF61u  /* #FCEE0A */
#define CYD32_SONAR_DRONE_HEX      0xFCEE0Au

/* DRONE (Y3-d yellow) is reserved for drones / Remote ID: drone echoes, R and DRN counters, drone label + box text. */

/* ---------- sweep ---------- */
#define CYD32_SONAR_SWEEP_PERIOD_MS   3000   /* one clockwise revolution */
#define CYD32_SONAR_FRAME_MS          50     /* 20 fps target (15 fps acceptable on CYD) */
#define CYD32_SONAR_SWEEP_STEP_DEG    6      /* 360 * FRAME_MS / PERIOD_MS */
#define CYD32_SONAR_WEDGE_DEG         40     /* trailing wedge behind the line */
#define CYD32_SONAR_WEDGE_STEPS       8      /* 8 sectors x 5 deg, one filled triangle each */
#define CYD32_SONAR_WEDGE_STEP_DEG    5
/* G4 opacity per sector, sector 0 = touching the sweep line (40 % -> 5 %) */
static const uint8_t  cyd32_sonar_wedge_opa[CYD32_SONAR_WEDGE_STEPS] = { 102, 89, 77, 64, 51, 38, 26, 13 };
/* same, pre-blended over each background (use when drawing without alpha) */
static const uint16_t cyd32_sonar_wedge_cyd[CYD32_SONAR_WEDGE_STEPS] = { 0x1B66, 0x1305, 0x12A4, 0x0A24, 0x09C3, 0x0962, 0x0102, 0x00A1 };
static const uint16_t cyd32_sonar_wedge_sw[CYD32_SONAR_WEDGE_STEPS]  = { 0x1B26, 0x12C5, 0x1264, 0x0A03, 0x0983, 0x0922, 0x00C1, 0x0060 };

/* ---------- echo decay: d = (sweep_deg - echo_bearing_deg) mod 360 ---------- */
#define CYD32_SONAR_DECAY_L4_DEG      12     /* d < 12  : L4 core G5 / halo G3 */
#define CYD32_SONAR_DECAY_L3_DEG      90     /* d < 90  : L3 core G4 / halo G2 */
#define CYD32_SONAR_DECAY_L2_DEG      220    /* d < 220 : L2 core G3 / halo G1 ; else L1 core G2 / halo G1 */
/* drone echo core per level L1..L4 (DRONE over background at 35/60/85/100 %) */
static const uint16_t cyd32_sonar_drone_decay_cyd[4] = { 0x5AC0, 0x9481, 0xD661, 0xFF61 };
static const uint16_t cyd32_sonar_drone_decay_sw[4]  = { 0x5A80, 0x9460, 0xD641, 0xFF61 };

/* ---------- ticks (drawn outward from the rim radius) ---------- */
#define CYD32_SONAR_TICK_STEP_DEG     5
#define CYD32_SONAR_LABEL_STEP_DEG    30     /* "%03d" bearing labels */

/* ---------- CYD 320x240: sonar ---------- */
#define CYD_SONAR_CX                  160
#define CYD_SONAR_CY                  110
static const uint8_t cyd_sonar_ring_r[4] = { 18, 36, 54, 72 };   /* last = rim (G3) */
#define CYD_SONAR_TICK_LEN_5          3      /* G2 */
#define CYD_SONAR_TICK_LEN_10         5      /* G3 */
#define CYD_SONAR_TICK_LEN_30         8      /* G4 */
#define CYD_SONAR_LABEL_R             88     /* unscii_8, centre of label */
#define CYD_SONAR_COL_PITCH_X10       144    /* A-J column pitch 14.4 px */
#define CYD_SONAR_ROW_PITCH           16     /* rows 1-8 */
#define CYD_SONAR_ECHO_R              2      /* core; halo = 4 */
#define CYD_SONAR_TARGET_BOX          13     /* corner-bracket box, legs 4 px */
#define CYD_HEADER_H                  17     /* G1 strip 0..16, G2 rule at y = 17 */
#define CYD_BUTTON_Y                  204
#define CYD_BUTTON_H                  35
#define CYD_BUTTON_W                  78
#define CYD_BUTTON_PITCH              80
#define CYD_BUTTON_CHAMFER            5      /* active = G4 fill + BG text, idle = G2 outline + G4 text */

/* ---------- CYD 320x240: boot ---------- */
#define CYD_BOOT_CX                   160
#define CYD_BOOT_CY                   120
static const uint8_t cyd_boot_ring_r[4] = { 25, 50, 75, 100 };   /* G1 rings, rim G3 */
#define CYD_BOOT_LABEL_R              114    /* skip 060 / 090 / 270 / 300 behind the title */
#define CYD_BOOT_LOGO_Y               48     /* img_cyd32_logo_cyd, centred on x */
#define CYD_BOOT_TITLE_Y              94     /* img_cyd32_title_cyd, centred on x */
#define CYD_BOOT_SUBTITLE_Y           128    /* "DEFENSIVE SONAR" unscii_8, G4, centred */
#define CYD_BOOT_BAR_X0               72
#define CYD_BOOT_BAR_X1               248
#define CYD_BOOT_BAR_Y                146    /* 7 px, outline G3, fill G4 */
#define CYD_BOOT_STATUS_Y             160

/* ---------- StopWatch 466x466 (integer centre 233; mockups use 232.5) ---------- */
#define SW_CX                         233
#define SW_CY                         233
#define SW_SAFE_R                     229    /* full panel: no ink beyond r = 227 (r = 233 is the glass edge) */
static const uint8_t sw_sonar_ring_r[4] = { 46, 92, 138, 184 };  /* last = rim (G3, 1-2 px) */
static const uint8_t sw_boot_ring_r[4]  = { 48, 97, 145, 194 };  /* G1 rings, rim + progress arc */
#define SW_TICK_LEN_5                 4
#define SW_TICK_LEN_10                7
#define SW_TICK_LEN_30                10
#define SW_LABEL_R                    207    /* unscii_16; pulled inward until far corner <= 226 */
#define SW_BOOT_LABEL_R               214
#define SW_CROSS_R                    138    /* crosshair stops at ring 3 */
#define SW_ECHO_R                     5      /* core; halo = 9 */
#define SW_TARGET_BOX                 28
/* Curved 4-tab band = the STANDARD StopWatch nav on every screen except boot, the running
   update and the unsigned-mode prompt. Active tab = G4 fill + BG text, idle = G1 fill + G2
   outline + G4 text, Montserrat 18. Hit-test in polar coordinates.
   StopWatch geometry uses the whole 466 px panel (outer ink at r <= 227). */
#define SW_TAB_R0                     160
#define SW_TAB_R1                     225
#define SW_TAB_BRG0                   130    /* MENU | CREA | GRID | SONAR from 130 to 230 deg */
#define SW_TAB_BRG1                   230
#define SW_TAB_SPAN_DEG               25
#define SW_TAB_GAP_DEG                2
#define SW_KEYTAG_TICK_R0             201    /* 2 px G4 tick at 270 (A BACK) and 090 (B SELECT) */
#define SW_KEYTAG_TICK_R1             210
#define SW_BOOT_LOGO_Y                116    /* img_m5stack_logo_sw */
#define SW_BOOT_TITLE_Y               182    /* img_cyd32_title_sw */
#define SW_BOOT_SUBTITLE_Y            236    /* "DEFENSIVE SONAR" unscii_16, G4 */
#define SW_BOOT_DEVICE_Y              270    /* "STOPWATCH" Montserrat 14, G3 */
/* StopWatch GRID (green-leaning): 10 x 8 cells of 22 px, origin below; labels unscii_16 G4 */
#define SW_GRID_X0                    123
#define SW_GRID_Y0                    106
#define SW_GRID_CELL                  22
#define SW_GRID_PLATE_Y0              292    /* chamfered info plate 200 x 44, G2 outline, G1 fill */
#define SW_GRID_PLATE_H               44
#define SW_RIM_R                      226    /* green bezel ring (G2) + 10/30 deg ticks, gap behind the tab band */
/* Wardriving (signal-only): radius from RSSI, angle = stable hash of the address (NOT a direction) */
static const int8_t sw_ward_ring_dbm[4] = { -40, -55, -70, -85 };
#define SW_WARD_RSSI_R(dbm)           ((((-(dbm)) - 25) * 184) / 60)   /* clamp to [8, 178] */

/* ---------- title / logo assets (I4, palette index 0 transparent) ---------- */
#if defined(LV_LVGL_H_INCLUDE_SIMPLE) || defined(LVGL_H)
LV_IMAGE_DECLARE(img_cyd32_logo_cyd);   /* 172x44 I4, 3848 B */
LV_IMAGE_DECLARE(img_cyd32_title_cyd);   /* 272x32 I4, 4416 B */
LV_IMAGE_DECLARE(img_m5stack_logo_sw);   /* 296x56 I4, 8352 B */
LV_IMAGE_DECLARE(img_cyd32_title_sw);   /* 404x44 I4, 8952 B */
#endif

#endif /* CYD32_SONAR_THEME_H */
