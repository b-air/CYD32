// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "creature.h"
#include <esp_random.h>
#include "cyd32_sprites.h"
namespace {
const char *kNames[5] = {"FENNI", "SIRRA", "LUMO", "KRABBI", "POULPI"};
const lv_image_dsc_t *kSprites[5][5] = {
    {&fenni_s0, &fenni_s1, &fenni_s2, &fenni_s3, &fenni_s4},
    {&sirra_s0, &sirra_s1, &sirra_s2, &sirra_s3, &sirra_s4},
    {&lumo_s0, &lumo_s1, &lumo_s2, &lumo_s3, &lumo_s4},
    {&krabbi_s0, &krabbi_s1, &krabbi_s2, &krabbi_s3, &krabbi_s4},
    {&poulpi_s0, &poulpi_s1, &poulpi_s2, &poulpi_s3, &poulpi_s4}};
const cyd32_anim_t *kAnims[5][5] = {
    {&fenni_s0_anim, &fenni_s1_anim, &fenni_s2_anim, &fenni_s3_anim, &fenni_s4_anim},
    {&sirra_s0_anim, &sirra_s1_anim, &sirra_s2_anim, &sirra_s3_anim, &sirra_s4_anim},
    {&lumo_s0_anim, &lumo_s1_anim, &lumo_s2_anim, &lumo_s3_anim, &lumo_s4_anim},
    {&krabbi_s0_anim, &krabbi_s1_anim, &krabbi_s2_anim, &krabbi_s3_anim, &krabbi_s4_anim},
    {&poulpi_s0_anim, &poulpi_s1_anim, &poulpi_s2_anim, &poulpi_s3_anim, &poulpi_s4_anim}};
uint8_t line = 0, stg = 0;
uint32_t bonusXp = 0, baseXp = 0;
bool evolved = false;
}
namespace creature {
void begin() { line = esp_random() % 5; stg = 0; bonusXp = 0; }
void update(uint32_t drones, uint32_t wifi, uint32_t ble) {
  baseXp = drones * 10 + wifi + ble;
  uint8_t s = 0;
  for (uint8_t i = 0; i < CYD32_CREA_STAGES; i++) if (drones >= cyd32_evo_threshold[i]) s = i;
  if (s > stg) { stg = s; evolved = true; }
}
uint8_t stage() { return stg; }
uint32_t xp() { return baseXp + bonusXp; }
void addXp(uint32_t v) { bonusXp += v; }
const char *name() { return kNames[line]; }
const lv_image_dsc_t *sprite() { return kSprites[line][stg]; }
const cyd32_anim_t *anim(uint8_t s) { return kAnims[line][s < CYD32_CREA_STAGES ? s : CYD32_CREA_STAGES - 1]; }
bool consumeEvolved() { bool e = evolved; evolved = false; return e; }
uint32_t threshold(uint8_t s) { return s < CYD32_CREA_STAGES ? cyd32_evo_threshold[s] : 0; }
}
