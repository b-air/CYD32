// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// 320x170 keyboard UI: SONAR / DASH / CREA / MENU. No GRID, no TORPEDO.
#include "app.hpp"
#include "lvgl.h"
#include "creature_view.hpp"
#include "live.hpp"
#include "radio.hpp"
#include "Arduino.h"
#include "bearing.h"
#include "creature.h"
#include "cyd32_palette.h"
#include "cyd32_sonar_theme.h"
#include "detections.h"
#include "privacy.h"
#include "prox_beep.h"
#include "rssi_track.h"
#include "signatures.h"
#include "ui_common.h"
#include "ui_feed.h"
#include "ui_radar.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#ifndef CYD32_CZ_VERSION
#define CYD32_CZ_VERSION "dev"
#endif
#ifndef CYD32_CZ_COMMIT
#define CYD32_CZ_COMMIT "unknown"
#endif

namespace czui {
enum Page { PG_SONAR, PG_DASH, PG_CREA, PG_MENU, PG_DETAIL, PG_WARD, PG_ABOUT, PG_UPDATE };
constexpr int kW = 320, kH = 170, kHead = 16, kTabs = 22;
Page page = PG_SONAR, back = PG_SONAR;
int focus = 0, sonar_i = -1;
bool beep_on = false, have_pin = false, sim = true, quit = false;
uint32_t pin = 0, beep_at = 0, flash_until = 0;
det::Cat mode = det::CAT_ALL;
uic::View view = uic::V_WIFI;
det::Entry shown{};
bool have_shown = false;
lv_obj_t *scr = nullptr, *header = nullptr, *radar = nullptr;
radar::Style style{};
CzOpts opt;
int frame_n = 0;
bool self_ok = true;

lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }
lv_color_t hex(uint32_t v) { return lv_color_hex(v); }

void sim_tick() {
  if (!sim) return;
  const uint32_t t = millis();
  const int8_t rssi = (int8_t)(-78 + (int)((t / 180) % 48));
  const uint8_t ap[6] = {0x02, 0x10, 0x20, 0x30, 0x40, 0x01};
  const uint8_t fl[6] = {0xB4, 0x1E, 0x52, 0x10, 0x00, 0x01};
  const uint8_t ble[6] = {0x02, 0xAA, 0x00, 0x00, 0x10, 0x01};
  const uint8_t dr[6] = {0x02, 0xD0, 0x00, 0x00, 0x00, 0x02};
  det::report(det::WIFI, ap, rssi, 6, det::traitBit(det::AP) | det::traitBit(det::G24) | det::traitBit(det::OPEN), sig::RANDOM_ADDR);
  det::report(det::WIFI, fl, (int8_t)(rssi - 8), 11, det::traitBit(det::AP) | det::traitBit(det::G24), sig::FLOCK);
  det::report(det::BLE, ble, (int8_t)(rssi + 6), 0, det::traitBit(det::TRK), sig::TRK | sig::RANDOM_ADDR);
  det::report(det::DRONE, dr, (int8_t)(rssi - 4), 6, 0, 0);
}

void counts_refresh() {
  sim_tick();
  if (!sim) live_poll();
  const auto c = det::counts();
  creature::update(c.recent3m, c.unique24h, (uint16_t)c.droneEver, c.seenTraits);
}

lv_obj_t *lab(lv_obj_t *p, const char *t, int x, int y, const lv_font_t *f, uint16_t col) {
  lv_obj_t *o = lv_label_create(p);
  lv_label_set_text(o, t);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_text_font(o, f, 0);
  lv_obj_set_style_text_color(o, C(col), 0);
  return o;
}

void header_text() {
  if (!header) return;
  char b[96];
  const char *radio = sim ? "SIM" : live_status();
  snprintf(b, sizeof b, "%s %s%s %s", page == PG_SONAR ? det::catName(mode) : page == PG_DASH ? uic::kViewTxt[view] : "CYD32",
           beep_on ? "BEEP" : "mute", (beep_on && (int32_t)(millis() - flash_until) < 0) ? "*" : "", radio);
  lv_label_set_text(header, b);
}

void beep_poll() {
  if (!beep_on) return;
  int8_t dbm = 0;
  if (!det::beepTarget(mode, have_pin, pin, dbm)) return;
  const uint32_t iv = beep_rules::intervalMs(dbm);
  if (!iv) return;
  const uint32_t now = millis();
  if ((int32_t)(now - beep_at) >= 0) { flash_until = now + beep_rules::kToneMs; beep_at = now + iv; }
}

