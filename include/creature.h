// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Tamagotchi creature: evolves with the number of distinct drones (Remote ID)
// seen since boot, using cyd32_evo_threshold {0,1,5,12,20}. State is RAM only.
#pragma once
#include <stdint.h>
#include "lvgl.h"
#include "cyd32_sprites.h"
namespace creature {
void begin();                    // picks a random lineage for this boot
void update(uint32_t dronesEver, uint32_t wifi, uint32_t ble);
uint8_t stage();
uint32_t xp();
void addXp(uint32_t v);
const char *name();
const lv_image_dsc_t *sprite();
const cyd32_anim_t *anim(uint8_t stage);  // idle/reaction patches + evolution masks
bool consumeEvolved();           // true once after a stage change
uint32_t threshold(uint8_t stage);   // drones needed for a stage, 0 past the last stage
constexpr uint8_t kStages = 5;
}
