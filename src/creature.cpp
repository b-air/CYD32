// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "creature.h"
#include <esp_random.h>
namespace {
const cyd32_creature_t *kAll[CYD32_CREA_COUNT] = {
    &cyd32_fenni, &cyd32_lumo, &cyd32_krabbi, &cyd32_poulpi};
// Trait overlays become visible at these sizes (1..5): TRK AP / HOT OPEN / 2.4 5 / 6 7
const uint8_t kTraitMinSize[8] = {2, 2, 3, 3, 4, 4, 5, 5};
uint8_t line = 0, sz = 1, cth = 0, unlocked = 0, newTraits = 0;
uint16_t uniques = 0, drones = 0;
uint32_t bonusXp = 0;
bool grew = false, cthUp = false, hatched = false;
uint8_t sizeFromUniques(uint16_t n) {
  uint8_t s = 1;
  for (uint8_t i = 0; i < 4; i++) if (n >= CYD32_SIZE_THRESH[i]) s = (uint8_t)(i + 2);
  return s;
}
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
  sz = 1; cth = 0; unlocked = 0; newTraits = 0;
  uniques = 0; drones = 0; bonusXp = 0;
  grew = false; cthUp = false; hatched = false;
}
void update(uint16_t unique24h, uint16_t drones24h, uint8_t seenTraits) {
  uniques = unique24h;
  drones = drones24h;
  const uint8_t s = sizeFromUniques(unique24h);
  const uint8_t c = cthFromDrones(drones24h);
  if (s > sz) { sz = s; grew = true; }
  if (c > cth) { cth = c; cthUp = true; }
  const uint8_t bits = traitsFor(seenTraits, sz);
  newTraits |= (uint8_t)(bits & ~unlocked);
  unlocked = bits;
}
uint8_t size() { return sz; }
uint8_t cthulhuStage() { return cth; }
uint8_t unlockedTraits() { return unlocked; }
uint8_t consumeNewTraits() { const uint8_t t = newTraits; newTraits = 0; return t; }
bool consumeGrew() { const bool e = grew; grew = false; return e; }
bool consumeCthulhuUp() { const bool e = cthUp; cthUp = false; return e; }
bool consumeHatched() {
  if (hatched || sz > 1 || cth > 0) return false;
  hatched = true;
  return true;
}
const char *name() { return kAll[line]->name; }
const cyd32_creature_t *spec() { return kAll[line]; }
uint32_t uniqueCount() { return uniques; }
uint32_t droneCount() { return drones; }
uint32_t xp() { return (uint32_t)uniques + (uint32_t)drones * 10u + bonusXp; }
void addXp(uint32_t v) { bonusXp += v; }
uint32_t nextSizeAt() { return sz >= 5 ? 0 : CYD32_SIZE_THRESH[sz - 1]; }
uint32_t sizeFloor() { return sz <= 1 ? 0 : CYD32_SIZE_THRESH[sz - 2]; }
}
