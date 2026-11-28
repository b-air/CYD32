/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
#pragma once
// Linux evdev codes from the CardputerZero v5 keypad (m5stack,tca8418c).
// Returns a CzAct. Safe to call before LVGL starts.
int cz_map_evdev(int code);
// 0 when the v5 arrow layer, the letter layer, and keys 4–8 match CzAct.
int cz_evdev_map_check();