void style_scope() {
  memset(&style, 0, sizeof style);
  style.cx = 78; style.cy = 64; style.rim = 52;
  style.rings[0] = (int16_t)rssi::radius(-50, style.rim, 6, style.rim - 6);
  style.rings[1] = (int16_t)rssi::radius(-65, style.rim, 6, style.rim - 6);
  style.rings[2] = (int16_t)rssi::radius(-80, style.rim, 6, style.rim - 6);
  style.closeR = style.rings[0];
  style.veryCloseR = (int16_t)rssi::radius(-42, style.rim, 6, style.rim - 6);
  style.bg = CYD32_SONAR_BG_CYD;
  style.ringCol = CYD32_SONAR_G2;
  style.wedge = cyd32_sonar_wedge_cyd;
  style.droneRamp = cyd32_sonar_drone_decay_cyd;
  style.rimW = 1; style.tick5 = 2; style.tick10 = 3; style.tick30 = 4;
  style.skip0 = 1; style.skip1 = 0; style.crossR = 4;
  style.idFont = &lv_font_unscii_8;
  style.echoR = 2; style.boxSize = 11; style.boxW = 1; style.lineW = 1; style.underW = 1;
  style.sweep = true;
}

int prev_clockwise(const det::Entry *e, int n, uint32_t cur, bool have) {
  if (n <= 0) return -1;
  int cur_key = -1;
  if (have) for (int i = 0; i < n; i++) if (e[i].id == cur) { cur_key = e[i].bearing * 1000 + (int)(e[i].id % 1000); break; }
  int best = -1, best_key = -1, last = 0, last_key = -1;
  for (int i = 0; i < n; i++) {
    const int key = e[i].bearing * 1000 + (int)(e[i].id % 1000);
    if (key > last_key) { last_key = key; last = i; }
    if (cur_key >= 0 && key < cur_key && key > best_key) { best_key = key; best = i; }
  }
  return best >= 0 ? best : last;
}

void paint_sonar() {
  det::Entry ents[32];
  const int n = det::snapshot(ents, 32, mode);
  if (sonar_i >= n) sonar_i = n - 1;
  radar::Contact cs[32];
  const uint32_t now = millis();
  for (int i = 0; i < n; i++) {
    uic::toContact(style, ents[i], now, cs[i]);
    uic::tag2(ents[i].id, cs[i].id);
  }
  if (radar) radar::setContacts(radar, cs, n, sonar_i);
  have_shown = sonar_i >= 0 && sonar_i < n;
  if (have_shown) shown = ents[sonar_i];
}

void add_tabs(lv_obj_t *s) {
  const char *t[4] = {"SONAR", "DASH", "CREA", "MENU"};
  const Page pg[4] = {PG_SONAR, PG_DASH, PG_CREA, PG_MENU};
  for (int i = 0; i < 4; i++) {
    lv_obj_t *o = lab(s, t[i], 8 + i * 78, kH - kTabs + 2, &lv_font_montserrat_12, page == pg[i] ? CYD32_COL_AMBER : CYD32_COL_CYAN);
    (void)o;
  }
}

void open_page(Page p);

