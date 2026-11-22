// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Tamagotchi creature: one of four lineages. Size follows the number of
// distinct devices seen in the last 3 minutes (creature_rules.h): egg at 0,
// hatch at 1, sizes 2 / 3 / 4 at 30 / 60 / 120, shrinking again when fewer
// devices are around. Cthulhu stages from unique Remote ID drones (24 h).
// Traits unlock on the first sighting of each source type, gated by size.
// State is RAM only.
#pragma once
#include <stdint.h>
#include "cyd32_creature.h"
namespace creature {
void begin();  // picks a random lineage for this boot
void update(uint16_t recent3m, uint16_t unique24h, uint16_t drones24h, uint8_t seenTraits);
uint8_t size();             // 0 = egg, 1..4
bool hatched();
uint8_t cthulhuStage();     // 0 = not transformed, 1..5
uint8_t unlockedTraits();   // bitmask, same order as cyd32_trait_t[8]
uint8_t consumeNewTraits(); // bits that newly unlocked since last call
bool consumeSizeChanged();  // true once after a size change (grow or shrink)
bool consumeCthulhuUp();    // true once after a Cthulhu stage increase
bool consumeHatched();      // true once when the egg hatches
const char *name();
const cyd32_creature_t *spec();
uint32_t recentCount();     // distinct devices in the last 3 minutes
uint32_t uniqueCount();     // unique addresses, 24 h window
uint32_t droneCount();
uint32_t xp();
void addXp(uint32_t v);
uint32_t nextSizeAt();      // recent devices needed for the next size, 0 at max
uint32_t sizeFloor();       // recent devices that keep the current size
}
