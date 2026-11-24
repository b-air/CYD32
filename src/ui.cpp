// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#ifndef CYD32_HW_M5STACK_STOPWATCH
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "lvgl.h"
#include "cyd32_build_gen.h"
#include "cyd32_palette.h"
#include "creature.h"
#include "detections.h"
#include "scanner.h"
#include "fw_status.h"
#include "ota_portal.h"
#include "privacy.h"
#include "ui_layout.h"
#include "ui_radar.h"
#include "ui_common.h"
#include "rssi_track.h"
#include "signatures.h"
#include "beep.h"
#include "ui_feed.h"

namespace {
lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }
constexpr int kTop = CYD32_HEADER_H, kTabY = CYD32_TAB_Y, kContentH = CYD32_CONTENT_H;

lv_obj_t *scr, *header, *headerLbl, *statusLbl, *banner, *content, *tabbar;
ui::Screen cur = ui::BOOT;
bool updateReq = false, unsignedBanner = false;
int unsignedDecision = -1;
// per-screen widgets
lv_obj_t *bootBar, *bootPct, *bootStat[3], *bootLogo, *bootTitle, *scanLine;
lv_timer_t *fxTimer = nullptr;
uint32_t fxNext = 0, fxEnd = 0;
int16_t scanY = 0;
// v9 phosphor sonar: BOOT and SONAR draw on a full-screen "stage" (child of the
// screen, so the scope may overlap the header / tab rows). Deleted by show().
lv_obj_t *stage = nullptr, *radarObj = nullptr;
lv_obj_t *hudTgt, *hudDrn, *hudSel[3], *hudMode, *hudBeep;
lv_obj_t *hdr[7], *tabBtn[4];
bool phosphor = false;
lv_obj_t *creaImg, *creaInfo, *creaBar, *flash;
lv_obj_t *detRows[14], *detHit, *detBody;
// Selected device (sonar tap, list row) for DETAILS; tracked = pinned on the sonar.
det::Entry selEnt;
bool haveSel = false, hasTracked = false;
uint32_t trackedId = 0;
// One snapshot scratch for every view (LVGL task only). The view buffers are
// heap, allocated once in ui::begin(): the CYD static DRAM segment is full.
det::Entry *gSnap = nullptr;
// Previous screen for the on-screen BACK buttons (the CYD has no keys).
ui::Screen prevSc = ui::SONAR;
lv_timer_t *sonarTimer = nullptr;
constexpr int kSonarMax = 24;
det::Entry *sonarEnt = nullptr;
radar::Contact *sonarRc = nullptr;
int sonarN = 0, sonarSel = -1;
// Dashboard / lists
constexpr int kListRows = 9;
lv_obj_t *listRows[kListRows], *listEmpty, *dashTop;
det::Entry listEnt[kListRows];
int listN = 0;
lv_obj_t *otaQr, *otaInfo, *otaBar, *otaMsg, *otaConfirm;
uint32_t nowMs = 0;

lv_obj_t *label(lv_obj_t *p, const char *t, const lv_font_t *f, uint16_t col) {
  lv_obj_t *l = lv_label_create(p);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, C(col), 0);
  return l;
}
lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint16_t bg) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, C(bg), 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
lv_obj_t *button(lv_obj_t *p, const char *t, int x, int y, int w, int h, lv_event_cb_t cb, void *ud, uint16_t col = CYD32_COL_AMBER) {
  lv_obj_t *b = box(p, x, y, w, h, CYD32_COL_PANEL);
  lv_obj_set_style_border_color(b, C(col), 0); lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(b, C(CYD32_COL_GRID), LV_STATE_PRESSED);
  lv_obj_t *l = label(b, t, &lv_font_montserrat_14, col);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}
void onShow(lv_event_t *e) { ui::show((ui::Screen)(intptr_t)lv_event_get_user_data(e)); }
// 5 px top-left chamfer on the phosphor tab buttons (cut in the bar colour,
// idle buttons get a G2 diagonal to close the outline).
void onTabChamfer(lv_event_t *e) {
  lv_obj_t *b = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  lv_layer_t *L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(b, &a);
  const int k = CYD_BUTTON_CHAMFER;
  lv_draw_triangle_dsc_t t;
  lv_draw_triangle_dsc_init(&t);
  t.bg_color = C(CYD32_SONAR_BG_CYD);
  t.bg_opa = LV_OPA_COVER;
  t.p[0].x = a.x1; t.p[0].y = a.y1;
  t.p[1].x = a.x1 + k + 1; t.p[1].y = a.y1;
  t.p[2].x = a.x1; t.p[2].y = a.y1 + k + 1;
  lv_draw_triangle(L, &t);
  if (lv_obj_get_style_border_width(b, 0)) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = C(CYD32_SONAR_G2);
    d.width = 1;
    d.p1.x = a.x1; d.p1.y = a.y1 + k;
    d.p2.x = a.x1 + k; d.p2.y = a.y1;
    lv_draw_line(L, &d);
  }
}
lv_obj_t *stageLabel(const char *t, const lv_font_t *f, uint16_t col, lv_align_t al, int x, int y) {
  lv_obj_t *l = label(stage, t, f, col);
  lv_obj_align(l, al, x, y);
  return l;
}
lv_obj_t *makeStage(int y, int h) {
  stage = box(scr, 0, y, CYD32_SCREEN_W, h, CYD32_SONAR_BG_CYD);
  lv_obj_set_style_bg_opa(stage, LV_OPA_TRANSP, 0);
  return stage;
}

