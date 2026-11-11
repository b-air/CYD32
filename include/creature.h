// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Tamagotchi creature: one of four lineages, grown from unique detections
// (24 h address dedup) and Cthulhu stages from unique Remote ID drones.
// Traits unlock on the first sighting of each source type, gated by size.
// State is RAM only.
#pragma once
#include <stdint.h>
#include "cyd32_creature.h"
namespace creature {
void begin();  // picks a random lineage for this boot
void update(uint16_t unique24h, uint16_t drones24h, uint8_t seenTraits);
uint8_t size();             // 1..5
uint8_t cthulhuStage();     // 0 = not transformed, 1..5
uint8_t unlockedTraits();   // bitmask, same order as cyd32_trait_t[8]
uint8_t consumeNewTraits(); // bits that newly unlocked since last call
bool consumeGrew();         // true once after a size increase
bool consumeCthulhuUp();    // true once after a Cthulhu stage increase
bool consumeHatched();      // true if hatch should play (first size-1 showing)
const char *name();
const cyd32_creature_t *spec();
uint32_t uniqueCount();
uint32_t droneCount();
uint32_t xp();
void addXp(uint32_t v);
uint32_t nextSizeAt();      // unique detections needed for the next size, 0 at max
uint32_t sizeFloor();       // unique detections that unlocked the current size
}
