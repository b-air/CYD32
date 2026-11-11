/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Design assets, modified for CYD32. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* CYD32 creatures v7 - generated. I4 keyframe + per-frame (dx,dy,nrect,{x,y,w,h,I4 px}) deltas.
   Runtime (shared code, no data): growth zoom, trait overlays, breathing (scale_y), dithered stage dissolve,
   procedural sigil ring + runes, pulse ring, palette flash, glitch row-shift. LVGL 9 cannot scale I4:
   decode the current frame into one shared RGB565A8 buffer (64x64x3 = 12 KB RAM) then lv_image_set_scale. */
typedef struct { const uint32_t *pal; const uint8_t *key; const uint8_t *delta; const uint16_t *off; uint16_t n; uint16_t ms; } cyd32_anim_t;
typedef struct { const uint8_t *px; const uint8_t *anchor; uint8_t w, h, inst; } cyd32_trait_t; /* anchor: 2 x (x,y) per adult_idle frame */
typedef struct { uint8_t ring, hot, tick, rune, seed; } cyd32_sigil_t; /* palette indices into cth palette */
typedef struct { const char *name; const cyd32_anim_t *hatch,*baby_idle,*baby_happy,*adult_idle,*adult_happy,*cth_keys,*cth_idle; const cyd32_trait_t *traits; cyd32_sigil_t sigil; } cyd32_creature_t;
typedef enum { CYD32_CREA_FENNI=0, CYD32_CREA_LUMO, CYD32_CREA_KRABBI, CYD32_CREA_POULPI, CYD32_CREA_COUNT } cyd32_crea_id_t;
/* cth_keys: 10 frames = stage 0.5,1,1.5,...,5 (odd index = stage key); traits order: BT tracker, Wi-Fi AP, hotspot, open public, 2.4 GHz, 5 GHz, Wi-Fi 6, Wi-Fi 7 */
/* THRESHOLDS - option A (chosen), shared by all creatures */
#define CYD32_DEDUP_WINDOW_S (24u*3600u)
static const uint16_t CYD32_SIZE_THRESH[4]={10,40,120,300};      /* sizes 2..5, cumulative unique detections */
static const uint8_t  CYD32_CTHULHU_DRONES[5]={1,3,6,10,15};      /* stages 1..5, cumulative drones */
static const uint16_t CYD32_SIZE_ZOOM[5]={256,169,197,225,256};  /* size 1 = baby anims native; 2..5 = adult anims zoomed */
/* first sighting of each source type unlocks its trait overlay + one sigil rune */
extern const cyd32_creature_t cyd32_fenni, cyd32_lumo, cyd32_krabbi, cyd32_poulpi;
#ifdef __cplusplus
}
#endif