void buildChrome() {
  scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, C(CYD32_COL_BG), 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  // Header: G1 strip (y 0-16) with a G2 rule at y 17, on every screen.
  header = box(scr, CYD32_HEADER_X, CYD32_HEADER_Y, CYD32_HEADER_W, CYD32_HEADER_H, CYD32_COL_PANEL);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(header, C(CYD32_COL_GRID), 0);
  lv_obj_set_style_border_width(header, 1, 0);
  // "CYD32 W0143 B0389 R0002" as 7 labels (LVGL 9 has no inline recolor).
  static const int16_t hx[7] = {4, 52, 60, 100, 108, 148, 156};
  for (int i = 0; i < 7; i++) {
    hdr[i] = label(header, i == 0 ? "CYD32" : i == 1 ? "W" : i == 3 ? "B" : i == 5 ? "R" : "", &lv_font_unscii_8, CYD32_COL_AMBER);
    lv_obj_set_pos(hdr[i], hx[i], 4);
  }
  headerLbl = hdr[0];
  statusLbl = label(header, "", &lv_font_unscii_8, CYD32_COL_CYAN);
  lv_obj_align(statusLbl, LV_ALIGN_RIGHT_MID, -4, 0);
  banner = box(scr, CYD32_HEADER_X, CYD32_HEADER_Y, CYD32_HEADER_W, CYD32_HEADER_H, CYD32_COL_HIT);
  lv_obj_t *bl = label(banner, "UNSIGNED FIRMWARE MODE", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_center(bl);
  lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
  content = box(scr, CYD32_CONTENT_X, CYD32_CONTENT_Y, CYD32_CONTENT_W, CYD32_CONTENT_H, CYD32_COL_BG);
  tabbar = box(scr, CYD32_TAB_X, CYD32_TAB_Y, CYD32_TAB_W, CYD32_TAB_H, CYD32_COL_PANEL);
  const char *tabs[4] = {"SONAR", "DASH", "CREA", "MENU"};
  const ui::Screen ts[4] = {ui::SONAR, ui::COUNTERS, ui::CREATURE, ui::MENU};
  for (int i = 0; i < 4; i++) {
    tabBtn[i] = button(tabbar, tabs[i], i * CYD32_TAB_BTN_W, 0, CYD32_TAB_BTN_W, CYD32_TAB_H, onShow, (void *)(intptr_t)ts[i]);
    lv_obj_add_event_cb(tabBtn[i], onTabChamfer, LV_EVENT_DRAW_POST, nullptr);
  }
}

// Theme for the header and the tab buttons: v9 phosphor on the sonar screen,
// Y3-d (yellow HUD, harmonised greens) everywhere else.
void styleChrome() {
  const int active = cur == ui::SONAR ? 0 : cur == ui::COUNTERS ? 1 : cur == ui::CREATURE ? 2 : cur == ui::MENU ? 3 : -1;
  // The tab buttons use the sonar nav style on every screen (README section 7).
  lv_obj_set_style_bg_color(tabbar, C(CYD32_SONAR_BG_CYD), 0);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *b = tabBtn[i], *l = lv_obj_get_child(b, 0);
    const bool on = i == active;
    lv_obj_set_pos(b, i * CYD_BUTTON_PITCH + 1, CYD_BUTTON_Y - CYD32_TAB_Y);
    lv_obj_set_size(b, CYD_BUTTON_W, CYD_BUTTON_H);
    lv_obj_set_style_bg_color(b, C(on ? CYD32_SONAR_G4 : CYD32_SONAR_BG_CYD), 0);
    lv_obj_set_style_border_width(b, on ? 0 : 1, 0);
    lv_obj_set_style_border_color(b, C(CYD32_SONAR_G2), 0);
    lv_obj_set_style_bg_color(b, C(on ? CYD32_SONAR_G5 : CYD32_SONAR_G1), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(l, C(on ? CYD32_SONAR_BG_CYD : CYD32_SONAR_G4), 0);
    lv_obj_center(l);
  }
  lv_obj_set_style_text_color(statusLbl, C(phosphor ? CYD32_SONAR_G3 : CYD32_COL_CYAN), 0);
}

// --- v9 phosphor boot --------------------------------------------------------
// V3 effect, kept cheap: a 1 px scanline that walks down the screen (two rows
// invalidated per frame) and, every 1.5-3 s, a 100 ms burst that slides the
// logo and the title 2-3 px in opposite directions.
uint32_t mix(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; return x; }
void fxTick(lv_timer_t *) {
  const uint32_t now = lv_tick_get();
  if (scanLine) {
    scanY = (int16_t)((scanY + 3) % CYD32_SCREEN_H);
    lv_obj_set_y(scanLine, scanY);
  }
  if (!bootLogo || !bootTitle) return;
  if (fxEnd && now >= fxEnd) {
    fxEnd = 0;
    lv_obj_set_x(bootLogo, (CYD32_SCREEN_W - img_cyd32_logo_cyd.header.w) / 2);
    lv_obj_set_x(bootTitle, (CYD32_SCREEN_W - img_cyd32_title_cyd.header.w) / 2);
    if (scanLine) lv_obj_set_style_bg_color(scanLine, C(CYD32_SONAR_G3), 0);
  } else if (!fxEnd && now >= fxNext) {
    const uint32_t r = mix(now);
    const int d = (r & 1) ? 2 : 3, sgn = (r & 2) ? 1 : -1;
    fxEnd = now + 100;
    fxNext = now + 1500 + r % 1500;
    lv_obj_set_x(bootLogo, (CYD32_SCREEN_W - img_cyd32_logo_cyd.header.w) / 2 - sgn * d);
    lv_obj_set_x(bootTitle, (CYD32_SCREEN_W - img_cyd32_title_cyd.header.w) / 2 + sgn * d);
    if (scanLine) lv_obj_set_style_bg_color(scanLine, C(CYD32_SONAR_G4), 0);
  }
}
radar::Style cydStyle(bool boot) {
  radar::Style s{};
  s.bg = CYD32_SONAR_BG_CYD;
  s.wedge = cyd32_sonar_wedge_cyd;
  s.droneRamp = cyd32_sonar_drone_decay_cyd;
  s.rimW = 1;
  s.tick5 = CYD_SONAR_TICK_LEN_5; s.tick10 = CYD_SONAR_TICK_LEN_10; s.tick30 = CYD_SONAR_TICK_LEN_30;
  s.skip0 = 1; s.skip1 = 0;
  s.bearFont = &lv_font_unscii_8;
  s.bearRmax = 999;
  s.idFont = &lv_font_unscii_8;
  s.lineW = 1; s.underW = 2;
  s.sweep = true;
  if (boot) {
    s.cx = CYD_BOOT_CX; s.cy = CYD_BOOT_CY;
    for (int i = 0; i < 3; i++) s.rings[i] = cyd_boot_ring_r[i];
    s.rim = cyd_boot_ring_r[3];
    s.ringCol = CYD32_SONAR_G1;
    s.crossR = s.rim;
    s.bearR = CYD_BOOT_LABEL_R;
    s.bearMaj = CYD32_SONAR_G4; s.bearMin = CYD32_SONAR_G2;
    s.bearSkip = (1u << 2) | (1u << 3) | (1u << 9) | (1u << 10);  // 060 090 270 300 behind the titles
  } else {
    s.cx = CYD_SONAR_CX; s.cy = CYD_SONAR_CY - kTop;
    for (int i = 0; i < 3; i++) s.rings[i] = cyd_sonar_ring_r[i];
    s.rim = cyd_sonar_ring_r[3];
    s.ringCol = CYD32_SONAR_G2;
    s.crossR = s.rim;
    s.bearR = CYD_SONAR_LABEL_R;
    s.bearMaj = CYD32_SONAR_G4; s.bearMin = CYD32_SONAR_G3;
    s.echoR = CYD_SONAR_ECHO_R;
    s.boxSize = CYD_SONAR_TARGET_BOX;
    s.boxW = 1;
    // Rings are RSSI steps (rssi_track.h): ring 1 = -50 dBm is drawn as the
    // dotted CLOSE ring, plus a VERY CLOSE ring at -42 dBm.
    s.rings[0] = 0;
    s.closeR = cyd_sonar_ring_r[0];
    s.veryCloseR = (int16_t)(s.rim * (-35 - rssi::kVeryCloseDbm) / 60);
  }
  return s;
}

void buildBoot() {
  makeStage(0, CYD32_SCREEN_H);
  radarObj = radar::create(stage, 0, 0, CYD32_SCREEN_W, CYD32_SCREEN_H, cydStyle(true));
  bootLogo = lv_image_create(stage);
  lv_image_set_src(bootLogo, &img_cyd32_logo_cyd);
  lv_obj_set_pos(bootLogo, (CYD32_SCREEN_W - img_cyd32_logo_cyd.header.w) / 2, CYD_BOOT_LOGO_Y);
  bootTitle = lv_image_create(stage);
  lv_image_set_src(bootTitle, &img_cyd32_title_cyd);
  lv_obj_set_pos(bootTitle, (CYD32_SCREEN_W - img_cyd32_title_cyd.header.w) / 2, CYD_BOOT_TITLE_Y);
  stageLabel("DEFENSIVE SONAR", &lv_font_unscii_8, CYD32_SONAR_G4, LV_ALIGN_TOP_MID, 0, CYD_BOOT_SUBTITLE_Y);
  bootBar = lv_bar_create(stage);
  lv_obj_remove_style_all(bootBar);
  lv_obj_set_pos(bootBar, CYD_BOOT_BAR_X0, CYD_BOOT_BAR_Y);
  lv_obj_set_size(bootBar, CYD_BOOT_BAR_X1 - CYD_BOOT_BAR_X0, 7);
  lv_obj_set_style_border_color(bootBar, C(CYD32_SONAR_G3), 0);
  lv_obj_set_style_border_width(bootBar, 1, 0);
  lv_obj_set_style_pad_all(bootBar, 2, 0);
  lv_obj_set_style_bg_color(bootBar, C(CYD32_SONAR_G4), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bootBar, LV_OPA_COVER, LV_PART_INDICATOR);
  const char *st[3] = {"BLE --", "WIFI --", "RID --"};
  const int sx[3] = {88, 152, 224};
  for (int i = 0; i < 3; i++) bootStat[i] = stageLabel(st[i], &lv_font_unscii_8, CYD32_SONAR_G2, LV_ALIGN_TOP_LEFT, sx[i], CYD_BOOT_STATUS_Y);
  stageLabel(CYD32_HOLD_HINT, &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_MID, 0, 186);
  stageLabel("BOOT", &lv_font_unscii_8, CYD32_SONAR_G4, LV_ALIGN_TOP_LEFT, 4, 3);
  stageLabel("SELF-TEST", &lv_font_unscii_8, CYD32_SONAR_G2, LV_ALIGN_TOP_LEFT, 4, 13);
  bootPct = stageLabel("0%", &lv_font_unscii_8, CYD32_SONAR_G4, LV_ALIGN_TOP_RIGHT, -4, 3);
  stageLabel("LOAD", &lv_font_unscii_8, CYD32_SONAR_G2, LV_ALIGN_TOP_RIGHT, -4, 13);
  lv_obj_t *ver = stageLabel(CYD32_VERSION, &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_BOTTOM_LEFT, 4, -3);
  lv_label_set_long_mode(ver, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(ver, 136);  // keep clear of the "180" bearing
  scanLine = box(stage, 0, 0, CYD32_SCREEN_W, 1, CYD32_SONAR_G3);
  lv_obj_set_style_bg_opa(scanLine, 56, 0);  // style bg opacity, not object opacity: no layer
  scanY = 0;
  fxNext = lv_tick_get() + 1200;
  fxEnd = 0;
  fxTimer = lv_timer_create(fxTick, 50, nullptr);
}

// --- v9 phosphor sonar -------------------------------------------------------
void selectEntry(const det::Entry &e) { selEnt = e; haveSel = true; }
void refreshSonarContacts();
void sonarTick(lv_timer_t *) { refreshSonarContacts(); }
// Scope gestures, both handled on RELEASED (no CLICKED handler, so a swipe
// can never also open DETAILS): a horizontal swipe (|dx| > 40 px and more
// horizontal than vertical) changes the sonar mode, anything shorter than
// 12 px is a tap: the contact under the finger (else the selected one) opens
// DETAILS.
lv_point_t pressPt;
bool pressOn = false;
void onScopePress(lv_event_t *) { lv_indev_get_point(lv_indev_active(), &pressPt); pressOn = true; }
void refreshSonar();
void onScopeRelease(lv_event_t *) {
  if (!pressOn) return;
  pressOn = false;
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  const int mx = p.x - pressPt.x, my = p.y - pressPt.y;
  const int ax = mx < 0 ? -mx : mx, ay = my < 0 ? -my : my;
  if (ax > 40 && ax * 2 > ay * 3) {  // swipe left = next mode, swipe right = previous
    det::setSonarCat(det::nextCat(det::sonarCat(), mx < 0 ? 1 : -1));
    refreshSonar();
    return;
  }
  if (ax > 12 || ay > 12) return;
  if (!radarObj) return;  // screen rebuilt between press and release
  lv_area_t a;
  lv_obj_get_coords(radarObj, &a);
  const radar::Style s = cydStyle(false);
  const int x = p.x - a.x1 - s.cx, y = p.y - a.y1 - s.cy;
  int best = -1, bd = 16 * 16;
  for (int i = 0; i < sonarN; i++) {
    const int dx = sonarRc[i].dx - x, dy = sonarRc[i].dy - y, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0) best = sonarSel;
  if (best < 0) return;
  selectEntry(sonarEnt[best]);
  ui::show(ui::DETAILS);
}
void buildSonar() {
  makeStage(kTop, CYD_BUTTON_Y - kTop);
  radarObj = radar::create(stage, 0, 0, CYD32_SCREEN_W, CYD_BUTTON_Y - kTop, cydStyle(false));
  lv_obj_add_flag(radarObj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(radarObj, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_remove_flag(radarObj, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(radarObj, onScopePress, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(radarObj, onScopeRelease, LV_EVENT_RELEASED, nullptr);
  pressOn = false;
  stageLabel("TGT", &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_LEFT, 5, 4);
  hudTgt = stageLabel("00", &lv_font_unscii_16, CYD32_SONAR_G4, LV_ALIGN_TOP_LEFT, 5, 13);
  stageLabel("MODE", &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_LEFT, 5, 34);
  hudMode = stageLabel("ALL", &lv_font_unscii_8, CYD32_SONAR_G4, LV_ALIGN_TOP_LEFT, 5, 44);
  stageLabel("DRN", &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_RIGHT, -5, 4);
  hudDrn = stageLabel("00", &lv_font_unscii_16, CYD32_SONAR_DRONE, LV_ALIGN_TOP_RIGHT, -5, 13);
  hudBeep = stageLabel("BEEP", &lv_font_unscii_8, CYD32_SONAR_G4, LV_ALIGN_TOP_RIGHT, -5, 34);
  stageLabel("RX ONLY", &lv_font_unscii_8, CYD32_SONAR_G4, LV_ALIGN_TOP_LEFT, 5, 142);
  stageLabel("ANGLE IS", &lv_font_unscii_8, CYD32_SONAR_G2, LV_ALIGN_TOP_LEFT, 5, 152);
  stageLabel("INDICATIVE", &lv_font_unscii_8, CYD32_SONAR_G2, LV_ALIGN_TOP_LEFT, 5, 162);
  for (int i = 0; i < 3; i++) hudSel[i] = stageLabel("", &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_RIGHT, -5, 142 + i * 10);
  // Positions refresh at ~7 Hz (RSSI is smoothed per packet in det::report).
  sonarTimer = lv_timer_create(sonarTick, 150, nullptr);
}
// Radius = smoothed RSSI (dotted CLOSE ring = -50 dBm, VERY CLOSE = -42 dBm),
// angle = the indicative bearing (FNV-1a of the address, the same on every
// CYD32). There is no direction finding.
void refreshSonarContacts() {
  if (!radarObj || !sonarEnt || !sonarRc) return;
  const uint32_t now = lv_tick_get();
  sonarN = det::snapshot(sonarEnt, kSonarMax, det::sonarCat());
  const radar::Style s = cydStyle(false);
  int sel = -1, best = -100000, labels = 0;
  for (int i = 0; i < sonarN; i++) {
    // Selected: the pinned target if visible, else strongest drone, else strongest contact.
    const int score = (hasTracked && sonarEnt[i].id == trackedId ? 100000 : 0) + (sonarEnt[i].kind == det::DRONE ? 1000 : 0) + det::smoothed(sonarEnt[i]);
    if (score > best) { best = score; sel = i; }
  }
  for (int i = 0; i < sonarN; i++) {
    uic::toContact(s, sonarEnt[i], now, sonarRc[i]);
    if (i == sel || sonarRc[i].kind == radar::DRONE || labels < 5) {
      uic::tag2(sonarEnt[i].id, sonarRc[i].id);
      if (i != sel) labels++;
    }
  }
  sonarSel = sel;
  radar::setContacts(radarObj, sonarRc, sonarN, sel);
}
void refreshSonar() {
  if (!radarObj) return;
  refreshSonarContacts();
  const det::Counts cnt = det::counts();
  char t[24];
  snprintf(t, sizeof t, "%02u", (unsigned)LV_MIN(cnt.wifi + cnt.ble, 99));
  lv_label_set_text(hudTgt, t);
  snprintf(t, sizeof t, "%02u", (unsigned)LV_MIN(cnt.drone, 99));
  lv_label_set_text(hudDrn, t);
  lv_label_set_text(hudMode, det::catName(det::sonarCat()));
  if (beep::enabled()) lv_obj_remove_flag(hudBeep, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(hudBeep, LV_OBJ_FLAG_HIDDEN);
  if (sonarSel >= 0) {
    const det::Entry &e = sonarEnt[sonarSel];
    const bool drone = e.kind == det::DRONE;
    const int sm = det::smoothed(e);
    char tag[3];
    uic::tag2(e.id, tag);
    snprintf(t, sizeof t, "SEL %s%s", tag, sm >= rssi::kCloseDbm ? " CLOSE" : "");
    lv_label_set_text(hudSel[0], t);
    lv_obj_set_style_text_color(hudSel[0], C(drone ? CYD32_SONAR_DRONE : CYD32_SONAR_G4), 0);
    if (e.kind == det::WIFI) snprintf(t, sizeof t, "WIFI CH%u", e.channel);
    else snprintf(t, sizeof t, "%s", drone ? "REMOTE ID" : "BLE");
    lv_label_set_text(hudSel[1], t);
    snprintf(t, sizeof t, "%dDBM %03u", sm, (unsigned)e.bearing);
    lv_label_set_text(hudSel[2], t);
  } else {
    lv_label_set_text(hudSel[0], "NO CONTACT");
    lv_obj_set_style_text_color(hudSel[0], C(CYD32_SONAR_G3), 0);
    lv_label_set_text(hudSel[1], "");
    lv_label_set_text(hudSel[2], "");
  }
}

// Light creature renderer: decode the current I4 frame (key + sequential
// deltas) into one shared 64x64 RGB565A8 buffer (12 KB) and zoom with LVGL.
// Extra 4 KB: I4 playback state + a compose scratch so trait overlays do not
// bake into the animation buffer. LVGL 9 cannot scale I4 directly.
constexpr int kSpr = 64;
constexpr size_t kI4Size = kSpr * kSpr / 2;
constexpr size_t kRgbSize = kSpr * kSpr * 3;
uint8_t *pool = nullptr, *i4Buf = nullptr, *i4Draw = nullptr, *rgbBuf = nullptr;
lv_image_dsc_t rgbDsc;
const cyd32_creature_t *curSpec = nullptr;
const cyd32_anim_t *anim = nullptr;
lv_timer_t *animTimer = nullptr;
uint16_t animFrame = 0;
bool playingHappy = false, reactReq = false, playingHatch = false;
uint32_t lastUniques = 0, growStart = 0, popStart = 0;
uint16_t growFrom = 256, growTo = 256;
uint8_t popping = 0;
constexpr uint32_t kGrowMs = 560, kPopMs = 560;
// V4: the egg (first hatch frame) sits for 3 s before the hatch plays; the
// baby then grows with unique detections (creature::size()).
constexpr uint32_t kEggIdleMs = 3000;
uint32_t eggHoldUntil = 0;
// Egg: the first hatch frame, held until the first detection (creature_rules.h).
bool eggWait = false;
lv_obj_t *creaToast = nullptr;
uint32_t toastUntil = 0, lastAdv = 0;

float backEase(float t) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  const float s = 1.70158f;
  t -= 1.f;
  return t * t * ((s + 1.f) * t + s) + 1.f;
}
void i4Roll(uint8_t *buf, int8_t dx, int8_t dy) {
  if (dy) {
    uint8_t *tmp = rgbBuf;
    memcpy(tmp, buf, kI4Size);
    for (int y = 0; y < kSpr; y++) {
      const int sy = (y - dy) & (kSpr - 1);
      memcpy(buf + y * 32, tmp + sy * 32, 32);
    }
  }
  if (!dx) return;
  for (int y = 0; y < kSpr; y++) {
    uint8_t *row = buf + y * 32;
    uint8_t pix[kSpr], out[kSpr];
    for (int x = 0; x < kSpr; x++) {
      const uint8_t b = row[x / 2];
      pix[x] = (x & 1) ? (uint8_t)(b & 0x0f) : (uint8_t)(b >> 4);
    }
    for (int x = 0; x < kSpr; x++) out[x] = pix[(x - dx) & (kSpr - 1)];
    for (int x = 0; x < kSpr; x += 2) row[x / 2] = (uint8_t)((out[x] << 4) | out[x + 1]);
  }
}
void i4Blit(uint8_t *dst, int x, int y, int w, int h, const uint8_t *packed, bool skip0) {
  const int sw = (w + 1) / 2;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t v = packed[j * sw + i / 2];
      v = (i & 1) ? (uint8_t)(v & 0x0f) : (uint8_t)(v >> 4);
      if (skip0 && !v) continue;
      const int X = x + i, Y = y + j;
      if ((unsigned)X >= kSpr || (unsigned)Y >= kSpr) continue;
      uint8_t *d = &dst[Y * 32 + X / 2];
      *d = (X & 1) ? (uint8_t)((*d & 0xF0) | v) : (uint8_t)((*d & 0x0F) | (v << 4));
    }
  }
}
size_t deltaSlice(const cyd32_anim_t *a, uint16_t idx) {
  if (!a || !a->off || !a->delta || a->n < 2 || idx >= (uint16_t)(a->n - 1)) return 0;
  const uint32_t start = a->off[idx];
  const uint32_t next = (idx + 1u < (uint16_t)(a->n - 1)) ? (uint32_t)a->off[idx + 1] : a->dlen;
  if (next < start || start > a->dlen || next > a->dlen) return 0;
  return (size_t)(next - start);
}
void applyDelta(const uint8_t *d, size_t cap) {
  if (!d || cap < 3) return;
  const int8_t dx = (int8_t)d[0], dy = (int8_t)d[1];
  const uint8_t nrect = d[2];
  const uint8_t *p = d + 3;
  const uint8_t *end = d + cap;
  i4Roll(i4Buf, dx, dy);
  for (uint8_t r = 0; r < nrect; r++) {
    if (p + 4 > end) return;  // nrect overruns the frame slice
    const uint8_t x = p[0], y = p[1], w = p[2], h = p[3];
    p += 4;
    const size_t nbytes = (size_t)((w + 1) / 2) * (size_t)h;
    if (p + nbytes > end) return;  // pixel payload overruns the frame slice
    // Reject rectangles that would address outside the 64x64 I4 buffer.
    if (w == 0 || h == 0 || x >= kSpr || y >= kSpr ||
        (int)x + (int)w > kSpr || (int)y + (int)h > kSpr) {
      p += nbytes;
      continue;
    }
    i4Blit(i4Buf, x, y, w, h, p, false);
    p += nbytes;
  }
}
void gotoFrame(const cyd32_anim_t *a, uint16_t i) {
  memcpy(i4Buf, a->key, kI4Size);
  if (i >= a->n) i = (uint16_t)(a->n ? a->n - 1 : 0);
  for (uint16_t f = 1; f <= i; f++) applyDelta(a->delta + a->off[f - 1], deltaSlice(a, (uint16_t)(f - 1)));
  animFrame = i;
}
void blitTrait(uint8_t *dst, const cyd32_trait_t *tr, int frame, float scale) {
  if (!tr->px || !tr->anchor || scale <= 0) return;
  const int nw = (int)(tr->w * scale + 0.5f);
  const int nh = (int)(tr->h * scale + 0.5f);
  if (nw < 1 || nh < 1) return;
  const int sw = (tr->w + 1) / 2;
  const int inst = tr->inst ? tr->inst : 1;
  for (int k = 0; k < inst; k++) {
    const int ax = tr->anchor[frame * 4 + k * 2];
    const int ay = tr->anchor[frame * 4 + k * 2 + 1];
    const int px0 = ax + ((int)tr->w - nw) / 2;
    const int py0 = ay + ((int)tr->h - nh) / 2;
    for (int j = 0; j < nh; j++) {
      const int sy = j * tr->h / nh;
      for (int i = 0; i < nw; i++) {
        const int sx = i * tr->w / nw;
        uint8_t v = tr->px[sy * sw + sx / 2];
        v = (sx & 1) ? (uint8_t)(v & 0x0f) : (uint8_t)(v >> 4);
        if (!v) continue;
        const int X = px0 + i, Y = py0 + j;
        if ((unsigned)X >= kSpr || (unsigned)Y >= kSpr) continue;
        uint8_t *d = &dst[Y * 32 + X / 2];
        *d = (X & 1) ? (uint8_t)((*d & 0xF0) | v) : (uint8_t)((*d & 0x0F) | (v << 4));
      }
    }
  }
}
void decodeRgb(const uint8_t *src, const uint32_t *pal) {
  uint16_t *rgb = (uint16_t *)rgbBuf;
  uint8_t *alpha = rgbBuf + kSpr * kSpr * 2;
  for (int y = 0; y < kSpr; y++) {
    for (int x = 0; x < kSpr; x++) {
      const uint8_t b = src[y * 32 + x / 2];
      const uint8_t idx = (x & 1) ? (uint8_t)(b & 0x0f) : (uint8_t)(b >> 4);
      const uint32_t c = pal[idx];
      const int o = y * kSpr + x;
      rgb[o] = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c >> 3) & 0x1F));
      alpha[o] = (uint8_t)(c >> 24);
    }
  }
}
uint16_t currentZoom() {
  const uint8_t s = creature::size();
  const uint16_t target = CYD32_STAGE_ZOOM[s ? s - 1 : 0];
  if (!growStart) return target;
  const uint32_t dt = nowMs - growStart;
  if (dt >= kGrowMs) { growStart = 0; growFrom = growTo = target; return target; }
  const float u = backEase((float)dt / (float)kGrowMs);
  return (uint16_t)(growFrom + (int)((int)growTo - (int)growFrom) * u);
}
void compose() {
  if (!i4Buf || !rgbBuf || !anim || !creaImg) return;
  memcpy(i4Draw, i4Buf, kI4Size);
  const uint8_t bits = creature::unlockedTraits();
  if (bits && curSpec && creature::size() >= 2) {
    const int nAnc = curSpec->adult_idle ? curSpec->adult_idle->n : 12;
    const int fr = nAnc ? (int)(animFrame % nAnc) : 0;
    const float popT = popping && popStart ? backEase((float)(nowMs - popStart) / (float)kPopMs) : 1.f;
    for (int i = 0; i < 8; i++) {
      if (!(bits & (uint8_t)(1u << i))) continue;
      float sc = 1.f;
      if (popping & (uint8_t)(1u << i)) sc = popT;
      blitTrait(i4Draw, &curSpec->traits[i], fr, sc);
    }
  }
  decodeRgb(i4Draw, anim->pal);
  rgbDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  rgbDsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
  rgbDsc.header.w = kSpr;
  rgbDsc.header.h = kSpr;
  rgbDsc.header.stride = kSpr * 2;
  rgbDsc.data_size = kRgbSize;
  rgbDsc.data = rgbBuf;
  lv_image_cache_drop(&rgbDsc);
  lv_image_set_src(creaImg, &rgbDsc);
  uint16_t z = currentZoom();
  uint16_t zy = z;
  const bool idle = anim && !playingHappy && !playingHatch &&
                    (anim == curSpec->baby_idle || anim == curSpec->adult_idle ||
                     anim == curSpec->cth_idle || anim == curSpec->cth_keys);
  if (idle) {
    const float breath = 0.03f * sinf(nowMs * 0.004f);
    zy = (uint16_t)((float)z * (1.f + breath) + 0.5f);
    if (zy < 1) zy = 1;
  }
  lv_image_set_pivot(creaImg, kSpr / 2, kSpr);
  lv_image_set_antialias(creaImg, false);
  uint16_t fx, fy;  // FEED chomp: squash and stretch
  feed::creatureScale(fx, fy);
  lv_image_set_scale_x(creaImg, (uint16_t)((uint32_t)z * fx / 256u));
  lv_image_set_scale_y(creaImg, (uint16_t)((uint32_t)zy * fy / 256u));
  lv_obj_invalidate(creaImg);
}
void setAnim(const cyd32_anim_t *a, uint16_t frame = 0) {
  if (!i4Buf || !a) return;
  anim = a;
  gotoFrame(a, frame);
}
void startIdle() {
  playingHappy = false;
  playingHatch = false;
  if (!curSpec) return;
  const uint8_t cth = creature::cthulhuStage();
  if (cth >= 5) setAnim(curSpec->cth_idle, 0);
  else if (cth >= 1) setAnim(curSpec->cth_keys, (uint16_t)(2 * cth - 1));
  else if (creature::size() <= 1) setAnim(curSpec->baby_idle, 0);
  else setAnim(curSpec->adult_idle, 0);
}
void startHappy() {
  if (!curSpec || playingHatch) return;
  if (creature::cthulhuStage() >= 1) return;  // Cthulhu keys/idle have no happy clip
  playingHappy = true;
  setAnim(creature::size() <= 1 ? curSpec->baby_happy : curSpec->adult_happy, 0);
}
void animTick(lv_timer_t *t) {
  nowMs = lv_tick_get();
  if (popping && popStart && nowMs - popStart >= kPopMs) { popping = 0; popStart = 0; }
  const bool eggHold = playingHatch && (eggWait || (int32_t)(nowMs - eggHoldUntil) < 0);
  // Frames advance at the clip's own rate; the timer runs faster (40 ms)
  // only while FEED plays, for a smooth squash and stretch.
  const uint32_t frameMs = anim && anim->ms ? anim->ms : 50;
  const bool advance = !feed::active() || nowMs - lastAdv >= frameMs;
  if (advance) lastAdv = nowMs;
  if (advance && i4Buf && anim && curSpec && anim->ms && anim != curSpec->cth_keys && !eggHold) {
    if ((uint16_t)(animFrame + 1) < anim->n) {
      applyDelta(anim->delta + anim->off[animFrame], deltaSlice(anim, animFrame));
      animFrame++;
    } else if (playingHatch || playingHappy) {
      startIdle();
    } else {
      memcpy(i4Buf, anim->key, kI4Size);
      animFrame = 0;
    }
  }
  if (feed::consumeChomp()) { creature::addXp(2); reactReq = true; }
  if (reactReq && !playingHatch && !playingHappy) { reactReq = false; startHappy(); }
  if (creaToast && toastUntil && (int32_t)(nowMs - toastUntil) >= 0) { toastUntil = 0; lv_label_set_text(creaToast, ""); }
  compose();
  lv_timer_set_period(t, feed::active() && frameMs > 40 ? 40 : frameMs);
}
// Creature layout (content coordinates): the sprite stands on a pivot at
// (84, 168), so the 120 px stage 4 fits above it; text, bar and buttons in a
// column from x 168.
constexpr int kCreaPx = 84, kCreaPy = 168;
// PET: forget the 25 oldest devices of the 3-minute window (the creature
// shrinks at once, never back to an egg) and +1 XP.
void onPet(lv_event_t *) {
  const int k = uic::petForget();
  creature::addXp(1);
  reactReq = true;
  if (creaToast) {
    char t[24];
    snprintf(t, sizeof t, "FORGOT %d", k);
    lv_label_set_text(creaToast, t);
    toastUntil = lv_tick_get() + 1500;
  }
  ui::refresh();
}
// FEED: the home-made drone flies into the mouth; +2 XP on the chomp.
void onFeed(lv_event_t *) {
  const int h = 64 * currentZoom() / 256;
  feed::Spec f{};
  f.x0 = 300; f.y0 = 12;
  f.mouthX = kCreaPx; f.mouthY = kCreaPy - h * 55 / 100;
  f.arm = 9;
  f.frame = CYD32_COL_CYAN; f.rotor = CYD32_COL_WHITE; f.crumb = CYD32_COL_AMBER; f.text = CYD32_COL_OK;
  f.font = &lv_font_montserrat_14;
  feed::start(content, f);
}
void buildCreature() {
  curSpec = creature::spec();
  creaImg = lv_image_create(content);
  lv_obj_set_pos(creaImg, kCreaPx - kSpr / 2, kCreaPy - kSpr);
  if (!pool) {
    pool = (uint8_t *)malloc(kI4Size * 2 + kRgbSize);
    if (pool) { i4Buf = pool; i4Draw = pool + kI4Size; rgbBuf = pool + kI4Size * 2; }
  }
  memset(&rgbDsc, 0, sizeof rgbDsc);
  playingHappy = false;
  reactReq = false;
  eggWait = !creature::hatched();
  if (eggWait || creature::consumeHatched()) {
    playingHatch = true;
    setAnim(curSpec->hatch, 0);
    eggHoldUntil = lv_tick_get() + kEggIdleMs;
  } else {
    startIdle();
  }
  compose();
  creaInfo = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(creaInfo, 168, 6);
  creaBar = lv_bar_create(content);
  lv_obj_set_size(creaBar, 146, 8); lv_obj_set_pos(creaBar, 168, 100);
  lv_obj_set_style_bg_color(creaBar, C(CYD32_COL_GRID), 0);
  lv_obj_set_style_bg_color(creaBar, C(CYD32_COL_CYAN), LV_PART_INDICATOR);
  button(content, "PET", 168, 116, 70, 36, onPet, nullptr, CYD32_COL_CYAN);
  button(content, "FEED", 244, 116, 70, 36, onFeed, nullptr, CYD32_COL_CYAN);
  creaToast = label(content, "", &lv_font_unscii_8, CYD32_COL_AMBER);
  lv_obj_set_pos(creaToast, 168, 160);
  toastUntil = 0;
  flash = box(content, 0, 0, CYD32_CONTENT_W, kContentH, CYD32_COL_WHITE);
  lv_obj_add_flag(flash, LV_OBJ_FLAG_HIDDEN);
  lastUniques = creature::uniqueCount();
  growFrom = growTo = currentZoom();
  animTimer = lv_timer_create(animTick, 80, nullptr);
}
void refreshCreature() {
  const uint8_t s = creature::size();
  const uint8_t cth = creature::cthulhuStage();
  const uint32_t next = creature::nextSizeAt();
  const uint32_t near3 = creature::recentCount();
  const uint32_t uniq = creature::uniqueCount();
  const uint32_t drones = creature::droneCount();
  char traits[48] = "";
  const char *tn[8] = {"TRK", "AP", "HOT", "OPEN", "2.4", "5", "6", "7"};
  const uint8_t bits = creature::unlockedTraits();
  for (int i = 0; i < 8; i++) {
    if (!(bits & (uint8_t)(1u << i))) continue;
    if (traits[0]) strncat(traits, " ", sizeof traits - strlen(traits) - 1);
    strncat(traits, tn[i], sizeof traits - strlen(traits) - 1);
  }
  if (!traits[0]) strncpy(traits, "-", sizeof traits - 1);
  char t[192];
  if (s == 0)
    snprintf(t, sizeof t, "%s\nEGG\nhatches at\nthe first\ndevice\n\n3 min %lu", creature::name(), (unsigned long)near3);
  else
    snprintf(t, sizeof t, "%s\nsize %u/4\n3 min %lu\nnext %s%lu\nRID  %lu\nCth  %u\nXP   %lu\n%s",
             creature::name(), s, (unsigned long)near3, next ? "" : "max ", (unsigned long)(next ? next : near3),
             (unsigned long)drones, cth, (unsigned long)creature::xp(), traits);
  lv_label_set_text(creaInfo, t);
  // Bar: progress inside the current size band of the 3-minute count.
  const uint32_t lo = creature::sizeFloor();
  int pct = next ? (int)(100 * ((int)near3 - (int)lo) / (int)(next - lo)) : 100;
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  lv_bar_set_value(creaBar, pct, LV_ANIM_OFF);
  if (eggWait && creature::hatched()) {  // the egg hatches on screen
    eggWait = false;
    creature::consumeHatched();
    eggHoldUntil = nowMs;
  }
  if (uniq > lastUniques) { lastUniques = uniq; reactReq = true; }
  if (creature::consumeSizeChanged()) {  // grows or shrinks with the 3-minute window
    growFrom = growTo;
    growTo = CYD32_STAGE_ZOOM[s ? s - 1 : 0];
    growStart = nowMs ? nowMs : lv_tick_get();
    if (!playingHatch) startIdle();
  }
  if (creature::consumeCthulhuUp() && !playingHatch) startIdle();
  const uint8_t fresh = creature::consumeNewTraits();
  if (fresh) { popping = fresh; popStart = nowMs ? nowMs : lv_tick_get(); }
}

// MENU: the same 6 entries as the StopWatch (ui_common.cpp), 2 x 3 buttons.
// BEEP toggles the proximity beep in place and shows its state.
lv_obj_t *beepBtnLbl = nullptr;
void setBeepLabel() {
  if (!beepBtnLbl) return;
  lv_label_set_text(beepBtnLbl, beep::enabled() ? "BEEP ON" : "BEEP OFF");
  lv_obj_set_style_text_color(beepBtnLbl, C(beep::enabled() ? CYD32_SONAR_G5 : CYD32_SONAR_G4), 0);
}
void onMenu(lv_event_t *e) {
  const ui::Screen s = (ui::Screen)(intptr_t)lv_event_get_user_data(e);
  if (s == ui::BEEP) { beep::toggle(); setBeepLabel(); return; }
  ui::show(s);
}
void buildMenu() {
  beepBtnLbl = nullptr;
  for (int i = 0; i < uic::kMenuN; i++) {
    lv_obj_t *b = button(content, uic::kMenuTxt[i], 8 + (i % 2) * 156, 6 + (i / 2) * 58, 148, 52, onMenu,
                         (void *)(intptr_t)uic::kMenuScr[i], CYD32_SONAR_G4);
    lv_obj_set_style_bg_color(b, C(CYD32_SONAR_BG_CYD), 0);
    lv_obj_set_style_border_color(b, C(CYD32_SONAR_G2), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &lv_font_montserrat_16, 0);
    if (uic::kMenuScr[i] == ui::BEEP) beepBtnLbl = lv_obj_get_child(b, 0);
  }
  setBeepLabel();
}
void onBack(lv_event_t *) { ui::show(prevSc); }
void onTrack(lv_event_t *) {
  if (haveSel) { trackedId = selEnt.id; hasTracked = true; }
  ui::show(ui::SONAR);
}
void onDetNext(lv_event_t *) {
  det::Entry *e = gSnap;
  if (!e) return;
  const int n = det::snapshot(e, det::kMax);
  const int i = uic::nextClockwise(e, n, selEnt.id, haveSel);
  if (i >= 0) selectEntry(e[i]);
  ui::refresh();
}
// DEVICE DETAIL: one device (from the sonar or a dashboard list);
// NEXT steps clockwise by indicative angle, TRACK pins it on the sonar.
void buildDetails() {
  detHit = label(content, "", &lv_font_montserrat_16, CYD32_SONAR_G4);
  lv_obj_set_pos(detHit, 8, 4);
  detBody = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(detBody, 8, 30);
  lv_obj_set_width(detBody, 304);
  button(content, "BACK", 8, 140, 96, 36, onBack, nullptr, CYD32_SONAR_G4);
  button(content, "NEXT", 112, 140, 96, 36, onDetNext, nullptr, CYD32_SONAR_G4);
  button(content, "TRACK", 216, 140, 96, 36, onTrack, nullptr, CYD32_SONAR_G4);
}
void refreshDetails() {
  det::Entry *e = gSnap;
  const int n = e ? det::snapshot(e, det::kMax) : 0;
  if (!haveSel) {  // nothing picked yet: the strongest device
    int b = -1;
    for (int i = 0; i < n; i++) if (b < 0 || det::smoothed(e[i]) > det::smoothed(e[b])) b = i;
    if (b >= 0) selectEntry(e[b]);
  } else {
    for (int i = 0; i < n; i++) if (e[i].id == selEnt.id && e[i].kind == selEnt.kind) { selEnt = e[i]; break; }
  }
  if (!haveSel) {
    lv_label_set_text(detHit, "DEVICE DETAIL");
    lv_label_set_text(detBody, "Nothing seen yet.");
    return;
  }
  const bool drone = selEnt.kind == det::DRONE;
  const int sm = det::smoothed(selEnt);
  char t[320], id[9], tags[32], ch[12] = "";
  snprintf(t, sizeof t, "%s  %s", drone ? "HIT! DRONE" : uic::kindName(selEnt.kind), rssi::zone(sm));
  lv_label_set_text(detHit, t);
  lv_obj_set_style_text_color(detHit, C(drone ? CYD32_COL_DRONE : CYD32_SONAR_G4), 0);
  privacy::idToStr(selEnt.id, id);
  uic::badgeStr(selEnt.badges, tags, sizeof tags, true);
  if (selEnt.kind == det::WIFI) snprintf(ch, sizeof ch, "CH %u", selEnt.channel);
  snprintf(t, sizeof t,
           "RADIO  %s %s\nRSSI   %d dBm  (avg %d)\nANGLE  %03u  (indicative, not a\n       direction: hash of the address)\n"
           "TAGS   %s\nID     %s  (salted per boot)\nSEEN   %lu s ago",
           selEnt.kind == det::DRONE ? "REMOTE ID" : uic::kindName(selEnt.kind), ch, selEnt.rssi, sm,
           (unsigned)selEnt.bearing, tags[0] ? tags : "-", id,
           (unsigned long)((nowMs - selEnt.lastSeen) / 1000));
  lv_label_set_text(detBody, t);
}

// COUNTERS (DASH tab): compact counters + ABOUT, then a selector bar
// WIFI | BLE | RID | TAGGED | LOG showing ONE list at a time with full rows,
// strongest first (LOG: newest first). Tap a row: DEVICE DETAIL.
uic::View dashView = uic::V_WIFI;
lv_obj_t *dashSel[uic::V_COUNT];
det::LogEvt listLog[kListRows];
void onListRow(lv_event_t *e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i >= listN) return;
  if (dashView == uic::V_LOG) {  // the log keeps the id only: open it if it is still in the table
    det::Entry *en = gSnap;
    const int n = en ? det::snapshot(en, det::kMax) : 0;
    for (int k = 0; k < n; k++)
      if (en[k].id == listLog[i].id) { selectEntry(en[k]); ui::show(ui::DETAILS); return; }
    return;
  }
  selectEntry(listEnt[i]);
  ui::show(ui::DETAILS);
}
void onDashView(lv_event_t *e) {
  dashView = (uic::View)(intptr_t)lv_event_get_user_data(e);
  ui::refresh();
}
// Keep the n strongest entries matching `want`, strongest first.
template <typename F>
int pickStrongest(det::Entry *out, int max, F want) {
  det::Entry *e = gSnap;
  if (!e) return 0;
  const int n = det::snapshot(e, det::kMax);
  int k = 0;
  for (int i = 0; i < n; i++) {
    if (!want(e[i])) continue;
    int pos = k;
    while (pos > 0 && det::smoothed(out[pos - 1]) < det::smoothed(e[i])) pos--;
    if (pos >= max) continue;
    for (int j = LV_MIN(k, max - 1); j > pos; j--) out[j] = out[j - 1];
    out[pos] = e[i];
    if (k < max) k++;
  }
  return k;
}
void rowText(const det::Entry &x, char *t, size_t cap) {
  char tags[24], id[9], ch[8] = "";
  uic::badgeStr(x.badges, tags, sizeof tags, true);
  privacy::idToStr(x.id, id);
  id[6] = 0;
  if (x.kind == det::WIFI) snprintf(ch, sizeof ch, "ch%u", x.channel);
  snprintf(t, cap, "%s %-5s %4d dBm %-4s %s", id, x.kind == det::DRONE ? "RID" : uic::kindName(x.kind), det::smoothed(x), ch, tags);
}
void buildCounters() {
  dashTop = label(content, "", &lv_font_unscii_8, CYD32_SONAR_G4);
  lv_obj_set_pos(dashTop, 4, 3);
  lv_obj_set_width(dashTop, 252);
  lv_obj_t *ab = button(content, "ABOUT", 262, 2, 54, 20, onShow, (void *)(intptr_t)ui::ABOUT, CYD32_SONAR_G3);
  lv_obj_set_style_text_font(lv_obj_get_child(ab, 0), &lv_font_unscii_8, 0);
  for (int i = 0; i < uic::V_COUNT; i++) {
    dashSel[i] = button(content, uic::kViewTxt[i], 3 + i * 63, 26, 61, 20, onDashView, (void *)(intptr_t)i, CYD32_SONAR_G4);
    lv_obj_set_style_text_font(lv_obj_get_child(dashSel[i], 0), &lv_font_unscii_8, 0);
  }
  for (int i = 0; i < kListRows; i++) {
    listRows[i] = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
    lv_obj_set_pos(listRows[i], 4, 52 + i * 14);
    lv_obj_set_size(listRows[i], 312, 13);
    lv_label_set_long_mode(listRows[i], LV_LABEL_LONG_CLIP);
    lv_obj_add_flag(listRows[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(listRows[i], onListRow, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
  listEmpty = label(content, "", &lv_font_montserrat_14, CYD32_SONAR_G3);
  lv_obj_align(listEmpty, LV_ALIGN_TOP_MID, 0, 96);
}
void refreshCounters() {
  const det::Counts c = det::counts();
  char t[96];
  snprintf(t, sizeof t, "W%u B%u RID%u TRK%u META%u FLK%u\n3 MIN %u   24 H %u",
           (unsigned)c.wifi, (unsigned)c.ble, (unsigned)c.drone, (unsigned)c.trk, (unsigned)c.meta, (unsigned)c.flock,
           (unsigned)c.recent3m, (unsigned)c.unique24h);
  lv_label_set_text(dashTop, t);
  for (int i = 0; i < uic::V_COUNT; i++) {
    const bool on = i == dashView;
    lv_obj_set_style_bg_color(dashSel[i], C(on ? CYD32_SONAR_G4 : CYD32_SONAR_BG_CYD), 0);
    lv_obj_set_style_border_color(dashSel[i], C(on ? CYD32_SONAR_G4 : CYD32_SONAR_G2), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(dashSel[i], 0), C(on ? CYD32_SONAR_BG_CYD : CYD32_SONAR_G3), 0);
  }
  if (dashView == uic::V_LOG) {
    listN = det::logSnapshot(listLog, kListRows);
    for (int i = 0; i < kListRows; i++) {
      char l[56] = "", tm[8], id[9], tags[24];
      if (i < listN) {
        uic::mmss(listLog[i].ms, tm);
        privacy::idToStr(listLog[i].id, id);
        id[6] = 0;
        uic::badgeStr(listLog[i].badges, tags, sizeof tags);
        snprintf(l, sizeof l, "%s %s %-5s %4d dBm %s", tm, id, listLog[i].kind == det::DRONE ? "RID" : uic::kindName((det::Kind)listLog[i].kind),
                 listLog[i].rssi, tags);
      }
      lv_label_set_text(listRows[i], l);
      lv_obj_set_style_text_color(listRows[i], C(i < listN && listLog[i].kind == det::DRONE ? CYD32_COL_DRONE : CYD32_COL_WHITE), 0);
    }
    lv_label_set_text(listEmpty, listN ? "" : "No drone or tagged device yet");
    return;
  }
  const uic::View v = dashView;
  listN = pickStrongest(listEnt, kListRows, [v](const det::Entry &x) { return uic::inView(x, v); });
  for (int i = 0; i < kListRows; i++) {
    char l[64] = "";
    if (i < listN) rowText(listEnt[i], l, sizeof l);
    lv_label_set_text(listRows[i], l);
    lv_obj_set_style_text_color(listRows[i], C(i < listN && listEnt[i].kind == det::DRONE ? CYD32_COL_DRONE : CYD32_COL_WHITE), 0);
  }
  static const char *const kEmpty[uic::V_COUNT] = {"No Wi-Fi device", "No BLE device", "No drone in range",
                                                   "No tracker, Meta or Flock device", ""};
  lv_label_set_text(listEmpty, listN ? "" : kEmpty[v]);
}
void buildStub(const char *title, const char *what) {
  lv_obj_t *t = label(content, title, &lv_font_montserrat_16, CYD32_COL_AMBER);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_t *s = label(content, what, &lv_font_montserrat_12, CYD32_COL_WHITE);
  lv_obj_set_width(s, 280); lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_t *p = label(content, "Coming in phase 3", &lv_font_montserrat_14, CYD32_COL_CYAN);
  lv_obj_align(p, LV_ALIGN_BOTTOM_MID, 0, -20);
}
void onStartUpdate(lv_event_t *) { updateReq = true; }
void buildUpdateConfirm() {
  lv_obj_t *s = label(content, "Update mode stops scanning and opens a\ntemporary WPA2 hotspot for 10 minutes.\n\n1. Scan the QR code with the iPhone\n   camera to join the hotspot.\n2. Open http://192.168.4.1 in Safari.\n3. Upload the signed .bin file.", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(s, 6, 10);
  button(content, "Start update mode", 60, 110, 200, 44, onStartUpdate, nullptr, CYD32_COL_OK);
}
void buildOta() {
  otaConfirm = nullptr;
  otaQr = lv_qrcode_create(content);
  lv_qrcode_set_size(otaQr, 140);
  lv_qrcode_set_dark_color(otaQr, lv_color_black());
  lv_qrcode_set_light_color(otaQr, lv_color_white());
  lv_obj_set_pos(otaQr, 8, 8);
  lv_obj_set_style_border_color(otaQr, lv_color_white(), 0);
  lv_obj_set_style_border_width(otaQr, 4, 0);
  const ota_portal::State &st = ota_portal::state();
  lv_qrcode_update(otaQr, st.qr, strlen(st.qr));
  otaInfo = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(otaInfo, 160, 8);
  otaBar = lv_bar_create(content);
  lv_obj_set_size(otaBar, 150, 10); lv_obj_set_pos(otaBar, 160, 128);
  lv_obj_set_style_bg_color(otaBar, C(CYD32_COL_GRID), 0);
  lv_obj_set_style_bg_color(otaBar, C(CYD32_COL_AMBER), LV_PART_INDICATOR);
  otaMsg = label(content, "", &lv_font_unscii_8, CYD32_COL_CYAN);
  lv_obj_set_pos(otaMsg, 160, 146); lv_obj_set_width(otaMsg, 158);
}
void onDowngrade(lv_event_t *e) { ota_portal::confirmDowngrade(lv_event_get_user_data(e) != nullptr); }
void refreshOta() {
  const ota_portal::State &st = ota_portal::state();
  const bool ask = st.phase == ota_portal::AWAIT_CONFIRM;
  if (ask && !otaConfirm) {
    otaConfirm = box(content, 0, 0, CYD32_CONTENT_W, CYD32_SCREEN_H - CYD32_HEADER_Y - CYD32_HEADER_H, CYD32_COL_BG);
    lv_obj_t *t = label(otaConfirm, "DOWNGRADE", &lv_font_montserrat_24, CYD32_COL_HIT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_t *m = label(otaConfirm, st.msg, &lv_font_unscii_8, CYD32_COL_WHITE);
    lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_t *w = label(otaConfirm, "The uploaded firmware is OLDER than\nthe running one (or has no version).\nInstall it anyway?", &lv_font_unscii_8, CYD32_COL_AMBER);
    lv_obj_align(w, LV_ALIGN_TOP_MID, 0, 80);
    button(otaConfirm, "Cancel", 10, 150, 140, 44, onDowngrade, nullptr, CYD32_COL_OK);
    button(otaConfirm, "Install", 170, 150, 140, 44, onDowngrade, (void *)1, CYD32_COL_HIT);
  } else if (!ask && otaConfirm) {
    lv_obj_delete(otaConfirm); otaConfirm = nullptr;
  }
  char t[200];
  snprintf(t, sizeof t, "UPDATE MODE\n\nSSID %s\nPASS %s\n\nhttp://192.168.4.1\n\n%lu:%02lu left\n%s\n%lu KB",
           st.ssid, st.pass, (unsigned long)st.remainingS / 60, (unsigned long)st.remainingS % 60,
           st.unsignedArmed ? "UNSIGNED ARMED" : "signed only", (unsigned long)st.received / 1024);
  lv_label_set_text(otaInfo, t);
  lv_bar_set_value(otaBar, st.expected ? (int)(100.0 * st.received / st.expected) : 0, LV_ANIM_OFF);
  lv_label_set_text(otaMsg, st.msg);
  lv_obj_set_style_text_color(otaMsg, C(st.phase == ota_portal::FAILED ? CYD32_COL_HIT : st.phase == ota_portal::SUCCESS ? CYD32_COL_OK : CYD32_COL_CYAN), 0);
}
lv_obj_t *aboutWifi = nullptr;
void buildAbout() {
  char t[400];
  snprintf(t, sizeof t,
           "CYD32 - passive RF sonar (receive only)\nVersion  %s\nCommit   %s\nLicence  %s\nTarget   %s\nStatus   %s\nKey      %s\n\n\n\n\nSource code:\n%s\n"
           "ABSOLUTELY NO WARRANTY. Free software:\nredistribute it under the AGPL-3.0;\nget the source at the URL above.",
           CYD32_VERSION, CYD32_COMMIT, CYD32_LICENSE, CYD32_TARGET, fw_status::kindStr(), fw_status::keyFingerprint(), CYD32_SOURCE_URL);
  lv_obj_t *l = label(content, t, &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(l, 2, 2);
  aboutWifi = label(content, "", &lv_font_unscii_8, CYD32_SONAR_G4);
  lv_obj_set_pos(aboutWifi, 2, 2 + 8 * 8);
}
// Wi-Fi bring-up status (scanner::wifiDiag): OK, or the real esp_err_t, the
// failing step, the attempt count and the next 30 s retry; RAM at the last try.
void refreshAbout() {
  if (!aboutWifi) return;
  const scanner::WifiDiag d = scanner::wifiDiag();
  char t[120];
  if (d.ready)
    snprintf(t, sizeof t, "WiFi     OK (bring-up #%u)\nRAM      int %uK max %uK dma %uK", (unsigned)d.attempts,
             (unsigned)(d.freeInt / 1024), (unsigned)(d.largestInt / 1024), (unsigned)(d.largestDma / 1024));
  else
    snprintf(t, sizeof t, "WiFi     ERR 0x%x %s #%u retry %us\n         %s\nRAM      int %uK max %uK dma %uK", (unsigned)d.err,
             d.step[0] ? d.step : "-", (unsigned)d.attempts, (unsigned)((d.retryInMs + 999) / 1000), d.errName,
             (unsigned)(d.freeInt / 1024), (unsigned)(d.largestInt / 1024), (unsigned)(d.largestDma / 1024));
  lv_label_set_text(aboutWifi, t);
  lv_obj_set_style_text_color(aboutWifi, C(d.ready ? CYD32_SONAR_G4 : CYD32_COL_HIT), 0);
}
void onUnsigned(lv_event_t *e) { unsignedDecision = (int)(intptr_t)lv_event_get_user_data(e); }
void buildUnsignedConfirm() {
  lv_obj_t *s = label(content, CYD32_UNSIGNED_BODY, &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(s, 8, 8);
  button(content, "Cancel", 10, 120, 140, 44, onUnsigned, (void *)0, CYD32_COL_OK);
  button(content, "Arm unsigned", 170, 120, 140, 44, onUnsigned, (void *)1, CYD32_COL_HIT);
}
void refreshHeader() {
  const det::Counts c = det::counts();
  char t[8];
  // "W!" instead of the Wi-Fi count when the Wi-Fi scanner failed to start.
  const bool wFail = scanner::wifiFailed();
  if (wFail) snprintf(t, sizeof t, "!");
  else snprintf(t, sizeof t, "%04u", (unsigned)LV_MIN(c.wifi, 9999));
  lv_label_set_text(hdr[2], t);
  snprintf(t, sizeof t, "%04u", (unsigned)LV_MIN(c.ble, 9999));
  lv_label_set_text(hdr[4], t);
  snprintf(t, sizeof t, "%04u", (unsigned)LV_MIN(c.drone, 9999));
  lv_label_set_text(hdr[6], t);
  // Phosphor: CYD32 G4, letters G3, digits G4, R digits DRONE. Y3-d: amber.
  const uint16_t letter = phosphor ? CYD32_SONAR_G3 : CYD32_COL_AMBER, digit = phosphor ? CYD32_SONAR_G4 : CYD32_COL_AMBER;
  const uint16_t wDigit = wFail ? (uint16_t)CYD32_COL_HIT : digit;
  const uint16_t rDigit = CYD32_COL_DRONE;  // drones: true yellow #FCEE0A on every screen
  const uint16_t cols[7] = {digit, letter, wDigit, letter, digit, letter, rDigit};
  for (int i = 0; i < 7; i++) lv_obj_set_style_text_color(hdr[i], C(cols[i]), 0);
  lv_label_set_text(statusLbl, fw_status::kindStr());
  if (unsignedBanner || fw_status::kind() == fw_status::UNSIGNED) lv_obj_remove_flag(banner, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

namespace ui {
void begin() {
  if (!gSnap) gSnap = (det::Entry *)malloc(sizeof(det::Entry) * det::kMax);
  if (!sonarEnt) sonarEnt = (det::Entry *)malloc(sizeof(det::Entry) * kSonarMax);
  if (!sonarRc) sonarRc = (radar::Contact *)malloc(sizeof(radar::Contact) * kSonarMax);
  buildChrome();
  show(BOOT);
}
void show(Screen s) {
  if (cur != s && cur != BOOT && cur != DETAILS) prevSc = cur;
  feed::stop();  // its overlay lives in content
  if (fxTimer) { lv_timer_delete(fxTimer); fxTimer = nullptr; }
  if (sonarTimer) { lv_timer_delete(sonarTimer); sonarTimer = nullptr; }
  sonarN = 0;
  if (stage) { lv_obj_delete(stage); stage = nullptr; }
  radarObj = bootBar = bootPct = bootLogo = bootTitle = scanLine = nullptr;
  beepBtnLbl = aboutWifi = creaToast = nullptr;
  lv_obj_clean(content);
  if (animTimer) { lv_timer_delete(animTimer); animTimer = nullptr; }
  if (pool) { lv_image_cache_drop(&rgbDsc); free(pool); pool = i4Buf = i4Draw = rgbBuf = nullptr; }
  anim = nullptr; curSpec = nullptr; playingHappy = playingHatch = false;
  cur = s;
  phosphor = s == SONAR;
  const bool tabs = s != BOOT && s != OTA && s != UNSIGNED_CONFIRM;
  if (tabs) lv_obj_remove_flag(tabbar, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(tabbar, LV_OBJ_FLAG_HIDDEN);
  // The v9 boot screen is full screen (no header strip).
  if (s == BOOT) lv_obj_add_flag(header, LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(header, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_height(content, tabs ? kContentH : CYD32_SCREEN_H - CYD32_HEADER_Y - CYD32_HEADER_H);
  styleChrome();
  switch (s) {
    case BOOT: buildBoot(); break;
    case SONAR: buildSonar(); break;
    case CREATURE: buildCreature(); break;
    case MENU: buildMenu(); break;
    case DETAILS: buildDetails(); break;
    case BEEP: break;  // MENU action, not a screen
    case COUNTERS: buildCounters(); break;
    case EVOLUTION: buildCreature(); break;
    case WARDRIVING: buildStub("WARDRIVE", "On request only: export to the local microSD card, hashed MACs, never uploaded anywhere."); break;
    case UPDATE_CONFIRM: buildUpdateConfirm(); break;
    case OTA: buildOta(); break;
    case ABOUT: buildAbout(); break;
    case UNSIGNED_CONFIRM: buildUnsignedConfirm(); break;
  }
  refresh();
}
void refresh() {
  nowMs = lv_tick_get();
  refreshHeader();
  switch (cur) {
    case SONAR: refreshSonar(); break;
    case CREATURE: refreshCreature(); break;
    case DETAILS: refreshDetails(); break;
    case COUNTERS: refreshCounters(); break;
    case ABOUT: refreshAbout(); break;
    case OTA: refreshOta(); break;
    default: break;
  }
}
bool takeUpdateRequest() { bool r = updateReq; updateReq = false; return r; }
bool pinnedTarget(uint32_t &id) { id = trackedId; return hasTracked; }
int takeUnsignedDecision() { int d = unsignedDecision; unsignedDecision = -1; return d; }
void setUnsignedBanner(bool on) { unsignedBanner = on; refreshHeader(); }
void setBootProgress(int pct, const char *msg) {
  if (cur != BOOT || !bootBar) return;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  lv_bar_set_value(bootBar, pct, LV_ANIM_OFF);
  char t[8];
  snprintf(t, sizeof t, "%d%%", pct);
  lv_label_set_text(bootPct, t);
  // Light BLE / WIFI / RID as the self-test advances ("BLE OK", "WIFI OK", "RID OK").
  const int step = msg && msg[0] == 'W' ? 1 : msg && msg[0] == 'R' ? 2 : msg && msg[0] == 'B' ? 0 : -1;
  static const char *ok[3] = {"BLE OK", "WIFI OK", "RID OK"};
  for (int i = 0; i < 3; i++) {
    if (i > step) continue;
    lv_label_set_text(bootStat[i], ok[i]);
    lv_obj_set_style_text_color(bootStat[i], C(CYD32_SONAR_G4), 0);
  }
}
}
#endif
