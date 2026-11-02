/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Design assets, modified for CYD32. */
#pragma once
#include "lvgl.h"
/* CYD32 creature sprites: 5 creatures x 5 stages, 64x64 LV_COLOR_FORMAT_I4 (LVGL 9) */

LV_IMAGE_DECLARE(fenni_s0);
LV_IMAGE_DECLARE(fenni_s1);
LV_IMAGE_DECLARE(fenni_s2);
LV_IMAGE_DECLARE(fenni_s3);
LV_IMAGE_DECLARE(fenni_s4);
LV_IMAGE_DECLARE(sirra_s0);
LV_IMAGE_DECLARE(sirra_s1);
LV_IMAGE_DECLARE(sirra_s2);
LV_IMAGE_DECLARE(sirra_s3);
LV_IMAGE_DECLARE(sirra_s4);
LV_IMAGE_DECLARE(lumo_s0);
LV_IMAGE_DECLARE(lumo_s1);
LV_IMAGE_DECLARE(lumo_s2);
LV_IMAGE_DECLARE(lumo_s3);
LV_IMAGE_DECLARE(lumo_s4);
LV_IMAGE_DECLARE(krabbi_s0);
LV_IMAGE_DECLARE(krabbi_s1);
LV_IMAGE_DECLARE(krabbi_s2);
LV_IMAGE_DECLARE(krabbi_s3);
LV_IMAGE_DECLARE(krabbi_s4);
LV_IMAGE_DECLARE(poulpi_s0);
LV_IMAGE_DECLARE(poulpi_s1);
LV_IMAGE_DECLARE(poulpi_s2);
LV_IMAGE_DECLARE(poulpi_s3);
LV_IMAGE_DECLARE(poulpi_s4);

#define CYD32_CREA_STAGES 5
static const uint8_t cyd32_evo_threshold[CYD32_CREA_STAGES] = {0, 1, 5, 12, 20}; /* drones */

/* Delta patch: I4 pixels (index 0 = transparent) blitted over a RAM copy of the base image.
   dx/dy = object offset applied with lv_obj_set_style_translate_x/y for this frame. */
typedef struct { uint8_t x, y, w, h; uint16_t ofs; uint16_t ms; int8_t dx, dy; } cyd32_patch_t;
typedef struct {
  const lv_image_dsc_t *base;   /* same palette as patches */
  const uint8_t *pix;
  const cyd32_patch_t *patch;   /* idle frames first, then reaction frames */
  uint8_t n_idle, n_react;
  const lv_image_dsc_t *evo_a, *evo_b; /* A1 silhouettes for evolution to next stage (NULL at stage 4) */
} cyd32_anim_t;

extern const cyd32_anim_t fenni_s0_anim;
extern const cyd32_anim_t fenni_s1_anim;
extern const cyd32_anim_t fenni_s2_anim;
extern const cyd32_anim_t fenni_s3_anim;
extern const cyd32_anim_t fenni_s4_anim;
extern const cyd32_anim_t sirra_s0_anim;
extern const cyd32_anim_t sirra_s1_anim;
extern const cyd32_anim_t sirra_s2_anim;
extern const cyd32_anim_t sirra_s3_anim;
extern const cyd32_anim_t sirra_s4_anim;
extern const cyd32_anim_t lumo_s0_anim;
extern const cyd32_anim_t lumo_s1_anim;
extern const cyd32_anim_t lumo_s2_anim;
extern const cyd32_anim_t lumo_s3_anim;
extern const cyd32_anim_t lumo_s4_anim;
extern const cyd32_anim_t krabbi_s0_anim;
extern const cyd32_anim_t krabbi_s1_anim;
extern const cyd32_anim_t krabbi_s2_anim;
extern const cyd32_anim_t krabbi_s3_anim;
extern const cyd32_anim_t krabbi_s4_anim;
extern const cyd32_anim_t poulpi_s0_anim;
extern const cyd32_anim_t poulpi_s1_anim;
extern const cyd32_anim_t poulpi_s2_anim;
extern const cyd32_anim_t poulpi_s3_anim;
extern const cyd32_anim_t poulpi_s4_anim;

/* evolution timeline (ms): silhouette 150, flash 80, warp A 120, flash 80, warp B 120, flash 80, reveal 200 */
static const uint16_t cyd32_evo_ms[7] = {150, 80, 120, 80, 120, 80, 200};
