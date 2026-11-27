// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// CardputerZero panel: Linux framebuffer + the keyboard event device.
#include "app.hpp"
#include "lvgl.h"
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
namespace {
int kfd = -1;
int map_key(int code) {
  switch (code) {
    case KEY_UP: case KEY_LEFT: return CZ_PREV;
    case KEY_DOWN: case KEY_RIGHT: return CZ_NEXT;
    case KEY_ENTER: case KEY_SPACE: return CZ_SELECT;
    case KEY_ESC: return CZ_BACK;
    case KEY_TAB: return CZ_TAB_NEXT;
    case KEY_1: return CZ_TAB1;
    case KEY_2: return CZ_TAB2;
    case KEY_3: return CZ_TAB3;
    case KEY_4: return CZ_TAB4;
    case KEY_H: return CZ_HOME;
    case KEY_M: case KEY_RIGHTBRACE: return CZ_MODE_NEXT;
    case KEY_LEFTBRACE: return CZ_MODE_PREV;
    case KEY_B: return CZ_BEEP;
    case KEY_A: return CZ_ABOUT;
    case KEY_Q: return CZ_QUIT;
    default: return CZ_NONE;
  }
}
void poll_keys(lv_timer_t *) {
  if (kfd < 0) return;
  struct input_event ev;
  while (read(kfd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
    if (ev.type != EV_KEY || ev.value == 0) continue;
    const int act = map_key(ev.code);
    if (act) cz_act(act);
  }
}
}  // namespace

int main(int argc, char **argv) {
  CzOpts opt;
  const char *fb = "/dev/fb0";
  const char *ev = getenv("CYD32_EVDEV");
  if (!ev) ev = "/dev/input/event0";
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--self-test")) { opt.self_test = true; opt.sim = true; if (!opt.frames) opt.frames = 40; }
    else if (!strcmp(a, "--awus")) opt.sim = false;
    else if (!strcmp(a, "--frames") && i + 1 < argc) opt.frames = atoi(argv[++i]);
    else if (!strcmp(a, "--sys") && i + 1 < argc) opt.sysfs = argv[++i];
    else if (!strcmp(a, "--helper") && i + 1 < argc) opt.helper = argv[++i];
    else if (!strcmp(a, "--fb") && i + 1 < argc) fb = argv[++i];
    else { fprintf(stderr, "cyd32-ui [--awus] [--frames N] [--fb /dev/fb0]\n"); return 2; }
  }
  lv_init();
  lv_display_t *disp = lv_linux_fbdev_create();
  if (!disp) { fprintf(stderr, "framebuffer failed\n"); return 2; }
  lv_linux_fbdev_set_file(disp, fb);
  kfd = open(ev, O_RDONLY | O_NONBLOCK);
  if (kfd >= 0) lv_timer_create(poll_keys, 16, nullptr);
  else fprintf(stderr, "keyboard %s not opened\n", ev);
  return cz_app(opt);
}