void fill_body(lv_obj_t *s) {
  const int y0 = kHead + 2;
  if (page == PG_SONAR) {
    radar = radar::create(s, 0, kHead, 168, kH - kHead - kTabs, style);
    paint_sonar();
    char b[80] = "no contact";
    if (have_shown) {
      char id[5], badges[32];
      uic::tag4(shown.id, id);
      uic::badgeStr(shown.badges, badges, sizeof badges, true);
      snprintf(b, sizeof b, "%s %s\n%d dBm %s\nch %u\nangle indicative", id, uic::kindName(shown.kind),
               det::smoothed(shown), rssi::zone(det::smoothed(shown)), shown.channel);
      if (badges[0]) { strncat(b, "\n", sizeof b - strlen(b) - 1); strncat(b, badges, sizeof b - strlen(b) - 1); }
    }
    lab(s, b, 170, y0, &lv_font_montserrat_12, CYD32_COL_WHITE);
    return;
  }
  if (page == PG_DASH) {
    const auto c = det::counts();
    char sum[96];
    snprintf(sum, sizeof sum, "W %u B %u R %u  TRK %u META %u FLK %u\n3 min %u   24 h %u", c.wifi, c.ble, c.drone, c.trk, c.meta, c.flock, c.recent3m, c.unique24h);
    lab(s, sum, 4, y0, &lv_font_unscii_8, CYD32_COL_WHITE);
    det::Entry ents[8];
    det::LogEvt logs[8];
    int n = 0;
    if (view == uic::V_LOG) n = det::logSnapshot(logs, 6);
    else n = det::snapshot(ents, 96, view == uic::V_WIFI ? det::CAT_WIFI : view == uic::V_BLE ? det::CAT_BLE : view == uic::V_RID ? det::CAT_RID : det::CAT_ALL);
    int row = 0;
    if (view == uic::V_TAGGED) {
      det::Entry all[96];
      const int m = det::snapshot(all, 96, det::CAT_ALL);
      n = 0;
      for (int i = 0; i < m && n < 8; i++) if (det::isTagged(all[i])) ents[n++] = all[i];
    }
    if (view == uic::V_LOG) {
      for (int i = 0; i < n && row < 6; i++, row++) {
        char id[5], mm[8], line[48];
        uic::tag4(logs[i].id, id);
        uic::mmss(logs[i].ms, mm);
        snprintf(line, sizeof line, "%s %s %s %d", focus == row ? ">" : " ", mm, id, logs[i].rssi);
        lab(s, line, 4, 36 + row * 14, &lv_font_unscii_8, CYD32_COL_WHITE);
      }
    } else {
      for (int i = 0; i < n && row < 6; i++, row++) {
        char id[5], badges[24], line[64];
        uic::tag4(ents[i].id, id);
        uic::badgeStr(ents[i].badges, badges, sizeof badges, true);
        snprintf(line, sizeof line, "%s %s %d", focus == row ? ">" : " ", id, det::smoothed(ents[i]));
        if (badges[0]) { strncat(line, " ", sizeof line - strlen(line) - 1); strncat(line, badges, sizeof line - strlen(line) - 1); }
        lab(s, line, 4, 36 + row * 14, &lv_font_unscii_8, ents[i].kind == det::DRONE ? CYD32_COL_AMBER : CYD32_COL_WHITE);
        if (focus == row) { shown = ents[i]; have_shown = true; }
      }
    }
    if (n == 0) lab(s, "nothing visible", 4, 36, &lv_font_montserrat_12, CYD32_SONAR_G3);
    lab(s, focus == n ? "> ABOUT" : "  ABOUT", 4, 36 + 6 * 14, &lv_font_unscii_8, CYD32_COL_CYAN);
    return;
  }
  if (page == PG_MENU) {
    for (int i = 0; i < uic::kMenuN; i++) {
      char line[32];
      const char *name = uic::kMenuTxt[i];
      if (i == 4) name = beep_on ? "BEEP ON" : "BEEP OFF";
      if (i == 5) name = "OTA";
      snprintf(line, sizeof line, "%s %s", focus == i ? ">" : " ", name);
      lab(s, line, 8, y0 + i * 18, &lv_font_montserrat_14, focus == i ? CYD32_COL_AMBER : CYD32_COL_WHITE);
    }
    return;
  }
  if (page == PG_DETAIL && have_shown) {
    char id[9], badges[40], b[160];
    privacy::idToStr(shown.id, id);
    uic::badgeStr(shown.badges, badges, sizeof badges, true);
    const uint16_t mhz = cz::lookup_mhz(shown.id);
    snprintf(b, sizeof b, "%s  %s\n%d dBm  %s\n%s%u%s\n%s\nbearing %u  indicative\n%s",
             id, uic::kindName(shown.kind), det::smoothed(shown), rssi::zone(det::smoothed(shown)),
             mhz >= 5925 ? "6 GHz " : mhz >= 5000 ? "5 GHz " : mhz ? "2.4 GHz " : "ch ",
             mhz ? (unsigned)mhz : shown.channel, mhz ? " MHz" : "",
             badges[0] ? badges : "-", shown.bearing, have_pin && pin == shown.id ? "TRACK" : "enter = track");
    lab(s, b, 8, y0, &lv_font_montserrat_12, CYD32_COL_WHITE);
    return;
  }
  if (page == PG_WARD) {
    lab(s, "RAM log only. microSD is the system disk.", 4, y0, &lv_font_unscii_8, CYD32_COL_AMBER);
    det::LogEvt logs[6];
    const int n = det::logSnapshot(logs, 6);
    for (int i = 0; i < n; i++) {
      char id[5], mm[8], line[40];
      uic::tag4(logs[i].id, id);
      uic::mmss(logs[i].ms, mm);
      snprintf(line, sizeof line, "%s %s %s", mm, uic::kindName(logs[i].kind), id);
      lab(s, line, 4, 28 + i * 14, &lv_font_unscii_8, CYD32_COL_WHITE);
    }
    if (!n) lab(s, "empty", 4, 28, &lv_font_montserrat_12, CYD32_SONAR_G3);
    return;
  }
  if (page == PG_ABOUT) {
    char b[240];
    snprintf(b, sizeof b, "CYD32 %s\n%s\nAGPL-3.0-only\ngithub.com/b-air/CYD32\n%s\ninternal Wi-Fi stays managed\nRX only, no probe, no connect",
             CYD32_CZ_VERSION, CYD32_CZ_COMMIT, sim ? "radio SIM" : live_status());
    lab(s, b, 6, y0, &lv_font_unscii_8, CYD32_COL_WHITE);
    return;
  }
  if (page == PG_UPDATE) {
    lab(s, "Linux update, later.", 8, 40, &lv_font_montserrat_14, CYD32_COL_AMBER);
    lab(s, "No ESP32 OTA on this board.", 8, 64, &lv_font_montserrat_12, CYD32_COL_WHITE);
    return;
  }
  if (page == PG_CREA) crea_open(s);
}

