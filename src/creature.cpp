// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "creature.h"
#include "creature_rules.h"
#include <esp_random.h>
namespace {
const cyd32_creature_t *kAll[CYD32_CREA_COUNT] = {
    &cyd32_fenni, &cyd32_lumo, &cyd32_krabbi, &cyd32_poulpi};
// Trait overlays become visible at these sizes (1..4): TRK AP HOT / OPEN 2.4 5 / 6 7
const uint8_t kTraitMinSize[8] = {2, 2, 2, 3, 3, 3, 4, 4};
uint8_t line = 0, sz = 0, cth = 0, unlocked = 0, newTraits = 0;
uint16_t recent = 0, uniques = 0, drones = 0;
uint32_t bonusXp = 0;
bool sizeChanged = false, cthUp = false, isHatched = false, hatchEvt = false;
uint8_t cthFromDrones(uint16_t n) {
  uint8_t s = 0;
  for (uint8_t i = 0; i < 5; i++) if (n >= CYD32_CTHULHU_DRONES[i]) s = (uint8_t)(i + 1);
  return s;
}
uint8_t traitsFor(uint8_t seen, uint8_t sizeNow) {
  uint8_t bits = 0;
  for (uint8_t i = 0; i < 8; i++)
    if ((seen & (uint8_t)(1u << i)) && sizeNow >= kTraitMinSize[i]) bits |= (uint8_t)(1u << i);
  return bits;
}
}
namespace creature {
void begin() {
  line = (uint8_t)(esp_random() % CYD32_CREA_COUNT);
  sz = 0; cth = 0; unlocked = 0; newTraits = 0;
  recent = 0; uniques = 0; drones = 0; bonusXp = 0;
  sizeChanged = false; cthUp = false; isHatched = false; hatchEvt = false;
}
void update(uint16_t recent3m, uint16_t unique24h, uint16_t drones24h, uint8_t seenTraits) {
  recent = recent3m;
  uniques = unique24h;
  drones = drones24h;
  const uint8_t s = creature_rules::sizeFor(recent3m, isHatched);
  if (s >= 1 && !isHatched) { isHatched = true; hatchEvt = true; }
  if (s != sz) { sz = s; sizeChanged = true; }
  const uint8_t c = cthFromDrones(drones24h);
  if (c > cth) { cth = c; cthUp = true; }
  // Unlocked traits stay unlocked when the creature shrinks (sticky this boot).
  const uint8_t bits = traitsFor(seenTraits, sz) | unlocked;
  newTraits |= (uint8_t)(bits & ~unlocked);
  unlocked = bits;
}
uint8_t size() { return sz; }
bool hatched() { return isHatched; }
uint8_t cthulhuStage() { return cth; }
uint8_t unlockedTraits() { return unlocked; }
uint8_t consumeNewTraits() { const uint8_t t = newTraits; newTraits = 0; return t; }
bool consumeSizeChanged() { const bool e = sizeChanged; sizeChanged = false; return e; }
bool consumeCthulhuUp() { const bool e = cthUp; cthUp = false; return e; }
bool consumeHatched() { const bool e = hatchEvt; hatchEvt = false; return e; }
const char *name() { return kAll[line]->name; }
const cyd32_creature_t *spec() { return kAll[line]; }
uint32_t recentCount() { return recent; }
uint32_t uniqueCount() { return uniques; }
uint32_t droneCount() { return drones; }
uint32_t xp() { return (uint32_t)uniques + (uint32_t)drones * 10u + bonusXp; }
void addXp(uint32_t v) { bonusXp += v; }
uint32_t nextSizeAt() { return creature_rules::nextAt(sz); }
uint32_t sizeFloor() { return creature_rules::floorAt(sz); }
}
