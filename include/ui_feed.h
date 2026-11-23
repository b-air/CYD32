// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Creature FEED animation, shared by the CYD and the StopWatch: a small
// home-made drone (LVGL primitives only: an X frame, a body and four spinning
// rotors) flies a curve from the corner to the creature's mouth (~1.2 s),
// shrinking as it arrives; the creature then chomps (squash and stretch),
// crumbs fall and "+2 XP" floats up. No bitmap art.
#pragma once
#include <stdint.h>
#include "lvgl.h"
namespace feed {
struct Spec {
  int x0, y0;          // start (parent coordinates)
  int mouthX, mouthY;  // target
  int arm;             // drone half-span in px at full size
  uint16_t frame, rotor, crumb, text;  // RGB565 colours
  const lv_font_t *font;               // for "+2 XP"
};
constexpr uint32_t kFlyMs = 1200, kChompMs = 500, kTotalMs = 2000;
// Starts (or restarts) the animation on a full-size overlay child of parent.
void start(lv_obj_t *parent, const Spec &s);
bool active();
// True once when the drone reaches the mouth (the UI adds the XP then).
bool consumeChomp();
// Squash and stretch for the creature image (256 = 1.0), for the UI's zoom.
void creatureScale(uint16_t &sx, uint16_t &sy);
// Deletes the overlay and the timer (screen change). Safe when idle.
void stop();
// Host screenshots only: freeze the animation clock at t ms after start.
void freezeAt(int32_t t);
}
