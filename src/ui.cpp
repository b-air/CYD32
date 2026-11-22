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
lv_obj_t *hudTgt, *hudDrn, *hudSel[3];
lv_obj_t *hdr[7], *tabBtn[4];
bool phosphor = false;
lv_obj_t *gridMarks[80], *gridLog;
lv_obj_t *creaImg, *creaInfo, *creaBar, *flash;
lv_obj_t *detRows[14], *detHit, *detBody;
// Selected device (sonar tap, grid tap, list row) for DETAILS; tracked = pinned on the sonar.
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
lv_obj_t *listRows[kListRows], *listEmpty, *dashTop, *dashW[5], *dashB[5], *dashLog[3], *dashChip[4];
det::Entry listEnt[kListRows];
int listN = 0;
lv_obj_t *otaQr, *otaInfo, *otaBar, *otaMsg, *otaConfirm;
// easter egg
const char *kEgg[4] = {"A1", "B2", "C3", "D4"};
int eggPos = 0; uint32_t eggStart = 0; bool eggDone = false;
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
  const char *tabs[4] = {"SONAR", "GRID", "CREA", "MENU"};
  const ui::Screen ts[4] = {ui::SONAR, ui::GRID, ui::CREATURE, ui::MENU};
  for (int i = 0; i < 4; i++) {
    tabBtn[i] = button(tabbar, tabs[i], i * CYD32_TAB_BTN_W, 0, CYD32_TAB_BTN_W, CYD32_TAB_H, onShow, (void *)(intptr_t)ts[i]);
    lv_obj_add_event_cb(tabBtn[i], onTabChamfer, LV_EVENT_DRAW_POST, nullptr);
  }
}

