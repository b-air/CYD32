/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors (design assets, unmodified below this header) */
#pragma once
#include "lvgl.h"
/* 25 creature sprites 64x64 I4 */
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
