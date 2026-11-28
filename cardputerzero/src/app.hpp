/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
#pragma once
// Actions from the CardputerZero keyboard (and from the SDL host window).
enum CzAct : int {
  CZ_NONE = 0, CZ_PREV, CZ_NEXT, CZ_SELECT, CZ_BACK, CZ_TAB_NEXT,
  CZ_TAB1, CZ_TAB2, CZ_TAB3, CZ_TAB4, CZ_HOME, CZ_MODE_NEXT, CZ_MODE_PREV,
  CZ_BEEP, CZ_ABOUT, CZ_QUIT
};
struct CzOpts {
  bool self_test = false;
  bool sim = true;
  bool dry_run = false;
  int frames = 0;          // 0 = run until quit
  const char *sysfs = "/sys";
  const char *helper = nullptr;
};
int cz_app(const CzOpts &opt);
void cz_act(int act);
