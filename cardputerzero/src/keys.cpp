// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// CardputerZero v5 keypad, from m5stack/m5stack-linux-dtoverlays
// modules/CardputerZero/cardputerzero-v5-overlay.dts (linux,keymap2),
// commit a3fd3e83c0247e16e194e7e40f7fc1f5be933dd4.
// The kernel emits the Fn-layer code. This UI has no text field, so the
// unshifted letters under those legends navigate as well.
//
//   Fn+F  KEY_UP         103   up
//   Fn+X  KEY_DOWN       108   down
//   Fn+Z  KEY_LEFT       105   left
//   Fn+C  KEY_RIGHT      106   right
//   Fn+E  KEY_NEXTSONG   163   next track (not an arrow; treated as next)
//   F Z X C E            33 44 45 46 18
// Keys 1–4 stay SONAR DASH CREA MENU. Keys 5–7 repeat DASH CREA MENU.
// Key 8, under the right side of the panel, confirms.
#include "keys.hpp"
#include "app.hpp"
#include <stdio.h>

namespace {
constexpr int kEsc = 1, k1 = 2, k2 = 3, k3 = 4, k4 = 5, k5 = 6, k6 = 7, k7 = 8, k8 = 9;
constexpr int kTab = 15, kQ = 16, kE = 18, kEnter = 28, kF = 33, kH = 35, kZ = 44, kX = 45;
constexpr int kC = 46, kB = 48, kM = 50, kSpace = 57, kLeftBrace = 26, kRightBrace = 27, kA = 30;
constexpr int kUp = 103, kLeft = 105, kRight = 106, kDown = 108, kNextSong = 163;
constexpr int kFn = 389;  // KEY_DVD, the Fn key itself

int expect(int code, int act) { return cz_map_evdev(code) == act ? 0 : 1; }
}  // namespace

int cz_map_evdev(int code) {
  switch (code) {
    case kUp: case kF: case kLeft: case kZ: return CZ_PREV;
    case kDown: case kX: case kRight: case kC: case kE: case kNextSong: return CZ_NEXT;
    case kEnter: case kSpace: case k8: return CZ_SELECT;
    case kEsc: return CZ_BACK;
    case kTab: return CZ_TAB_NEXT;
    case k1: return CZ_TAB1;
    case k2: case k5: return CZ_TAB2;
    case k3: case k6: return CZ_TAB3;
    case k4: case k7: return CZ_TAB4;
    case kH: return CZ_HOME;
    case kM: case kRightBrace: return CZ_MODE_NEXT;
    case kLeftBrace: return CZ_MODE_PREV;
    case kB: return CZ_BEEP;
    case kA: return CZ_ABOUT;
    case kQ: return CZ_QUIT;
    case kFn: return CZ_NONE;
    default: return CZ_NONE;
  }
}

int cz_evdev_map_check() {
  int bad = 0;
  bad |= expect(kUp, CZ_PREV);
  bad |= expect(kF, CZ_PREV);
  bad |= expect(kLeft, CZ_PREV);
  bad |= expect(kZ, CZ_PREV);
  bad |= expect(kDown, CZ_NEXT);
  bad |= expect(kX, CZ_NEXT);
  bad |= expect(kRight, CZ_NEXT);
  bad |= expect(kC, CZ_NEXT);
  bad |= expect(kE, CZ_NEXT);
  bad |= expect(kNextSong, CZ_NEXT);
  bad |= expect(k1, CZ_TAB1);
  bad |= expect(k2, CZ_TAB2);
  bad |= expect(k3, CZ_TAB3);
  bad |= expect(k4, CZ_TAB4);
  bad |= expect(k5, CZ_TAB2);
  bad |= expect(k6, CZ_TAB3);
  bad |= expect(k7, CZ_TAB4);
  bad |= expect(k8, CZ_SELECT);
  bad |= expect(kQ, CZ_QUIT);
  bad |= expect(kEsc, CZ_BACK);
  bad |= expect(kFn, CZ_NONE);
  if (bad) fprintf(stderr, "evdev map check failed\n");
  else fprintf(stderr, "evdev map check ok\n");
  return bad ? 1 : 0;
}