void open_page(Page p) {
  if (page == PG_CREA) crea_close();
  feed::stop();
  if (p != page) focus = 0;
  page = p;
  lv_obj_t *old = scr;
  scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, hex(CYD32_SONAR_BG_CYD_HEX), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  header = lab(scr, "", 4, 0, &lv_font_unscii_8, CYD32_SONAR_G4);
  radar = nullptr;
  fill_body(scr);
  add_tabs(scr);
  header_text();
  lv_screen_load(scr);
  if (old) lv_obj_delete(old);
}

void move_sonar(int dir) {
  det::Entry ents[32];
  const int n = det::snapshot(ents, 32, mode);
  if (n <= 0) { sonar_i = -1; have_shown = false; return; }
  const uint32_t cur = sonar_i >= 0 && sonar_i < n ? ents[sonar_i].id : 0;
  const int idx = dir > 0 ? uic::nextClockwise(ents, n, cur, sonar_i >= 0) : prev_clockwise(ents, n, cur, sonar_i >= 0);
  sonar_i = idx;
  if (idx >= 0) { pin = ents[idx].id; have_pin = true; }
}

void on_select() {
  if (page == PG_SONAR) {
    if (have_shown) { back = PG_SONAR; open_page(PG_DETAIL); }
    return;
  }
  if (page == PG_DASH) {
    det::Entry ents[8];
    int n = 0;
    if (view != uic::V_LOG && view != uic::V_TAGGED) {
      const det::Cat c = view == uic::V_WIFI ? det::CAT_WIFI : view == uic::V_BLE ? det::CAT_BLE : view == uic::V_RID ? det::CAT_RID : det::CAT_ALL;
      n = det::snapshot(ents, 8, c);
    } else if (view == uic::V_TAGGED) {
      det::Entry all[96];
      const int m = det::snapshot(all, 96, det::CAT_ALL);
      for (int i = 0; i < m && n < 8; i++) if (det::isTagged(all[i])) ents[n++] = all[i];
    }
    if (view != uic::V_LOG && focus < n) { shown = ents[focus]; have_shown = true; back = PG_DASH; open_page(PG_DETAIL); return; }
    if (focus >= n) { back = PG_DASH; open_page(PG_ABOUT); }
    return;
  }
  if (page == PG_MENU) {
    if (focus == 4) { beep_on = !beep_on; open_page(PG_MENU); return; }
    if (focus == 5) { back = PG_MENU; open_page(PG_UPDATE); return; }
    const ui::Screen dest = uic::kMenuScr[focus];
    back = PG_MENU;
    if (dest == ui::SONAR) open_page(PG_SONAR);
    else if (dest == ui::COUNTERS) open_page(PG_DASH);
    else if (dest == ui::WARDRIVING) open_page(PG_WARD);
    else if (dest == ui::CREATURE) open_page(PG_CREA);
    return;
  }
  if (page == PG_DETAIL && have_shown) { have_pin = true; pin = shown.id; open_page(PG_DETAIL); }
  if (page == PG_CREA) crea_select();
}

