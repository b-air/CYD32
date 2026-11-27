// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// CardputerZero panel. APPLaunch starts this process, pauses its own keyboard
// thread, and waits. This process does not grab the keypad: the launcher keeps
// reading the same device so a long Esc can deliver SIGTERM.
#include "app.hpp"
#include "lvgl.h"
#include <ctype.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fstream>
#include <string>
namespace {
int kfd = -1;
volatile sig_atomic_t g_stop = 0;
void on_term(int) { g_stop = 1; }

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
bool readable(const char *p) { return p && *p && access(p, R_OK) == 0; }

std::string keyboard_named_tca8418() {
  std::ifstream in("/proc/bus/input/devices");
  if (!in) return {};
  std::string line, name, handlers;
  auto take = [&]() -> std::string {
    std::string low = name;
    for (char &c : low) c = (char)tolower((unsigned char)c);
    if (low.find("tca8418") == std::string::npos) return {};
    const auto ev = handlers.find("event");
    if (ev == std::string::npos) return {};
    std::string num;
    for (size_t i = ev + 5; i < handlers.size() && isdigit((unsigned char)handlers[i]); i++) num.push_back(handlers[i]);
    if (num.empty()) return {};
    return "/dev/input/event" + num;
  };
  while (std::getline(in, line)) {
    if (line.empty()) {
      const std::string path = take();
      name.clear();
      handlers.clear();
      if (!path.empty() && readable(path.c_str())) return path;
      continue;
    }
    if (line.compare(0, 3, "N: ") == 0) name = line;
    else if (line.compare(0, 3, "H: ") == 0) handlers = line;
  }
  const std::string path = take();
  if (!path.empty() && readable(path.c_str())) return path;
  return {};
}

std::string find_keyboard() {
  if (const char *e = getenv("CYD32_EVDEV"); readable(e)) return e;
  if (readable("/dev/input/cardputer-zero-internal")) return "/dev/input/cardputer-zero-internal";
  if (const std::string named = keyboard_named_tca8418(); !named.empty()) return named;
  if (readable("/dev/input/by-path/platform-3f804000.i2c-event"))
    return "/dev/input/by-path/platform-3f804000.i2c-event";
  if (const char *e = getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE"); readable(e)) return e;
  return {};
}

void poll_keys(lv_timer_t *) {
  if (g_stop) { cz_act(CZ_QUIT); return; }
  if (kfd < 0) return;
  struct input_event ev;
  while (read(kfd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
    if (ev.type != EV_KEY || ev.value == 0) continue;
    const int act = map_key(ev.code);
    if (act) cz_act(act);
  }
}

void blank_fb(const char *path) {
  const int fd = open(path, O_RDWR);
  if (fd < 0) return;
  struct fb_fix_screeninfo fix {};
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0 && fix.smem_len > 0) {
    void *p = mmap(nullptr, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p != MAP_FAILED) {
      memset(p, 0, fix.smem_len);
      munmap(p, fix.smem_len);
    }
  }
  close(fd);
}
}  // namespace

int main(int argc, char **argv) {
  CzOpts opt;
  const char *fb = nullptr;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--self-test")) { opt.self_test = true; opt.sim = true; if (!opt.frames) opt.frames = 40; }
    else if (!strcmp(a, "--awus")) opt.sim = false;
    else if (!strcmp(a, "--frames") && i + 1 < argc) opt.frames = atoi(argv[++i]);
    else if (!strcmp(a, "--sys") && i + 1 < argc) opt.sysfs = argv[++i];
    else if (!strcmp(a, "--helper") && i + 1 < argc) opt.helper = argv[++i];
    else if (!strcmp(a, "--fb") && i + 1 < argc) fb = argv[++i];
    else { fprintf(stderr, "cyd32-ui [--awus] [--self-test] [--frames N] [--fb PATH]\n"); return 2; }
  }
  if (!fb) {
    const char *env = getenv("APPLAUNCH_LINUX_FBDEV_DEVICE");
    fb = (env && *env) ? env : "/dev/fb0";
  }
  struct sigaction sa {};
  sa.sa_handler = on_term;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGHUP, &sa, nullptr);

  lv_init();
  lv_display_t *disp = lv_linux_fbdev_create();
  if (!disp) { fprintf(stderr, "framebuffer failed\n"); return 2; }
  lv_linux_fbdev_set_file(disp, fb);
  const std::string keys = find_keyboard();
  if (!keys.empty()) kfd = open(keys.c_str(), O_RDONLY | O_NONBLOCK);
  if (kfd < 0) fprintf(stderr, "keyboard not opened\n");
  lv_timer_create(poll_keys, 16, nullptr);
  const int rc = cz_app(opt);
  if (kfd >= 0) close(kfd);
  blank_fb(fb);
  return rc;
}
