// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Desktop / VPS entry. --self-test uses the SDL dummy video driver.
#include "app.hpp"
#include "radio.hpp"
#include "lvgl.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace {
int map_key(SDL_Keycode k) {
  switch (k) {
    case SDLK_UP: case SDLK_LEFT: return CZ_PREV;
    case SDLK_DOWN: case SDLK_RIGHT: return CZ_NEXT;
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return CZ_SELECT;
    case SDLK_ESCAPE: return CZ_BACK;
    case SDLK_TAB: return CZ_TAB_NEXT;
    case SDLK_1: return CZ_TAB1;
    case SDLK_2: return CZ_TAB2;
    case SDLK_3: return CZ_TAB3;
    case SDLK_4: return CZ_TAB4;
    case SDLK_h: return CZ_HOME;
    case SDLK_RIGHTBRACKET: case SDLK_m: return CZ_MODE_NEXT;
    case SDLK_LEFTBRACKET: return CZ_MODE_PREV;
    case SDLK_b: return CZ_BEEP;
    case SDLK_a: return CZ_ABOUT;
    case SDLK_q: return CZ_QUIT;
    default: return CZ_NONE;
  }
}
int watch(void *, SDL_Event *e) {
  if (e->type == SDL_KEYDOWN) {
    const int act = map_key(e->key.keysym.sym);
    if (act) cz_act(act);
  }
  return 1;
}
void dry_run(const char *sys) {
  const auto d = cz::detect_ifaces(sys ? sys : "/sys");
  for (const auto &iface : d.ifaces) {
    fprintf(stdout, "%s %s driver=%s usb=%s\n", iface.internal ? "skip" : "see", iface.name.c_str(),
                 iface.driver.empty() ? "-" : iface.driver.c_str(), iface.usb_id.empty() ? "-" : iface.usb_id.c_str());
  }
  fprintf(stdout, "chosen %s\n", d.chosen.empty() ? "none" : d.chosen.c_str());
  if (!d.note.empty()) fprintf(stdout, "note %s\n", d.note.c_str());
  if (d.chosen.empty()) {
    fprintf(stdout, "no capture interface; arm plan not executed\n");
    return;
  }
  cz::SavedLink s;
  s.ifname = d.chosen;
  s.type = "managed";
  s.nm_managed = true;
  fprintf(stdout, "# arm\n");
  for (const auto &c : cz::plan_arm(s)) fprintf(stdout, "%s\n", cz::join_cmd(c).c_str());
  fprintf(stdout, "# restore\n");
  for (const auto &c : cz::plan_restore(s)) fprintf(stdout, "%s\n", cz::join_cmd(c).c_str());
}
}  // namespace

int main(int argc, char **argv) {
  CzOpts opt;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--self-test")) { opt.self_test = true; opt.sim = true; if (!opt.frames) opt.frames = 40; }
    else if (!strcmp(a, "--dry-run")) opt.dry_run = true;
    else if (!strcmp(a, "--awus")) opt.sim = false;
    else if (!strcmp(a, "--frames") && i + 1 < argc) opt.frames = atoi(argv[++i]);
    else if (!strcmp(a, "--sys") && i + 1 < argc) opt.sysfs = argv[++i];
    else if (!strcmp(a, "--helper") && i + 1 < argc) opt.helper = argv[++i];
    else {
      fprintf(stderr, "cyd32-ui [--self-test] [--dry-run] [--awus] [--frames N] [--sys PATH] [--helper PATH]\n");
      return 2;
    }
  }
  if (opt.dry_run) dry_run(opt.sysfs);
  if (opt.dry_run && !opt.self_test && opt.frames <= 0) return 0;
  if (opt.self_test && !getenv("SDL_VIDEODRIVER")) setenv("SDL_VIDEODRIVER", "dummy", 0);
  lv_init();
  lv_display_t *disp = lv_sdl_window_create(320, 170);
  if (!disp) { fprintf(stderr, "SDL window failed\n"); return 2; }
  SDL_AddEventWatch(watch, nullptr);
  return cz_app(opt);
}