void on_act(int act) {
  if (act == CZ_QUIT) { quit = true; return; }
  if (act == CZ_BEEP) { beep_on = !beep_on; if (page == PG_MENU) open_page(PG_MENU); return; }
  if (act == CZ_HOME || act == CZ_TAB1) { open_page(PG_SONAR); return; }
  if (act == CZ_TAB2) { open_page(PG_DASH); return; }
  if (act == CZ_TAB3) { open_page(PG_CREA); return; }
  if (act == CZ_TAB4) { open_page(PG_MENU); return; }
  if (act == CZ_TAB_NEXT) {
    const Page order[] = {PG_SONAR, PG_DASH, PG_CREA, PG_MENU};
    int i = 0;
    for (; i < 4; i++) if (page == order[i] || (page != PG_SONAR && page != PG_DASH && page != PG_CREA && page != PG_MENU)) break;
    open_page(order[(i + 1) % 4]);
    return;
  }
  if (act == CZ_ABOUT) { back = page; open_page(PG_ABOUT); return; }
  if (act == CZ_BACK) { open_page(page == PG_SONAR ? PG_SONAR : back == page ? PG_SONAR : back); if (page != PG_SONAR) back = PG_SONAR; return; }
  if (act == CZ_MODE_NEXT || act == CZ_MODE_PREV) {
    const int step = act == CZ_MODE_NEXT ? 1 : -1;
    if (page == PG_DASH) view = (uic::View)(((int)view + step + uic::V_COUNT) % uic::V_COUNT);
    else mode = det::nextCat(mode, step);
    det::setSonarCat(mode);
    open_page(page == PG_DASH ? PG_DASH : PG_SONAR);
    return;
  }
  if (act == CZ_PREV || act == CZ_NEXT) {
    const int dir = act == CZ_NEXT ? 1 : -1;
    if (page == PG_SONAR) { move_sonar(dir); open_page(PG_SONAR); return; }
    if (page == PG_CREA) { crea_focus(dir); return; }
    if (page == PG_MENU) { focus = (focus + dir + uic::kMenuN) % uic::kMenuN; open_page(PG_MENU); return; }
    if (page == PG_DASH) {
      focus += dir;
      if (focus < 0) focus = 0;
      if (focus > 6) focus = 6;
      open_page(PG_DASH);
      return;
    }
  }
  if (act == CZ_SELECT) on_select();
}

int run_ui(const CzOpts &o) {
  opt = o;
  sim = o.sim;
  privacy::begin();
  det::begin();
  creature::begin();
  det::setSonarCat(det::CAT_ALL);
  if (!sim) {
    std::string helper = o.helper ? o.helper : "";
    if (helper.empty()) {
      char exe[4096];
      const ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
      if (n > 0) {
        exe[n] = 0;
        std::string p(exe);
        const auto slash = p.rfind('/');
        if (slash != std::string::npos) helper = p.substr(0, slash + 1) + "cyd32-awus-helper";
      }
    }
    std::string err = helper.empty() ? "helper missing" : "";
    if (helper.empty() || !live_start(helper, o.sysfs ? o.sysfs : "/sys", err)) live_set_status(err);
  }
  style_scope();
  open_page(PG_SONAR);
  const int limit = o.frames > 0 ? o.frames : (o.self_test ? 40 : 0);
  while (!quit && (limit == 0 || frame_n < limit)) {
    counts_refresh();
    beep_poll();
    header_text();
    if (page == PG_SONAR) paint_sonar();
    if (o.self_test) {
      if (frame_n == 12) on_act(CZ_TAB3);
      else if (frame_n == 22) on_act(CZ_TAB2);
      else if (frame_n == 30) on_act(CZ_TAB4);
    }
    lv_timer_handler();
    usleep(5000);
    frame_n++;
  }
  if (page == PG_CREA) crea_close();
  if (!sim) live_stop();
  if (o.self_test) {
    const auto c = det::counts();
    self_ok = c.wifi >= 1 && c.ble >= 1 && c.drone >= 1 && creature::hatched();
    fprintf(stderr, "self-test %s wifi=%u ble=%u drone=%u hatched=%d\n", self_ok ? "OK" : "FAIL",
            c.wifi, c.ble, c.drone, creature::hatched() ? 1 : 0);
    return self_ok ? 0 : 1;
  }
  return 0;
}
}  // namespace czui

void cz_act(int act) { czui::on_act(act); }
int cz_app(const CzOpts &o) { return czui::run_ui(o); }