// Theme for the header and the tab buttons: v9 phosphor on the sonar screen,
// Y3-d (yellow HUD, harmonised greens) everywhere else.
void styleChrome() {
  const int active = cur == ui::SONAR ? 0 : cur == ui::GRID ? 1 : cur == ui::CREATURE ? 2 : cur == ui::MENU ? 3 : -1;
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
    s.colPitchX10 = CYD_SONAR_COL_PITCH_X10; s.rowPitch = CYD_SONAR_ROW_PITCH;
    s.lettersFont = &lv_font_unscii_8;
    s.lettersDy = 3;
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
// Tap on the scope: the contact under the finger (else the selected one) opens DETAILS.
void onScopeTap(lv_event_t *ev) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
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
  lv_obj_add_event_cb(radarObj, onScopeTap, LV_EVENT_CLICKED, nullptr);
  stageLabel("TGT", &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_LEFT, 5, 4);
  hudTgt = stageLabel("00", &lv_font_unscii_16, CYD32_SONAR_G4, LV_ALIGN_TOP_LEFT, 5, 13);
  stageLabel("DRN", &lv_font_unscii_8, CYD32_SONAR_G3, LV_ALIGN_TOP_RIGHT, -5, 4);
  hudDrn = stageLabel("00", &lv_font_unscii_16, CYD32_SONAR_DRONE, LV_ALIGN_TOP_RIGHT, -5, 13);
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
  sonarN = det::snapshot(sonarEnt, kSonarMax, true);
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
      uic::cellName(sonarEnt[i].cell, sonarRc[i].id);
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
  if (sonarSel >= 0) {
    const det::Entry &e = sonarEnt[sonarSel];
    const bool drone = e.kind == det::DRONE;
    const int sm = det::smoothed(e);
    char cell[3];
    uic::cellName(e.cell, cell);
    snprintf(t, sizeof t, "SEL %s%s", cell, sm >= rssi::kCloseDbm ? " CLOSE" : "");
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

constexpr int kCw = 24, kCh = 17, kGx = 22, kGy = 12;
void eggStep(const char *cell) {
  if (eggDone) return;
  if (eggPos > 0 && nowMs - eggStart > 3000) eggPos = 0;
  if (strcmp(cell, kEgg[eggPos]) == 0) {
    if (eggPos == 0) eggStart = nowMs;
    if (++eggPos == 4) { eggDone = true; creature::addXp(50); ui::show(ui::TORPEDO); }
  } else {
    eggPos = strcmp(cell, kEgg[0]) == 0 ? 1 : 0;
    if (eggPos) eggStart = nowMs;
  }
}
// Tap a cell: easter-egg step; an occupied cell opens that device's DETAILS.
void onGridTouch(lv_event_t *e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t a; lv_obj_get_coords((lv_obj_t *)lv_event_get_target(e), &a);
  const int c = (p.x - a.x1) / kCw, r = (p.y - a.y1) / kCh;
  if (c < 0 || c > 9 || r < 0 || r > 7) return;
  char cell[3] = {(char)('A' + c), (char)('1' + r), 0};
  eggStep(cell);
  if (cur != ui::GRID || !gSnap) return;
  det::Entry *en = gSnap;
  const int n = det::snapshot(en, det::kMax, true);
  int best = -1;
  for (int i = 0; i < n; i++)
    if (en[i].cell == c + r * 10 && (best < 0 || det::smoothed(en[i]) > det::smoothed(en[best]))) best = i;
  if (best >= 0) { selectEntry(en[best]); ui::show(ui::DETAILS); }
}
void buildGrid() {
  for (int c = 0; c < 10; c++) { char s[2] = {(char)('A' + c), 0}; lv_obj_set_pos(label(content, s, &lv_font_unscii_8, CYD32_COL_AMBER), kGx + c * kCw + 8, 2); }
  lv_obj_t *g = box(content, kGx, kGy, 10 * kCw, 8 * kCh, CYD32_COL_PANEL);
  lv_obj_set_style_border_color(g, C(CYD32_COL_GRID), 0); lv_obj_set_style_border_width(g, 1, 0);
  lv_obj_add_flag(g, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g, onGridTouch, LV_EVENT_CLICKED, nullptr);
  for (int r = 0; r < 8; r++) {
    char s[2] = {(char)('1' + r), 0};
    lv_obj_set_pos(label(content, s, &lv_font_unscii_8, CYD32_COL_AMBER), 10, kGy + r * kCh + 5);
    for (int c = 0; c < 10; c++) {
      gridMarks[r * 10 + c] = label(g, ".", &lv_font_unscii_8, CYD32_COL_GRID);
      lv_obj_set_pos(gridMarks[r * 10 + c], c * kCw + 8, r * kCh + 4);
    }
  }
  gridLog = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(gridLog, 266, 12);
  lv_obj_set_width(gridLog, 56);
  lv_obj_t *h = label(content, "TAP A CELL: DETAIL  ANGLE INDICATIVE", &lv_font_unscii_8, CYD32_SONAR_G2);
  lv_obj_set_pos(h, 4, kGy + 8 * kCh + 8);
}
// Cells come from bearing::cellFromMac (same on every CYD32 and the StopWatch).
void refreshGrid() {
  det::Entry *e = gSnap;
  if (!e) return;
  const int n = det::snapshot(e, det::kMax, true);
  uint8_t mark[80] = {0};
  for (int i = 0; i < n; i++) { uint8_t &m = mark[e[i].cell % 80]; m = e[i].kind == det::DRONE ? 2 : (m ? m : 1); }
  for (int i = 0; i < 80; i++) {
    lv_label_set_text(gridMarks[i], mark[i] ? "X" : ".");
    lv_obj_set_style_text_color(gridMarks[i], C(mark[i] == 2 ? CYD32_COL_DRONE : mark[i] ? CYD32_COL_HITX : CYD32_COL_GRID), 0);
  }
  // Strongest five (smoothed): kind letter, cell, dBm.
  char t[96] = "LOG\n";
  for (int k = 0; k < 5 && k < n; k++) {
    int b = -1;
    for (int i = 0; i < n; i++) if (e[i].trailN != 0xFF && (b < 0 || det::smoothed(e[i]) > det::smoothed(e[b]))) b = i;
    if (b < 0) break;
    char cell[3], l[16];
    uic::cellName(e[b].cell, cell);
    snprintf(l, sizeof l, "%c%s%d\n", e[b].kind == det::WIFI ? 'W' : e[b].kind == det::BLE ? 'B' : 'D', cell, det::smoothed(e[b]));
    strncat(t, l, sizeof t - strlen(t) - 1);
    e[b].trailN = 0xFF;  // scratch copy: mark as listed
  }
  lv_label_set_text(gridLog, t);
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
  const uint16_t target = CYD32_SIZE_ZOOM[s ? s - 1 : 0];
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
#if CYD32_CREATURE_ZOOM != 256
  z = (uint16_t)((uint32_t)z * (uint32_t)CYD32_CREATURE_ZOOM / 256u);
#endif
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
  lv_image_set_scale_x(creaImg, z);
  lv_image_set_scale_y(creaImg, zy);
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
  if (i4Buf && anim && curSpec && anim->ms && anim != curSpec->cth_keys && !eggHold) {
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
  if (reactReq && !playingHatch && !playingHappy) { reactReq = false; startHappy(); }
  compose();
  uint32_t period = 50;
  if (anim && anim->ms) period = anim->ms;
  lv_timer_set_period(t, period);
}
void creaAction(lv_event_t *e) { creature::addXp((uintptr_t)lv_event_get_user_data(e)); reactReq = true; }
void buildCreature() {
  curSpec = creature::spec();
  creaImg = lv_image_create(content);
  lv_obj_set_pos(creaImg, 48, 40);
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
  lv_obj_set_pos(creaInfo, 176, 12);
  creaBar = lv_bar_create(content);
  lv_obj_set_size(creaBar, 130, 8); lv_obj_set_pos(creaBar, 176, 100);
  lv_obj_set_style_bg_color(creaBar, C(CYD32_COL_GRID), 0);
  lv_obj_set_style_bg_color(creaBar, C(CYD32_COL_CYAN), LV_PART_INDICATOR);
  button(content, "PET", 176, 120, 62, 32, creaAction, (void *)1, CYD32_COL_CYAN);
  button(content, "FEED", 244, 120, 62, 32, creaAction, (void *)2, CYD32_COL_CYAN);
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
    growTo = CYD32_SIZE_ZOOM[s ? s - 1 : 0];
    growStart = nowMs ? nowMs : lv_tick_get();
    if (!playingHatch) startIdle();
  }
  if (creature::consumeCthulhuUp() && !playingHatch) startIdle();
  const uint8_t fresh = creature::consumeNewTraits();
  if (fresh) { popping = fresh; popStart = nowMs ? nowMs : lv_tick_get(); }
}

// MENU: the same 8-entry tree as the StopWatch (ui_common.cpp), 2 x 4 buttons.
void buildMenu() {
  for (int i = 0; i < uic::kMenuN; i++) {
    lv_obj_t *b = button(content, uic::kMenuTxt[i], 8 + (i % 2) * 156, 6 + (i / 2) * 44, 148, 38, onShow,
                         (void *)(intptr_t)uic::kMenuScr[i], CYD32_SONAR_G4);
    lv_obj_set_style_bg_color(b, C(CYD32_SONAR_BG_CYD), 0);
    lv_obj_set_style_border_color(b, C(CYD32_SONAR_G2), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &lv_font_montserrat_16, 0);
  }
}
void onBack(lv_event_t *) { ui::show(prevSc); }
void onTrack(lv_event_t *) {
  if (haveSel) { trackedId = selEnt.id; hasTracked = true; }
  ui::show(ui::SONAR);
}
void onDetNext(lv_event_t *) {
  det::Entry *e = gSnap;
  if (!e) return;
  const int n = det::snapshot(e, det::kMax, true);
  const int i = uic::nextClockwise(e, n, selEnt.id, haveSel);
  if (i >= 0) selectEntry(e[i]);
  ui::refresh();
}
// DEVICE DETAIL: one device (from the sonar, the grid, a list or the dashboard);
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
  char t[320], id[9], cell[3], tags[32], ch[12] = "";
  snprintf(t, sizeof t, "%s  %s", drone ? "HIT! DRONE" : uic::kindName(selEnt.kind), rssi::zone(sm));
  lv_label_set_text(detHit, t);
  lv_obj_set_style_text_color(detHit, C(drone ? CYD32_COL_DRONE : CYD32_SONAR_G4), 0);
  privacy::idToStr(selEnt.id, id);
  uic::cellName(selEnt.cell, cell);
  uic::badgeStr(selEnt.badges, tags, sizeof tags, true);
  if (selEnt.kind == det::WIFI) snprintf(ch, sizeof ch, "  CH %u", selEnt.channel);
  snprintf(t, sizeof t,
           "CELL   %s%s\nRSSI   %d dBm  (avg %d)\nANGLE  %03u  (indicative, not a\n       direction: hash of the address)\n"
           "TAGS   %s\nID     %s  (salted per boot)\nSEEN   %lu s ago",
           cell, ch, selEnt.rssi, sm, (unsigned)selEnt.bearing, tags[0] ? tags : "-", id,
           (unsigned long)((nowMs - selEnt.lastSeen) / 1000));
  lv_label_set_text(detBody, t);
}

// Rows shared by the TRACKERS / DRONES screens (tap a row: DEVICE DETAIL).
void onListRow(lv_event_t *e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i >= listN) return;
  selectEntry(listEnt[i]);
  ui::show(ui::DETAILS);
}
void buildList(bool drones) {
  lv_obj_t *t = label(content, drones ? "DRONES  REMOTE ID (ASTM F3411)" : "TRACKERS  TRK / META / FLOCK", &lv_font_montserrat_14,
                      drones ? CYD32_SONAR_DRONE : CYD32_SONAR_G4);
  lv_obj_set_pos(t, 6, 3);
  for (int i = 0; i < kListRows; i++) {
    listRows[i] = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
    lv_obj_set_pos(listRows[i], 6, 24 + i * 15);
    lv_obj_set_size(listRows[i], 308, 14);
    lv_obj_add_flag(listRows[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(listRows[i], onListRow, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }
  listEmpty = label(content, "", &lv_font_montserrat_14, CYD32_SONAR_G3);
  lv_obj_align(listEmpty, LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_t *f = label(content, drones ? "ID / position fields: not decoded yet" : "Strongest first. Tap a row: detail",
                      &lv_font_unscii_8, CYD32_SONAR_G2);
  lv_obj_set_pos(f, 6, 166);
}
// Keep the n strongest entries matching `want`, strongest first.
template <typename F>
int pickStrongest(det::Entry *out, int max, bool filtered, F want) {
  det::Entry *e = gSnap;
  if (!e) return 0;
  const int n = det::snapshot(e, det::kMax, filtered);
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
  char cell[3], tags[24], id[9], ch[8] = "";
  uic::cellName(x.cell, cell);
  uic::badgeStr(x.badges, tags, sizeof tags, true);
  privacy::idToStr(x.id, id);
  id[6] = 0;
  if (x.kind == det::WIFI) snprintf(ch, sizeof ch, "ch%u", x.channel);
  snprintf(t, cap, "%s %-4s %4d dBm %s %-4s %s", cell, uic::kindName(x.kind), det::smoothed(x), id, ch, tags);
}
void refreshList() {
  const bool drones = cur == ui::DRONES;
  listN = pickStrongest(listEnt, kListRows, false, [drones](const det::Entry &x) {
    return drones ? x.kind == det::DRONE
                  : x.kind != det::DRONE && (x.badges & (sig::TRK | sig::META | sig::FLOCK));
  });
  for (int i = 0; i < kListRows; i++) {
    char t[64] = "";
    if (i < listN) rowText(listEnt[i], t, sizeof t);
    lv_label_set_text(listRows[i], t);
    lv_obj_set_style_text_color(listRows[i], C(i < listN && listEnt[i].kind == det::DRONE ? CYD32_COL_DRONE : CYD32_COL_WHITE), 0);
  }
  lv_label_set_text(listEmpty, listN ? "" : drones ? "No drone in range" : "No tracker, Meta or Flock device");
}

// COUNTERS = the v0.1 dashboard on one page: counters, Wi-Fi and BLE lists
// (5 strongest each), a 3-line log of first sightings (drones and tagged
// devices), 4 display filters and the ABOUT notice.
const uint8_t kFiltBit[4] = {det::F_WIFI, det::F_BLE, det::F_RID, det::F_TAGGED_ONLY};
const char *kFiltName[4] = {"WIFI", "BLE", "RID", "TAGGED"};
void onFilter(lv_event_t *e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  det::setFilters((uint8_t)(det::filters() ^ kFiltBit[i]));
  ui::refresh();
}
det::Entry dashEnt[10];
void onDashRow(lv_event_t *e) {
  const int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (!lv_label_get_text(i < 5 ? dashW[i] : dashB[i - 5])[0]) return;
  selectEntry(dashEnt[i]);
  ui::show(ui::DETAILS);
}
void buildCounters() {
  dashTop = label(content, "", &lv_font_unscii_8, CYD32_SONAR_G4);
  lv_obj_set_pos(dashTop, 4, 3);
  lv_obj_set_width(dashTop, 312);
  lv_obj_t *wl = label(content, "WIFI", &lv_font_unscii_8, CYD32_SONAR_G3);
  lv_obj_set_pos(wl, 4, 26);
  lv_obj_t *bl = label(content, "BLE", &lv_font_unscii_8, CYD32_SONAR_G3);
  lv_obj_set_pos(bl, 164, 26);
  for (int i = 0; i < 5; i++) {
    dashW[i] = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
    lv_obj_set_pos(dashW[i], 4, 37 + i * 11);
    lv_obj_set_size(dashW[i], 152, 11);
    dashB[i] = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
    lv_obj_set_pos(dashB[i], 164, 37 + i * 11);
    lv_obj_set_size(dashB[i], 152, 11);
    lv_obj_add_flag(dashW[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dashB[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dashW[i], onDashRow, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(dashB[i], onDashRow, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 5));
  }
  lv_obj_t *ll = label(content, "LOG", &lv_font_unscii_8, CYD32_SONAR_G3);
  lv_obj_set_pos(ll, 4, 95);
  for (int i = 0; i < 3; i++) {
    dashLog[i] = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
    lv_obj_set_pos(dashLog[i], 36, 95 + i * 11);
  }
  for (int i = 0; i < 4; i++) {
    dashChip[i] = button(content, kFiltName[i], 4 + i * 64, 138, 60, 32, onFilter, (void *)(intptr_t)i, CYD32_SONAR_G4);
    lv_obj_set_style_text_font(lv_obj_get_child(dashChip[i], 0), &lv_font_unscii_8, 0);
  }
  lv_obj_t *ab = button(content, "ABOUT", 262, 138, 54, 32, onShow, (void *)(intptr_t)ui::ABOUT, CYD32_SONAR_G3);
  lv_obj_set_style_text_font(lv_obj_get_child(ab, 0), &lv_font_unscii_8, 0);
}
void refreshCounters() {
  const det::Counts c = det::counts();
  char t[96];
  snprintf(t, sizeof t, "W %u  B %u  RID %u  TRK %u META %u FLK %u\n3 MIN %u   24 H %u   FILTERS: DISPLAY",
           (unsigned)c.wifi, (unsigned)c.ble, (unsigned)c.drone, (unsigned)c.trk, (unsigned)c.meta, (unsigned)c.flock,
           (unsigned)c.recent3m, (unsigned)c.unique24h);
  lv_label_set_text(dashTop, t);
  det::Entry w[5], b[5];
  const int nw = pickStrongest(w, 5, true, [](const det::Entry &x) { return x.kind == det::WIFI; });
  const int nb = pickStrongest(b, 5, true, [](const det::Entry &x) { return x.kind == det::BLE; });
  for (int i = 0; i < 5; i++) {
    char cell[3], tags[24], l[40] = "";
    if (i < nw) {
      dashEnt[i] = w[i];
      uic::cellName(w[i].cell, cell);
      uic::badgeStr(w[i].badges, tags, sizeof tags);
      snprintf(l, sizeof l, "%s %4d ch%-2u %s", cell, det::smoothed(w[i]), w[i].channel, tags);
    } else if (i == 0) {
      snprintf(l, sizeof l, "%s", det::filters() & det::F_WIFI ? "-" : "(filtered)");
    }
    lv_label_set_text(dashW[i], l);
    l[0] = 0;
    if (i < nb) {
      dashEnt[i + 5] = b[i];
      uic::cellName(b[i].cell, cell);
      uic::badgeStr(b[i].badges, tags, sizeof tags, true);
      snprintf(l, sizeof l, "%s %4d %s", cell, det::smoothed(b[i]), tags);
    } else if (i == 0) {
      snprintf(l, sizeof l, "%s", det::filters() & det::F_BLE ? "-" : "(filtered)");
    }
    lv_label_set_text(dashB[i], l);
  }
  det::LogEvt lg[3];
  const int nl = det::logSnapshot(lg, 3);
  for (int i = 0; i < 3; i++) {
    char l[48] = "", tm[8], cell[3], tags[24];
    if (i < nl) {
      uic::mmss(lg[i].ms, tm);
      uic::cellName(lg[i].cell, cell);
      uic::badgeStr(lg[i].badges, tags, sizeof tags);
      snprintf(l, sizeof l, "%s %-5s %s %4d dBm", tm, lg[i].kind == det::DRONE ? "RID" : tags, cell, lg[i].rssi);
    } else if (i == 0) {
      snprintf(l, sizeof l, "no drone or tagged device yet");
    }
    lv_label_set_text(dashLog[i], l);
    lv_obj_set_style_text_color(dashLog[i], C(i < nl && lg[i].kind == det::DRONE ? CYD32_COL_DRONE : CYD32_COL_WHITE), 0);
  }
  const uint8_t f = det::filters();
  for (int i = 0; i < 4; i++) {
    const bool on = f & kFiltBit[i];
    lv_obj_set_style_bg_color(dashChip[i], C(on ? CYD32_SONAR_G4 : CYD32_SONAR_BG_CYD), 0);
    lv_obj_set_style_border_color(dashChip[i], C(on ? CYD32_SONAR_G4 : CYD32_SONAR_G2), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(dashChip[i], 0), C(on ? CYD32_SONAR_BG_CYD : CYD32_SONAR_G3), 0);
  }
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
void buildAbout() {
  char t[400];
  snprintf(t, sizeof t,
           "CYD32 - passive RF sonar (receive only)\n\nVersion  %s\nCommit   %s\nLicence  %s\nTarget   %s\nStatus   %s\nKey      %s\n\nSource code:\n%s\n\n"
           "ABSOLUTELY NO WARRANTY. Free software:\nredistribute it under the AGPL-3.0;\nget the source at the URL above.",
           CYD32_VERSION, CYD32_COMMIT, CYD32_LICENSE, CYD32_TARGET, fw_status::kindStr(), fw_status::keyFingerprint(), CYD32_SOURCE_URL);
  label(content, t, &lv_font_unscii_8, CYD32_COL_WHITE);
}
void buildTorpedo() {
  lv_obj_t *l = lv_line_create(content);
  static lv_point_precise_t p[2] = {{20, 160}, {280, 30}};
  lv_line_set_points(l, p, 2);
  lv_obj_set_style_line_color(l, C(CYD32_COL_CYAN), 0);
  lv_obj_set_style_line_width(l, 3, 0);
  lv_obj_set_style_line_dash_width(l, 8, 0); lv_obj_set_style_line_dash_gap(l, 5, 0);
  lv_obj_t *h = box(content, 268, 18, 24, 24, CYD32_COL_HIT); lv_obj_set_style_radius(h, LV_RADIUS_CIRCLE, 0);
  lv_obj_t *t = label(content, "SUNK!", &lv_font_montserrat_24, CYD32_COL_HIT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 10);
  lv_obj_t *x = label(content, "+50 XP", &lv_font_montserrat_16, CYD32_COL_OK);
  lv_obj_align(x, LV_ALIGN_CENTER, 0, 44);
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
  if (cur != s && cur != BOOT && cur != DETAILS && cur != TORPEDO) prevSc = cur;
  if (fxTimer) { lv_timer_delete(fxTimer); fxTimer = nullptr; }
  if (sonarTimer) { lv_timer_delete(sonarTimer); sonarTimer = nullptr; }
  sonarN = 0;
  if (stage) { lv_obj_delete(stage); stage = nullptr; }
  radarObj = bootBar = bootPct = bootLogo = bootTitle = scanLine = nullptr;
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
    case GRID: buildGrid(); break;
    case CREATURE: buildCreature(); break;
    case MENU: buildMenu(); break;
    case DETAILS: buildDetails(); break;
    case TRACKERS: buildList(false); break;
    case DRONES: buildList(true); break;
    case COUNTERS: buildCounters(); break;
    case EVOLUTION: buildCreature(); break;
    case WARDRIVING: buildStub("WARDRIVE", "On request only: export to the local microSD card, hashed MACs, never uploaded anywhere."); break;
    case UPDATE_CONFIRM: buildUpdateConfirm(); break;
    case OTA: buildOta(); break;
    case ABOUT: buildAbout(); break;
    case TORPEDO: buildTorpedo(); break;
    case UNSIGNED_CONFIRM: buildUnsignedConfirm(); break;
  }
  refresh();
}
void refresh() {
  nowMs = lv_tick_get();
  refreshHeader();
  switch (cur) {
    case SONAR: refreshSonar(); break;
    case GRID: refreshGrid(); break;
    case CREATURE: refreshCreature(); break;
    case DETAILS: refreshDetails(); break;
    case TRACKERS:
    case DRONES: refreshList(); break;
    case COUNTERS: refreshCounters(); break;
    case OTA: refreshOta(); break;
    default: break;
  }
}
bool takeUpdateRequest() { bool r = updateReq; updateReq = false; return r; }
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
