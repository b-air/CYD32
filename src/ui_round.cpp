// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Native 466x466 round HUD for the M5Stack StopWatch (v8 design). Compiled
// only for that target; the CYD rectangular UI stays in src/ui.cpp.
#ifdef CYD32_HW_M5STACK_STOPWATCH
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <esp_heap_caps.h>
#include "lvgl.h"
#include "cyd32_build_gen.h"
#include "creature.h"
#include "detections.h"
#include "fw_status.h"
#include "hw.h"
#include "ota_portal.h"
#include "privacy.h"
#include "scanner.h"
#include "ui_layout.h"
#include "wardriving.h"
#include "cyd32_sonar_theme.h"
#include "ui_radar.h"
#include "src/core/lv_obj_event_private.h"  // lv_hit_test_info_t (polar tab hit test)

// Boot logo tile (I4, StopWatch background baked in). The Graphic Designer is
// replacing it with an "M5Stack" tile under a new name: swap it here.
#define CYD32_SW_BOOT_LOGO img_cyd32_logo_sw

namespace {
// StopWatch: green phosphor on every screen (user request, 18 Nov 2026). The
// former Y3-d tokens map to the v9 greens; yellow #FCEE0A is for drones only;
// red stays for warnings (unsigned mode, downgrade, failures).
constexpr uint16_t COL_BG = CYD32_SONAR_BG_SW, COL_YEL = CYD32_SONAR_G4, COL_CYAN = CYD32_SONAR_G5;
constexpr uint16_t COL_HIT = 0xF80E, COL_OK = CYD32_SONAR_G4, COL_TEXT = CYD32_SONAR_G5;
constexpr uint16_t COL_DRONE = CYD32_SONAR_DRONE;
constexpr uint16_t COL_DIM = CYD32_SONAR_G2, COL_GRID = CYD32_SONAR_G1, COL_MISS = CYD32_SONAR_G3;
constexpr uint16_t COL_TRACK = CYD32_SONAR_G1, COL_RIM = CYD32_SONAR_G3;
constexpr int CX = CYD32_CX, CY = CYD32_CY;
constexpr float PI = 3.14159265f;

lv_color_t C(uint16_t v) {
  return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3);
}

lv_obj_t *scr, *content, *tabbar, *banner, *statusBand, *statusBat;
lv_obj_t *bootMsg[3];
ui::Screen cur = ui::BOOT, prevSc = ui::HOME;
bool updateReq = false, unsignedBanner = false;
int unsignedDecision = -1;
int menuSel = 0;
uint32_t nowMs = 0;
lv_timer_t *btnTimer = nullptr;
// Chrome: one custom-drawn frame (yellow rim, dim rings, compass ticks) for the
// Y3-d screens, hidden on the phosphor screens (boot, sonar, wardriving) that
// draw their own scope. One object instead of 5 rings + 36 line objects.
lv_obj_t *frame, *keyA, *keyAL, *keyB, *keyBL;
bool phosphor = false;
// Phosphor screens
lv_obj_t *radarObj, *bootLogo, *bootTitle, *scanLine, *bootLoad;
lv_timer_t *fxTimer = nullptr;
uint32_t fxNext = 0, fxEnd = 0;
int16_t scanY = 0;
lv_obj_t *hudW, *hudB, *hudR, *hudSel, *hudSel2;
lv_obj_t *wdCnt[3], *wdRid;
uint32_t trackedId = 0;
bool hasTracked = false;

constexpr int kCw = 24, kCh = 20, kGx = 113, kGy = 108;
lv_obj_t *gridMarks[80], *gridPlate, *gridPlateLbl;
int selCol = -1, selRow = -1;
det::Entry selEnt{};
bool haveSel = false;
const char *kEgg[4] = {"A1", "B2", "C3", "D4"};
int eggPos = 0;
uint32_t eggStart = 0;
bool eggDone = false;

lv_obj_t *detBody, *histBar[16];
int8_t hist[16];
int histN = 0;
uint32_t histLast = 0;

lv_obj_t *otaQr, *otaInfo, *otaMsg, *otaPct, *otaConfirm, *otaSeg[17];
lv_obj_t *wdTimerLbl, *wdRecLbl, *wdRecDot;
lv_obj_t *cntArc[3], *cntNum[3];

// Creature renderer: decode I4 into 64x64 RGB565A8, then integer x3
// nearest-neighbour into a shared PSRAM buffer (192x192). LVGL 9 cannot
// scale I4 directly.
constexpr int kSpr = 64, kNN = 3, kBig = kSpr * kNN;
constexpr size_t kI4Size = kSpr * kSpr / 2;
constexpr size_t kRgb64 = kSpr * kSpr * 3;
constexpr size_t kRgb192 = kBig * kBig * 3;
uint8_t *pool = nullptr, *i4Buf = nullptr, *i4Draw = nullptr, *rgb64 = nullptr, *rgb192 = nullptr;
lv_image_dsc_t rgbDsc;
const cyd32_creature_t *curSpec = nullptr;
const cyd32_anim_t *anim = nullptr;
lv_timer_t *animTimer = nullptr;
lv_obj_t *creaImg, *creaInfo, *flash;
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
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, C(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
lv_obj_t *button(lv_obj_t *p, const char *t, int x, int y, int w, int h, lv_event_cb_t cb, void *ud, uint16_t col = COL_YEL) {
  lv_obj_t *b = box(p, x, y, w, h, COL_BG);
  lv_obj_set_style_border_color(b, C(col), 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(b, C(COL_GRID), LV_STATE_PRESSED);
  lv_obj_t *l = label(b, t, &lv_font_montserrat_14, col);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}
void onShow(lv_event_t *e) { ui::show((ui::Screen)(intptr_t)lv_event_get_user_data(e)); }

lv_obj_t *mkArc(lv_obj_t *p, int x, int y, int s, int a0, int a1, uint16_t bg, uint16_t fg, int w) {
  lv_obj_t *a = lv_arc_create(p);
  lv_obj_set_pos(a, x, y);
  lv_obj_set_size(a, s, s);
  lv_arc_set_bg_angles(a, a0, a1);
  lv_arc_set_range(a, 0, 100);
  lv_arc_set_value(a, 0);
  lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, C(bg), LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, C(fg), LV_PART_INDICATOR);
  lv_obj_set_style_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_width(a, 0, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  return a;
}

void drawFrame(lv_event_t *e) {
  lv_layer_t *L = lv_event_get_layer(e);
  lv_draw_arc_dsc_t a;
  lv_draw_arc_dsc_init(&a);
  a.center.x = CX; a.center.y = CY;
  a.start_angle = 0; a.end_angle = 360;
  static const int16_t rings[5] = {232, 210, 168, 126, 84};
  for (int i = 0; i < 5; i++) {
    a.radius = rings[i];
    a.width = i == 0 ? 3 : 1;
    a.color = C(i == 0 ? COL_RIM : COL_DIM);
    lv_draw_arc(L, &a);
  }
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.width = 1;
  for (int i = 0; i < 36; i++) {
    const float an = i * 10.f * PI / 180.f;
    const bool longt = i % 3 == 0;
    const float r0 = longt ? 218.f : 223.f, r1 = 230.f;
    d.p1.x = (lv_value_precise_t)lroundf(CX + r0 * cosf(an)); d.p1.y = (lv_value_precise_t)lroundf(CY + r0 * sinf(an));
    d.p2.x = (lv_value_precise_t)lroundf(CX + r1 * cosf(an)); d.p2.y = (lv_value_precise_t)lroundf(CY + r1 * sinf(an));
    d.color = C(longt ? COL_YEL : COL_DIM);
    lv_draw_line(L, &d);
  }
}

// --- curved tab band -----------------------------------------------------------
constexpr int kBandX = 70, kBandY = 320, kBandW = 326, kBandH = 122;  // bbox of the annulus sector
const ui::Screen kTabScr[4] = {ui::SONAR, ui::GRID, ui::CREATURE, ui::MENU};
const char *kTabTxt[4] = {"SONAR", "GRID", "CREA", "MENU"};
int bandTabAt(int x, int y) {
  const int dx = x - CX, dy = y - CY;
  const int r2 = dx * dx + dy * dy;
  if (r2 < SW_TAB_R0 * SW_TAB_R0 || r2 > (SW_TAB_R1 + 4) * (SW_TAB_R1 + 4)) return -1;
  const int brg = radar::bearingOf(dx, dy);
  if (brg < SW_TAB_BRG0 || brg > SW_TAB_BRG1) return -1;
  const int i = (SW_TAB_BRG1 - brg) / SW_TAB_SPAN_DEG;
  return i > 3 ? 3 : i;
}
void onBand(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_HIT_TEST) {
    lv_hit_test_info_t *info = static_cast<lv_hit_test_info_t *>(lv_event_get_param(e));
    info->res = bandTabAt(info->point->x, info->point->y) >= 0;
  } else if (code == LV_EVENT_CLICKED) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    const int i = bandTabAt(p.x, p.y);
    if (i >= 0) ui::show(kTabScr[i]);
  } else if (code == LV_EVENT_DRAW_MAIN) {
    lv_layer_t *L = lv_event_get_layer(e);
    const int active = cur == ui::SONAR ? 0 : cur == ui::GRID ? 1 : cur == ui::CREATURE ? 2 : cur == ui::MENU ? 3 : -1;
    const uint16_t onFill = CYD32_SONAR_G4, offFill = CYD32_SONAR_G1;
    const uint16_t outline = CYD32_SONAR_G2, offText = CYD32_SONAR_G4;
    for (int i = 0; i < 4; i++) {
      const int hi = SW_TAB_BRG1 - i * SW_TAB_SPAN_DEG - SW_TAB_GAP_DEG / 2;
      const int lo = SW_TAB_BRG1 - (i + 1) * SW_TAB_SPAN_DEG + SW_TAB_GAP_DEG / 2;
      const bool on = i == active;
      lv_draw_arc_dsc_t a;
      lv_draw_arc_dsc_init(&a);
      a.center.x = CX; a.center.y = CY;
      a.start_angle = lo - 90; a.end_angle = hi - 90;  // bearing 0 = up, LVGL 0 = 3 o'clock
      a.radius = SW_TAB_R1;
      a.width = SW_TAB_R1 - SW_TAB_R0;
      a.color = C(on ? onFill : offFill);
      lv_draw_arc(L, &a);
      if (!on) {
        a.width = 1;
        a.color = C(outline);
        lv_draw_arc(L, &a);
        a.radius = SW_TAB_R0 + 1;
        lv_draw_arc(L, &a);
        lv_draw_line_dsc_t d;
        lv_draw_line_dsc_init(&d);
        d.color = C(outline);
        d.width = 1;
        const int edges[2] = {lo, hi};
        for (int b : edges) {
          int16_t x0, y0, x1, y1;
          radar::polar(SW_TAB_R0, b, x0, y0);
          radar::polar(SW_TAB_R1, b, x1, y1);
          d.p1.x = CX + x0; d.p1.y = CY + y0; d.p2.x = CX + x1; d.p2.y = CY + y1;
          lv_draw_line(L, &d);
        }
      }
      int16_t tx, ty;
      radar::polar((SW_TAB_R0 + SW_TAB_R1) / 2, (lo + hi) / 2, tx, ty);
      lv_draw_label_dsc_t ld;
      lv_draw_label_dsc_init(&ld);
      ld.font = &lv_font_montserrat_14;
      ld.color = C(on ? COL_BG : offText);
      ld.text = kTabTxt[i];
      ld.align = LV_TEXT_ALIGN_CENTER;
      const lv_area_t ta = {CX + tx - 30, CY + ty - 8, CX + tx + 30, CY + ty + 8};
      lv_draw_label(L, &ld, &ta);
    }
  }
}

void refreshStatus() {
  if (statusBand) {
    // "W!" in red when the Wi-Fi scanner failed to start (same slot as "2.4G").
    const bool wFail = scanner::wifiFailed();
    lv_label_set_text(statusBand, wFail ? "W!" : scanner::wifiActive() ? "2.4G" : "BLE");
    lv_obj_set_style_text_color(statusBand, C(wFail ? COL_HIT : phosphor ? CYD32_SONAR_G3 : COL_CYAN), 0);
  }
  if (statusBat) {
    char t[8];
    snprintf(t, sizeof t, "%d%%", hw::batteryPct());
    lv_label_set_text(statusBat, t);
  }
  if (banner) {
    if (unsignedBanner || fw_status::kind() == fw_status::UNSIGNED) lv_obj_remove_flag(banner, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
  }
}

void buildChrome() {
  scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, C(COL_BG), 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(scr, LV_RADIUS_CIRCLE, 0);
  // No clip_corner here: on a parent with children, LVGL 9.2 renders them into
  // two ARGB8888 layers as tall as the radius (466 x 233 x 4 = 434 KB each).
  // That cannot fit in LV_MEM_SIZE, the layer allocation fails, and with
  // LV_OS_NONE lv_draw_dispatch_wait_for_request() spins forever: the first
  // frame never reaches the flush callback (black screen in v0.2.0-test2).
  // The round glass already hides the corners, so nothing is lost.

  frame = lv_obj_create(scr);
  lv_obj_remove_style_all(frame);
  lv_obj_set_size(frame, CYD32_SCREEN_W, CYD32_SCREEN_H);
  lv_obj_remove_flag(frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(frame, drawFrame, LV_EVENT_DRAW_MAIN, nullptr);

  content = box(scr, 0, 0, CYD32_SCREEN_W, CYD32_SCREEN_H, COL_BG);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);

  keyA = label(scr, "A", &lv_font_unscii_8, COL_YEL);
  keyAL = label(scr, "BACK", &lv_font_unscii_8, COL_YEL);
  keyB = label(scr, "B", &lv_font_unscii_8, COL_CYAN);
  keyBL = label(scr, "SELECT", &lv_font_unscii_8, COL_CYAN);

  statusBand = label(scr, "2.4G", &lv_font_unscii_8, COL_CYAN);
  statusBat = label(scr, "", &lv_font_unscii_8, COL_OK);

  banner = box(scr, 120, 52, 226, 16, COL_HIT);
  lv_obj_t *bl = label(banner, "UNSIGNED FIRMWARE MODE", &lv_font_unscii_8, COL_TEXT);
  lv_obj_center(bl);
  lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);

  // Curved tab band: annulus r 148-206 over bearings 230 -> 130 (bottom),
  // SONAR / GRID / CREA / MENU left to right, polar hit test.
  tabbar = lv_obj_create(scr);
  lv_obj_remove_style_all(tabbar);
  lv_obj_set_pos(tabbar, kBandX, kBandY);
  lv_obj_set_size(tabbar, kBandW, kBandH);
  lv_obj_add_flag(tabbar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(tabbar, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_remove_flag(tabbar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(tabbar, onBand, LV_EVENT_ALL, nullptr);
}

void setTabs(bool on) {
  if (on) lv_obj_remove_flag(tabbar, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(tabbar, LV_OBJ_FLAG_HIDDEN);
}

// Per-screen chrome theme: phosphor key tags and centred status line on the
// sonar-theme screens, Y3-d elsewhere.
void styleChrome() {
  if (phosphor) lv_obj_add_flag(frame, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(frame, LV_OBJ_FLAG_HIDDEN);
  const bool keys = cur != ui::BOOT;
  lv_obj_t *const keyObjs[4] = {keyA, keyAL, keyB, keyBL};
  for (lv_obj_t *o : keyObjs) {
    if (keys) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  // Phosphor key tags: A / B in unscii_16 centred on the 2 px rim tick at
  // 270 / 090 deg, captions in Montserrat 12. Y3-d: the original unscii_8 tags.
  const lv_font_t *kf = phosphor ? &lv_font_unscii_16 : &lv_font_unscii_8;
  const lv_font_t *cf = phosphor ? &lv_font_montserrat_12 : &lv_font_unscii_8;
  lv_obj_set_style_text_font(keyA, kf, 0);
  lv_obj_set_style_text_font(keyB, kf, 0);
  lv_obj_set_style_text_font(keyAL, cf, 0);
  lv_obj_set_style_text_font(keyBL, cf, 0);
  if (phosphor) {
    lv_obj_set_pos(keyA, 35, CY - 8);
    lv_obj_set_pos(keyAL, 26, CY + 12);
    lv_obj_set_pos(keyB, 423, CY - 8);
    lv_obj_set_pos(keyBL, 400, CY + 12);
  } else {
    lv_obj_set_pos(keyA, 18, CY - 18);
    lv_obj_set_pos(keyAL, 18, CY - 6);
    lv_obj_set_pos(keyB, 430, CY - 18);
    lv_obj_set_pos(keyBL, 398, CY - 6);
  }
  lv_obj_set_style_text_color(keyA, C(phosphor ? CYD32_SONAR_G4 : COL_YEL), 0);
  lv_obj_set_style_text_color(keyAL, C(phosphor ? CYD32_SONAR_G3 : COL_YEL), 0);
  lv_obj_set_style_text_color(keyB, C(phosphor ? CYD32_SONAR_G4 : COL_CYAN), 0);
  lv_obj_set_style_text_color(keyBL, C(phosphor ? CYD32_SONAR_G3 : COL_CYAN), 0);
  if (cur == ui::BOOT) {
    lv_obj_add_flag(statusBand, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(statusBat, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(statusBand, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(statusBat, LV_OBJ_FLAG_HIDDEN);
  }
  if (phosphor) {
    lv_obj_set_pos(statusBand, CX - 40, 104);
    lv_obj_set_pos(statusBat, CX + 4, 104);
  } else {
    lv_obj_set_pos(statusBand, 300, 64);
    lv_obj_set_pos(statusBat, 348, 64);
  }
  lv_obj_set_style_text_color(statusBat, C(phosphor ? CYD32_SONAR_G3 : COL_OK), 0);
  lv_obj_invalidate(tabbar);
}

uint32_t mix(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; return x; }

void goBack();
void doSelect();

void btnTick(lv_timer_t *) {
  if (hw::btnAClicked()) goBack();
  if (hw::btnBClicked()) doSelect();
  static uint32_t abMs = 0;
  if (hw::btnAPressed() && hw::btnBPressed()) {
    if (!abMs) abMs = lv_tick_get();
    else if (lv_tick_get() - abMs >= 800) {
      abMs = 0;
      if (cur != ui::BOOT && cur != ui::OTA && cur != ui::UNSIGNED_CONFIRM) ui::show(ui::MENU);
    }
  } else {
    abMs = 0;
  }
}

// --- phosphor helpers -------------------------------------------------------
radar::Style swStyle(ui::Screen sc) {
  radar::Style s{};
  s.cx = CX; s.cy = CY;
  s.bg = CYD32_SONAR_BG_SW;
  s.wedge = cyd32_sonar_wedge_sw;
  s.droneRamp = cyd32_sonar_drone_decay_sw;
  s.tick5 = SW_TICK_LEN_5; s.tick10 = SW_TICK_LEN_10; s.tick30 = SW_TICK_LEN_30;
  s.skip0 = 1; s.skip1 = 0;
  s.bearFont = &lv_font_unscii_16;
  s.bearRmax = SW_SAFE_R - 3;  // far corner <= 210
  s.bearMaj = CYD32_SONAR_G4; s.bearMin = CYD32_SONAR_G3;
  s.lineW = 2; s.underW = 4;
  s.sweep = true;
  s.echoR = SW_ECHO_R;
  if (sc == ui::BOOT) {
    for (int i = 0; i < 3; i++) s.rings[i] = sw_boot_ring_r[i];
    s.rim = sw_boot_ring_r[3];
    s.rimW = 1;
    s.ringCol = CYD32_SONAR_G1;
    s.crossR = s.rim;
    s.bearR = SW_BOOT_LABEL_R;
    s.bearMin = CYD32_SONAR_G2;
    s.bearSkip = (1u << 3) | (1u << 9);  // 090 / 270
  } else {
    for (int i = 0; i < 3; i++) s.rings[i] = sw_sonar_ring_r[i];
    s.rim = sw_sonar_ring_r[3];
    s.rimW = 2;
    s.ringCol = CYD32_SONAR_G2;
    s.bearR = SW_LABEL_R;
    s.keyTicks = true;
    s.bearSkip = (1u << 3) | (1u << 9);  // 090 / 270: A / B key tags
    if (sc == ui::SONAR) {
      s.bearSkip |= (1u << 5) | (1u << 6) | (1u << 7);  // 150 / 180 / 210 under the tab band
      s.skip0 = SW_TAB_BRG0 + 5; s.skip1 = SW_TAB_BRG1 - 5;  // no ticks showing through the tab gaps
      s.crossR = SW_CROSS_R;
      s.colPitchX10 = 292; s.rowPitch = 32;
      s.lettersFont = &lv_font_unscii_16;
      s.lettersDy = 4;
      s.idFont = &lv_font_unscii_16;
      s.boxSize = SW_TARGET_BOX;
      s.boxW = 2;
    } else {  // WARDRIVING: RSSI rings, no crosshair, no letters
      s.idFont = &lv_font_montserrat_12;
      s.echoR = 3;
      s.boxSize = 22;
      s.boxW = 2;
    }
  }
  return s;
}
lv_obj_t *txt(const char *t, const lv_font_t *f, uint16_t col, int y, int dx = 0) {
  lv_obj_t *l = label(content, t, f, col);
  lv_obj_align(l, LV_ALIGN_TOP_MID, dx, y);
  return l;
}

// V3 effect, kept cheap: a 1 px scanline walking down the screen (two rows
// invalidated per frame) and, every 1.5-3 s, a 100 ms burst that slides the
// logo and the title 2-3 px in opposite directions.
void fxTick(lv_timer_t *) {
  const uint32_t now = lv_tick_get();
  if (scanLine) {
    scanY = (int16_t)(scanY + 4);
    if (scanY > CY + 180) scanY = CY - 180;
    lv_obj_set_y(scanLine, scanY);
  }
  if (!bootLogo || !bootTitle) return;
  const int lx = CX - CYD32_SW_BOOT_LOGO.header.w / 2, tx = CX - img_cyd32_title_sw.header.w / 2;
  if (fxEnd && now >= fxEnd) {
    fxEnd = 0;
    lv_obj_set_x(bootLogo, lx);
    lv_obj_set_x(bootTitle, tx);
    if (scanLine) lv_obj_set_style_bg_color(scanLine, C(CYD32_SONAR_G3), 0);
  } else if (!fxEnd && now >= fxNext) {
    const uint32_t r = mix(now);
    const int d = (r & 1) ? 2 : 3, sgn = (r & 2) ? 1 : -1;
    fxEnd = now + 100;
    fxNext = now + 1500 + r % 1500;
    lv_obj_set_x(bootLogo, lx - sgn * d);
    lv_obj_set_x(bootTitle, tx + sgn * d);
    if (scanLine) lv_obj_set_style_bg_color(scanLine, C(CYD32_SONAR_G4), 0);
  }
}

// --- boot -----------------------------------------------------------------
void buildBoot() {
  radarObj = radar::create(content, 0, 0, CYD32_SCREEN_W, CYD32_SCREEN_H, swStyle(ui::BOOT));
  bootLogo = lv_image_create(content);
  lv_image_set_src(bootLogo, &CYD32_SW_BOOT_LOGO);
  lv_obj_set_pos(bootLogo, CX - CYD32_SW_BOOT_LOGO.header.w / 2, SW_BOOT_LOGO_Y);
  bootTitle = lv_image_create(content);
  lv_image_set_src(bootTitle, &img_cyd32_title_sw);
  lv_obj_set_pos(bootTitle, CX - img_cyd32_title_sw.header.w / 2, SW_BOOT_TITLE_Y);
  txt("DEFENSIVE SONAR", &lv_font_unscii_16, CYD32_SONAR_G4, SW_BOOT_SUBTITLE_Y);
  txt("M5 STOPWATCH", &lv_font_montserrat_14, CYD32_SONAR_G3, 262);
  bootMsg[0] = txt("BLE --", &lv_font_montserrat_14, CYD32_SONAR_G2, 292, -82);
  bootMsg[1] = txt("WIFI --", &lv_font_montserrat_14, CYD32_SONAR_G2, 292, 0);
  bootMsg[2] = txt("RID --", &lv_font_montserrat_14, CYD32_SONAR_G2, 292, 82);
  bootLoad = txt("LOAD 0%", &lv_font_montserrat_14, CYD32_SONAR_G3, 328);
  txt(CYD32_VERSION, &lv_font_montserrat_12, CYD32_SONAR_G2, 352);
  txt(CYD32_HOLD_HINT, &lv_font_montserrat_12, CYD32_SONAR_G2, 370);
  scanLine = box(content, CX - 180, CY - 180, 360, 1, CYD32_SONAR_G3);
  lv_obj_set_style_bg_opa(scanLine, 56, 0);  // style bg opacity, not object opacity: no layer
  scanY = CY - 180;
  fxNext = lv_tick_get() + 1200;
  fxEnd = 0;
  fxTimer = lv_timer_create(fxTick, 50, nullptr);
}

// --- sonar ----------------------------------------------------------------
void buildSonar() {
  radarObj = radar::create(content, 0, 0, CYD32_SCREEN_W, CYD32_SCREEN_H, swStyle(ui::SONAR));
  txt("SONAR ACTIVE", &lv_font_montserrat_14, CYD32_SONAR_G4, 76);
  hudW = label(content, "W 0000", &lv_font_montserrat_14, CYD32_SONAR_G4);
  lv_obj_set_pos(hudW, 118, 126);
  hudB = label(content, "B 0000", &lv_font_montserrat_14, CYD32_SONAR_G4);
  lv_obj_set_pos(hudB, 118, 146);
  hudR = label(content, "R 0000", &lv_font_montserrat_14, CYD32_SONAR_DRONE);
  lv_obj_set_pos(hudR, 118, 166);
  hudSel = label(content, "", &lv_font_montserrat_14, CYD32_SONAR_DRONE);
  lv_obj_align(hudSel, LV_ALIGN_TOP_RIGHT, -104, 296);
  hudSel2 = label(content, "", &lv_font_montserrat_12, CYD32_SONAR_G3);
  lv_obj_align(hudSel2, LV_ALIGN_TOP_RIGHT, -104, 316);
  txt("PASSIVE  RX ONLY", &lv_font_montserrat_12, CYD32_SONAR_G2, 332);
}
// Contacts sit in the same pseudonymous grid cell as on the GRID screen. The
// position is a hash of the address, not a direction (no direction finding),
// so the HUD shows the channel instead of a bearing.
void refreshSonar() {
  if (!radarObj) return;
  static det::Entry e[24];
  static radar::Contact rc[24];
  const int n = det::snapshot(e, 24);
  const radar::Style s = swStyle(ui::SONAR);
  int sel = -1, best = -100000, labels = 0;
  for (int i = 0; i < n; i++) {
    // Selected: the tracked target if visible, else strongest drone, else strongest contact.
    const int score = (hasTracked && e[i].id == trackedId ? 100000 : 0) + (e[i].kind == det::DRONE ? 1000 : 0) + e[i].rssi;
    if (score > best) { best = score; sel = i; }
  }
  for (int i = 0; i < n; i++) {
    const uint32_t h = mix(e[i].id), slot = h % 80;
    radar::Contact &c = rc[i];
    memset(&c, 0, sizeof c);
    radar::cellOffset(s, (int)(slot % 10), (int)(slot / 10), c.dx, c.dy);
    c.kind = e[i].kind == det::DRONE ? radar::DRONE : e[i].kind == det::BLE ? radar::BLE : radar::WIFI;
    c.seed = h;
    if (i == sel || c.kind == radar::DRONE || labels < 6) {
      c.id[0] = (char)('A' + slot % 10); c.id[1] = (char)('1' + slot / 10);
      if (i != sel) labels++;
    }
  }
  radar::setContacts(radarObj, rc, n, sel);
  const det::Counts c = det::counts();
  char t[32];
  if (scanner::wifiFailed()) snprintf(t, sizeof t, "W !");
  else snprintf(t, sizeof t, "W %04u", (unsigned)LV_MIN(c.wifi, 9999));
  lv_label_set_text(hudW, t);
  snprintf(t, sizeof t, "B %04u", (unsigned)LV_MIN(c.ble, 9999));
  lv_label_set_text(hudB, t);
  snprintf(t, sizeof t, "R %04u", (unsigned)LV_MIN(c.drone, 9999));
  lv_label_set_text(hudR, t);
  if (sel >= 0) {
    const bool drone = e[sel].kind == det::DRONE;
    snprintf(t, sizeof t, "%s %s", rc[sel].id, drone ? "DRONE" : e[sel].kind == det::BLE ? "BLE" : "WIFI");
    lv_label_set_text(hudSel, t);
    lv_obj_set_style_text_color(hudSel, C(drone ? CYD32_SONAR_DRONE : CYD32_SONAR_G4), 0);
    if (e[sel].kind == det::WIFI) snprintf(t, sizeof t, "CH %u  %dDBM", e[sel].channel, e[sel].rssi);
    else snprintf(t, sizeof t, "%dDBM", e[sel].rssi);
    lv_label_set_text(hudSel2, t);
  } else {
    lv_label_set_text(hudSel, "NO CONTACT");
    lv_obj_set_style_text_color(hudSel, C(CYD32_SONAR_G3), 0);
    lv_label_set_text(hudSel2, "");
  }
}

// --- grid + easter --------------------------------------------------------
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
void selectCell(int c, int r) {
  if (c < 0 || c > 9 || r < 0 || r > 7) return;
  selCol = c; selRow = r;
  char cell[3] = {(char)('A' + c), (char)('1' + r), 0};
  eggStep(cell);
  static det::Entry e[det::kMax];
  const int n = det::snapshot(e, det::kMax);
  haveSel = false;
  for (int i = 0; i < n; i++) {
    const int slot = (int)(mix(e[i].id) % 80);
    if (slot % 10 == c && slot / 10 == r) { selEnt = e[i]; haveSel = true; break; }
  }
  if (haveSel && gridPlateLbl) {
    char id[9], line[64];
    privacy::idToStr(selEnt.id, id);
    const char *k = selEnt.kind == det::DRONE ? "DRONE" : selEnt.kind == det::BLE ? "BLE" : "WIFI";
    snprintf(line, sizeof line, "%s %s\n%d dBm  ch%u", cell, k, selEnt.rssi, selEnt.channel);
    lv_label_set_text(gridPlateLbl, line);
    lv_obj_set_style_border_color(gridPlate, C(selEnt.kind == det::DRONE ? COL_DRONE : COL_YEL), 0);
    lv_obj_remove_flag(gridPlate, LV_OBJ_FLAG_HIDDEN);
  } else if (gridPlate) {
    lv_obj_add_flag(gridPlate, LV_OBJ_FLAG_HIDDEN);
  }
}
void onGridTouch(lv_event_t *e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t a;
  lv_obj_get_coords((lv_obj_t *)lv_event_get_target(e), &a);
  selectCell((p.x - a.x1) / kCw, (p.y - a.y1) / kCh);
}
void onPlate(lv_event_t *) { if (haveSel) ui::show(ui::DETAILS); }
void buildGrid() {
  label(content, "GRID A-J / 1-8", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  label(content, "X HIT   . MISS", &lv_font_unscii_8, COL_DIM);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 56);
  for (int c = 0; c < 10; c++) {
    char s[2] = {(char)('A' + c), 0};
    lv_obj_set_pos(label(content, s, &lv_font_unscii_8, COL_YEL), kGx + c * kCw + 8, kGy - 14);
  }
  lv_obj_t *g = box(content, kGx, kGy, 10 * kCw, 8 * kCh, COL_BG);
  lv_obj_set_style_border_color(g, C(COL_YEL), 0);
  lv_obj_set_style_border_width(g, 2, 0);
  lv_obj_add_flag(g, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g, onGridTouch, LV_EVENT_PRESSED, nullptr);
  for (int r = 0; r < 8; r++) {
    char s[2] = {(char)('1' + r), 0};
    lv_obj_set_pos(label(content, s, &lv_font_unscii_8, COL_YEL), kGx - 16, kGy + r * kCh + 4);
    for (int c = 0; c < 10; c++) {
      gridMarks[r * 10 + c] = label(g, ".", &lv_font_unscii_8, COL_MISS);
      lv_obj_set_pos(gridMarks[r * 10 + c], c * kCw + 8, r * kCh + 4);
    }
  }
  gridPlate = box(content, 133, 286, 200, 48, COL_BG);  // clear of the tab band
  lv_obj_set_style_border_width(gridPlate, 2, 0);
  lv_obj_set_style_border_color(gridPlate, C(COL_HIT), 0);
  lv_obj_add_flag(gridPlate, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(gridPlate, onPlate, LV_EVENT_CLICKED, nullptr);
  gridPlateLbl = label(gridPlate, "", &lv_font_unscii_8, COL_TEXT);
  lv_obj_center(gridPlateLbl);
  lv_obj_add_flag(gridPlate, LV_OBJ_FLAG_HIDDEN);
}
void refreshGrid() {
  static det::Entry e[det::kMax];
  const int n = det::snapshot(e, det::kMax);
  uint8_t mark[80] = {0};
  for (int i = 0; i < n; i++) {
    uint8_t &m = mark[mix(e[i].id) % 80];
    m = e[i].kind == det::DRONE ? 2 : (m ? m : 1);
  }
  for (int i = 0; i < 80; i++) {
    lv_label_set_text(gridMarks[i], mark[i] ? "X" : ".");
    lv_obj_set_style_text_color(gridMarks[i], C(mark[i] == 2 ? COL_DRONE : mark[i] ? COL_YEL : COL_MISS), 0);
  }
}

// --- target detail --------------------------------------------------------
void onTrack(lv_event_t *) {
  if (haveSel) { trackedId = selEnt.id; hasTracked = true; }
  ui::show(ui::SONAR);
}
void buildDetails() {
  const char *kind = haveSel ? (selEnt.kind == det::DRONE ? "DRONE" : selEnt.kind == det::BLE ? "BLE" : "WIFI") : "-";
  char title[32];
  snprintf(title, sizeof title, "TARGET %s", kind);
  label(content, title, &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  const bool drone = haveSel && selEnt.kind == det::DRONE;
  lv_obj_t *hit = label(content, drone ? "HIT! DRONE" : haveSel ? "CONTACT" : "NO TARGET",
                        &lv_font_montserrat_16, drone ? COL_DRONE : COL_YEL);
  lv_obj_align(hit, LV_ALIGN_TOP_MID, 0, 64);
  detBody = label(content, "", &lv_font_unscii_8, COL_TEXT);
  lv_obj_set_width(detBody, 320);
  lv_obj_align(detBody, LV_ALIGN_TOP_MID, 0, 96);
  label(content, "RSSI", &lv_font_unscii_8, COL_DIM);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 210);
  for (int i = 0; i < 16; i++) {
    histBar[i] = box(content, 145 + i * 11, 250, 8, 4, COL_YEL);
    lv_obj_add_flag(histBar[i], LV_OBJ_FLAG_HIDDEN);
  }
  histN = 0; histLast = 0;
  button(content, "TRACK", CX - 48, 320, 96, 48, onTrack, nullptr, COL_CYAN);
}
void refreshDetails() {
  if (!haveSel || !detBody) {
    if (detBody) lv_label_set_text(detBody, "Select a grid cell first.");
    return;
  }
  // Keep the selected record fresh from the live (already salted) table.
  static det::Entry e[det::kMax];
  const int n = det::snapshot(e, det::kMax);
  for (int i = 0; i < n; i++) if (e[i].id == selEnt.id) { selEnt = e[i]; break; }
  char id[9], buf[256];
  privacy::idToStr(selEnt.id, id);
  const int slot = (int)(mix(selEnt.id) % 80);
  const char cell[3] = {(char)('A' + slot % 10), (char)('1' + slot / 10), 0};
  const char *kind = selEnt.kind == det::DRONE ? "DRONE" : selEnt.kind == det::BLE ? "BLE" : "WIFI";
  snprintf(buf, sizeof buf,
           "TYPE    %s\nCELL    %s\nRSSI    %d dBm\nID      %s\nCHAN    %u\nSEEN    %lu s ago",
           kind, cell, selEnt.rssi, id, selEnt.channel,
           (unsigned long)((nowMs - selEnt.lastSeen) / 1000));
  lv_label_set_text(detBody, buf);
  if (!histLast || nowMs - histLast >= 1000) {
    histLast = nowMs;
    if (histN < 16) hist[histN++] = selEnt.rssi;
    else { memmove(hist, hist + 1, 15); hist[15] = selEnt.rssi; }
  }
  for (int i = 0; i < 16; i++) {
    if (i >= histN) { lv_obj_add_flag(histBar[i], LV_OBJ_FLAG_HIDDEN); continue; }
    int h = 100 + hist[i];
    if (h < 4) h = 4;
    if (h > 40) h = 40;
    lv_obj_set_height(histBar[i], h);
    lv_obj_set_y(histBar[i], 270 - h);
    lv_obj_remove_flag(histBar[i], LV_OBJ_FLAG_HIDDEN);
  }
}

// --- wardriving (signal only) ---------------------------------------------
// No route, no distance, no GPS. Radius = RSSI (stronger = closer to the
// centre); the angle is a stable hash of the (already salted) address id. It
// is NOT a direction: there is no direction finding.
uint8_t crc8(uint32_t v) {
  uint8_t c = 0;
  for (int i = 0; i < 4; i++) {
    c ^= (uint8_t)(v >> (8 * i));
    for (int b = 0; b < 8; b++) c = (uint8_t)((c & 0x80) ? (c << 1) ^ 0x07 : (c << 1));
  }
  return c;
}
void buildWardriving() {
  wardriving::start();
  radarObj = radar::create(content, 0, 0, CYD32_SCREEN_W, CYD32_SCREEN_H, swStyle(ui::WARDRIVING));
  txt("WARDRIVING", &lv_font_montserrat_14, CYD32_SONAR_G4, 76);
  for (int i = 0; i < 4; i++) {
    char t[8];
    snprintf(t, sizeof t, "%d", sw_ward_ring_dbm[i]);
    lv_obj_t *l = label(content, t, &lv_font_montserrat_12, CYD32_SONAR_G3);
    lv_obj_set_pos(l, CX + SW_WARD_RSSI_R(sw_ward_ring_dbm[i]) - 22, CY - 16);
  }
  wdCnt[0] = label(content, "AP 0000", &lv_font_montserrat_16, CYD32_SONAR_G4);
  lv_obj_set_pos(wdCnt[0], 116, 128);
  wdCnt[1] = label(content, "BLE 0000", &lv_font_montserrat_16, CYD32_SONAR_G4);
  lv_obj_set_pos(wdCnt[1], 116, 148);
  wdCnt[2] = label(content, "RID 0000", &lv_font_montserrat_16, CYD32_SONAR_DRONE);
  lv_obj_set_pos(wdCnt[2], 276, 128);
  // REC indicator: G4 ring + dot (not yellow, not red).
  lv_obj_t *ring = box(content, 147, 318, 13, 13, CYD32_SONAR_BG_SW);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(ring, C(CYD32_SONAR_G4), 0);
  lv_obj_set_style_border_width(ring, 1, 0);
  wdRecDot = box(content, 151, 322, 5, 5, CYD32_SONAR_G4);
  lv_obj_set_style_radius(wdRecDot, LV_RADIUS_CIRCLE, 0);
  wdRecLbl = label(content, "REC", &lv_font_montserrat_14, CYD32_SONAR_G4);
  lv_obj_set_pos(wdRecLbl, 167, 316);
  wdTimerLbl = label(content, "00:00:00", &lv_font_montserrat_14, CYD32_SONAR_G4);
  lv_obj_set_pos(wdTimerLbl, 210, 316);
  txt("LOGGING (RAM)", &lv_font_montserrat_14, CYD32_SONAR_G3, 340);
  txt("RSSI DBM  NO GPS", &lv_font_montserrat_12, CYD32_SONAR_G2, 362);
}
void refreshWardriving() {
  if (!radarObj) return;
  const int n = wardriving::count();
  const wardriving::Rec *rec = wardriving::records();
  static radar::Contact rc[radar::kMaxContacts];
  const int show = n < radar::kMaxContacts ? n : radar::kMaxContacts;
  int sel = -1;
  for (int i = 0; i < show; i++) {
    const wardriving::Rec &r = rec[n - show + i];
    radar::Contact &c = rc[i];
    memset(&c, 0, sizeof c);
    int rad = SW_WARD_RSSI_R(r.rssi);
    if (rad < 8) rad = 8;
    if (rad > 160) rad = 160;
    radar::polar(rad, (int)crc8(r.id) * 360 / 256, c.dx, c.dy);
    c.kind = r.kind == det::DRONE ? radar::DRONE : r.kind == det::BLE ? radar::BLE : radar::WIFI;
    c.r = c.kind == radar::DRONE ? 4 : c.kind == radar::BLE ? 2 : 3;
    c.seed = mix(r.id);
    if (c.kind == radar::DRONE && (sel < 0 || r.rssi > rec[n - show + sel].rssi)) sel = i;
  }
  if (sel >= 0) snprintf(rc[sel].id, sizeof rc[sel].id, "RID %d", rec[n - show + sel].rssi);
  radar::setContacts(radarObj, rc, show, sel);
  char t[20];
  const int ap = wardriving::countKind(det::WIFI);
  const int ble = wardriving::countKind(det::BLE);
  const int rid = wardriving::countKind(det::DRONE);
  snprintf(t, sizeof t, "AP %04d", LV_MIN(ap, 9999)); lv_label_set_text(wdCnt[0], t);
  snprintf(t, sizeof t, "BLE %04d", LV_MIN(ble, 9999)); lv_label_set_text(wdCnt[1], t);
  snprintf(t, sizeof t, "RID %04d", LV_MIN(rid, 9999)); lv_label_set_text(wdCnt[2], t);
  const uint32_t ms = wardriving::sessionMs();
  const uint32_t s = ms / 1000;
  snprintf(t, sizeof t, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  lv_label_set_text(wdTimerLbl, t);
  const bool on = wardriving::active();
  lv_label_set_text(wdRecLbl, on ? "REC" : "---");
  if (on) lv_obj_remove_flag(wdRecDot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(wdRecDot, LV_OBJ_FLAG_HIDDEN);
}

// --- OTA / update / unsigned ---------------------------------------------
void onStartUpdate(lv_event_t *) { updateReq = true; }
void buildUpdateConfirm() {
  label(content, "OTA UPDATE", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_t *s = label(content,
                      "Update mode stops scanning and opens a\n"
                      "temporary WPA2 hotspot for 10 minutes.\n\n"
                      "1. Scan the QR code with the iPhone\n"
                      "   camera to join the hotspot.\n"
                      "2. Open http://192.168.4.1 in Safari.\n"
                      "3. Upload the signed .bin file.",
                      &lv_font_unscii_8, COL_TEXT);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s, LV_ALIGN_CENTER, 0, -20);
  button(content, "Start update mode", CX - 100, 286, 200, 48, onStartUpdate, nullptr, COL_OK);
}
void buildOta() {
  otaConfirm = nullptr;
  label(content, "OTA UPDATE", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  otaQr = lv_qrcode_create(content);
  lv_qrcode_set_size(otaQr, 140);
  lv_qrcode_set_dark_color(otaQr, lv_color_black());
  lv_qrcode_set_light_color(otaQr, lv_color_white());
  lv_obj_align(otaQr, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_border_color(otaQr, lv_color_white(), 0);
  lv_obj_set_style_border_width(otaQr, 4, 0);
  const ota_portal::State &st0 = ota_portal::state();
  if (st0.qr[0]) lv_qrcode_update(otaQr, st0.qr, strlen(st0.qr));
  for (int i = 0; i < 17; i++) {
    const float a = (-90.f + i * (360.f / 17.f)) * PI / 180.f;
    const int x = CX + (int)(150.f * cosf(a)) - 8;
    const int y = CY + (int)(150.f * sinf(a)) - 16;
    otaSeg[i] = box(content, x, y, 16, 32, COL_DIM);
    lv_obj_set_style_radius(otaSeg[i], 3, 0);
    lv_obj_add_flag(otaSeg[i], LV_OBJ_FLAG_HIDDEN);
  }
  otaPct = label(content, "", &lv_font_montserrat_24, COL_YEL);
  lv_obj_align(otaPct, LV_ALIGN_CENTER, 0, -8);
  otaInfo = label(content, "", &lv_font_unscii_8, COL_TEXT);
  lv_obj_set_width(otaInfo, 300);
  lv_obj_set_style_text_align(otaInfo, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(otaInfo, LV_ALIGN_CENTER, 0, 40);
  otaMsg = label(content, "", &lv_font_unscii_8, COL_CYAN);
  lv_obj_set_width(otaMsg, 300);
  lv_obj_set_style_text_align(otaMsg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(otaMsg, LV_ALIGN_CENTER, 0, 110);
}
void onDowngrade(lv_event_t *e) { ota_portal::confirmDowngrade(lv_event_get_user_data(e) != nullptr); }
void refreshOta() {
  const ota_portal::State &st = ota_portal::state();
  const bool ask = st.phase == ota_portal::AWAIT_CONFIRM;
  if (ask && !otaConfirm) {
    otaConfirm = box(content, 40, 70, 386, 326, COL_BG);
    lv_obj_set_style_radius(otaConfirm, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *t = label(otaConfirm, "DOWNGRADE", &lv_font_montserrat_16, COL_HIT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_t *m = label(otaConfirm, st.msg, &lv_font_unscii_8, COL_TEXT);
    lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 90);
    button(otaConfirm, "Cancel", 50, 200, 130, 48, onDowngrade, nullptr, COL_OK);
    button(otaConfirm, "Install", 206, 200, 130, 48, onDowngrade, (void *)1, COL_HIT);
  } else if (!ask && otaConfirm) {
    lv_obj_delete(otaConfirm); otaConfirm = nullptr;
  }
  const bool waiting = st.phase == ota_portal::WAITING || st.phase == ota_portal::IDLE;
  if (waiting) {
    lv_obj_remove_flag(otaQr, LV_OBJ_FLAG_HIDDEN);
    for (auto *s : otaSeg) lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(otaPct, "");
    char t[200];
    snprintf(t, sizeof t, "SSID %s\nPASS %s\n\nhttp://192.168.4.1\n%lu:%02lu left\n%s",
             st.ssid, st.pass, (unsigned long)st.remainingS / 60, (unsigned long)st.remainingS % 60,
             st.unsignedArmed ? "UNSIGNED ARMED" : "signed only");
    lv_label_set_text(otaInfo, t);
  } else {
    lv_obj_add_flag(otaQr, LV_OBJ_FLAG_HIDDEN);
    const int pct = st.expected ? (int)(100.0 * st.received / st.expected) : 0;
    const int lit = pct * 17 / 100;
    for (int i = 0; i < 17; i++) {
      lv_obj_remove_flag(otaSeg[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(otaSeg[i], C(i < lit ? COL_YEL : COL_DIM), 0);
    }
    char p[8]; snprintf(p, sizeof p, "%d%%", pct);
    lv_label_set_text(otaPct, p);
    char t[240];
    const char *dl = st.received ? "OK" : "--";
    const char *sh = (st.phase == ota_portal::SUCCESS || st.phase == ota_portal::AWAIT_CONFIRM) ? "OK" :
                     (st.phase == ota_portal::FAILED ? "FAIL" : (st.phase == ota_portal::VERIFYING ? ".." : "--"));
    const char *rb = st.phase == ota_portal::SUCCESS ? "OK" : "--";
    snprintf(t, sizeof t,
             "%s\nAPP SLOT 6.25 MB\n\nDOWNLOAD ........ %s\nSHA-256 ......... %s\nFLASH WRITE ..... %d%%\nREBOOT .......... %s\n\nDO NOT UNPLUG",
             CYD32_VERSION, dl, sh, pct, rb);
    lv_label_set_text(otaInfo, t);
  }
  lv_label_set_text(otaMsg, st.msg);
  lv_obj_set_style_text_color(otaMsg, C(st.phase == ota_portal::FAILED ? COL_HIT :
                                        st.phase == ota_portal::SUCCESS ? COL_OK : COL_CYAN), 0);
}
void onUnsigned(lv_event_t *e) { unsignedDecision = (int)(intptr_t)lv_event_get_user_data(e); }
void buildUnsignedConfirm() {
  label(content, "UNSIGNED MODE", &lv_font_unscii_8, COL_HIT);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_t *s = label(content, CYD32_UNSIGNED_BODY, &lv_font_unscii_8, COL_TEXT);
  lv_obj_set_width(s, 320);
  lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(s, LV_ALIGN_CENTER, 0, -10);
  button(content, "Cancel", CX - 150, 320, 140, 48, onUnsigned, (void *)0, COL_OK);
  button(content, "Arm unsigned", CX + 10, 320, 140, 48, onUnsigned, (void *)1, COL_HIT);
}

// --- creature -------------------------------------------------------------
float backEase(float t) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  const float s = 1.70158f;
  t -= 1.f;
  return t * t * ((s + 1.f) * t + s) + 1.f;
}
void i4Roll(uint8_t *buf, int8_t dx, int8_t dy) {
  if (dy) {
    uint8_t *tmp = rgb64;
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
    if (p + 4 > end) return;
    const uint8_t x = p[0], y = p[1], w = p[2], h = p[3];
    p += 4;
    const size_t nbytes = (size_t)((w + 1) / 2) * (size_t)h;
    if (p + nbytes > end) return;
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
  uint16_t *rgb = (uint16_t *)rgb64;
  uint8_t *alpha = rgb64 + kSpr * kSpr * 2;
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
void nnScale3() {
  const uint16_t *srgb = (const uint16_t *)rgb64;
  const uint8_t *sa = rgb64 + kSpr * kSpr * 2;
  uint16_t *drgb = (uint16_t *)rgb192;
  uint8_t *da = rgb192 + kBig * kBig * 2;
  for (int y = 0; y < kSpr; y++) {
    for (int x = 0; x < kSpr; x++) {
      const uint16_t p = srgb[y * kSpr + x];
      const uint8_t a = sa[y * kSpr + x];
      for (int dy = 0; dy < kNN; dy++) {
        for (int dx = 0; dx < kNN; dx++) {
          const int o = (y * kNN + dy) * kBig + (x * kNN + dx);
          drgb[o] = p;
          da[o] = a;
        }
      }
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
  if (!i4Buf || !rgb64 || !rgb192 || !anim || !creaImg) return;
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
  nnScale3();
  rgbDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  rgbDsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
  rgbDsc.header.w = kBig;
  rgbDsc.header.h = kBig;
  rgbDsc.header.stride = kBig * 2;
  rgbDsc.data_size = kRgb192;
  rgbDsc.data = rgb192;
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
  lv_image_set_pivot(creaImg, kBig / 2, kBig);
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
  if (creature::cthulhuStage() >= 1) return;
  playingHappy = true;
  setAnim(creature::size() <= 1 ? curSpec->baby_happy : curSpec->adult_happy, 0);
}
void animTick(lv_timer_t *t) {
  nowMs = lv_tick_get();
  if (popping && popStart && nowMs - popStart >= kPopMs) { popping = 0; popStart = 0; }
  const bool eggHold = playingHatch && (int32_t)(nowMs - eggHoldUntil) < 0;
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
void allocCreature() {
  if (pool) return;
  const size_t need = kI4Size * 2 + kRgb64 + kRgb192;
  pool = (uint8_t *)heap_caps_malloc(need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pool) pool = (uint8_t *)malloc(need);
  if (pool) {
    i4Buf = pool;
    i4Draw = pool + kI4Size;
    rgb64 = pool + kI4Size * 2;
    rgb192 = rgb64 + kRgb64;
  }
}
void freeCreature() {
  if (animTimer) { lv_timer_delete(animTimer); animTimer = nullptr; }
  if (pool) {
    lv_image_cache_drop(&rgbDsc);
    free(pool);
    pool = i4Buf = i4Draw = rgb64 = rgb192 = nullptr;
  }
  anim = nullptr; curSpec = nullptr; playingHappy = playingHatch = false;
}
void buildCreature() {
  curSpec = creature::spec();
  char title[40];
  snprintf(title, sizeof title, "CREATURE // %s", creature::name());
  label(content, title, &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  creaImg = lv_image_create(content);
  lv_obj_align(creaImg, LV_ALIGN_CENTER, 0, -40);
  allocCreature();
  memset(&rgbDsc, 0, sizeof rgbDsc);
  playingHappy = false;
  reactReq = false;
  if (creature::consumeHatched()) {
    playingHatch = true;
    setAnim(curSpec->hatch, 0);
    eggHoldUntil = lv_tick_get() + kEggIdleMs;
  } else {
    startIdle();
  }
  compose();
  creaInfo = label(content, "", &lv_font_unscii_8, COL_TEXT);
  lv_obj_set_width(creaInfo, 360);
  lv_obj_set_style_text_align(creaInfo, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(creaInfo, LV_ALIGN_CENTER, 0, 84);
  // Left / right of the info block, clear of the curved tab band.
  button(content, "PET", CX - 176, 250, 72, 48, creaAction, (void *)1, COL_CYAN);
  button(content, "FEED", CX + 104, 250, 72, 48, creaAction, (void *)2, COL_CYAN);
  flash = box(content, 0, 0, CYD32_SCREEN_W, CYD32_SCREEN_H, COL_TEXT);
  lv_obj_add_flag(flash, LV_OBJ_FLAG_HIDDEN);
  lastUniques = creature::uniqueCount();
  growFrom = growTo = currentZoom();
  animTimer = lv_timer_create(animTick, 80, nullptr);
}
void refreshCreature() {
  const uint8_t s = creature::size();
  const uint8_t cth = creature::cthulhuStage();
  const uint32_t next = creature::nextSizeAt();
  const uint32_t uniq = creature::uniqueCount();
  const uint32_t drones = creature::droneCount();
  char t[192];
  snprintf(t, sizeof t, "SIZE %u   CTHULHU %u/5\nUNIQUES %lu   DRONES %lu/%s%lu\nXP %lu",
           s, cth, (unsigned long)uniq, (unsigned long)drones,
           next ? "" : "max ", (unsigned long)(next ? next : drones),
           (unsigned long)creature::xp());
  lv_label_set_text(creaInfo, t);
  if (uniq > lastUniques) { lastUniques = uniq; reactReq = true; }
  if (creature::consumeGrew()) {
    growFrom = growTo;
    growTo = CYD32_SIZE_ZOOM[s ? s - 1 : 0];
    growStart = nowMs ? nowMs : lv_tick_get();
    if (!playingHatch) startIdle();
  }
  if (creature::consumeCthulhuUp()) {
    if (!playingHatch) startIdle();
    ui::show(ui::EVOLUTION);
    return;
  }
  const uint8_t fresh = creature::consumeNewTraits();
  if (fresh) { popping = fresh; popStart = nowMs ? nowMs : lv_tick_get(); }
}

lv_point_precise_t evoRay[12][2];
void buildEvolution() {
  label(content, "EVOLVING!", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  for (int i = 0; i < 12; i++) {
    const float a = i * (2 * PI / 12);
    evoRay[i][0] = {(lv_value_precise_t)CX, (lv_value_precise_t)CY};
    evoRay[i][1] = {(lv_value_precise_t)(CX + 200 * cosf(a)), (lv_value_precise_t)(CY + 200 * sinf(a))};
    lv_obj_t *ln = lv_line_create(content);
    lv_line_set_points(ln, evoRay[i], 2);
    lv_obj_set_style_line_color(ln, C(i & 1 ? COL_YEL : COL_DIM), 0);
    lv_obj_set_style_line_width(ln, 10, 0);
  }
  // Reuse the creature compositor for the current form, shown large.
  curSpec = creature::spec();
  creaImg = lv_image_create(content);
  lv_obj_align(creaImg, LV_ALIGN_CENTER, 40, 0);
  allocCreature();
  startIdle();
  compose();
  char t[48];
  snprintf(t, sizeof t, "DRONE #%lu: MUTATION", (unsigned long)creature::droneCount());
  lv_obj_t *boxl = box(content, 80, 360, 306, 36, COL_BG);
  lv_obj_set_style_border_color(boxl, C(COL_CYAN), 0);
  lv_obj_set_style_border_width(boxl, 2, 0);
  lv_obj_t *l = label(boxl, t, &lv_font_unscii_8, COL_CYAN);
  lv_obj_center(l);
  animTimer = lv_timer_create(animTick, 80, nullptr);
}

// --- menu / counters / about / torpedo -----------------------------------
static const ui::Screen kWedge[6] = {
    ui::SONAR, ui::GRID, ui::CREATURE, ui::WARDRIVING, ui::UPDATE_CONFIRM, ui::COUNTERS};
static const char *kWedgeName[6] = {"SONAR", "GRID", "CREATURE", "WARDRIVE", "OTA", "COUNTERS"};

int wedgeAt(int x, int y) {
  const int dx = x - CX, dy = y - CY;
  const int r2 = dx * dx + dy * dy;
  if (r2 < 70 * 70 || r2 > 160 * 160) return -1;
  float deg = atan2f((float)dy, (float)dx) * 180.f / PI;
  float fromTop = deg + 90.f;
  if (fromTop < 0) fromTop += 360.f;
  int w = (int)((fromTop + 30.f) / 60.f);
  return w % 6;
}
void onMenuTouch(lv_event_t *) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  const int w = wedgeAt(p.x, p.y);
  if (w < 0) return;
  menuSel = w;
  ui::show(kWedge[w]);
}
void buildMenu() {
  label(content, "MENU", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  // Six 60 deg annulus wedges. LVGL 0 deg = 3 o'clock, clockwise.
  // SONAR is centred at the top (270 deg).
  const int starts[6] = {240, 300, 0, 60, 120, 180};
  for (int i = 0; i < 6; i++) {
    lv_obj_t *a = mkArc(content, CX - 150, CY - 150, 300, starts[i], starts[i] + 60,
                        i == menuSel ? COL_YEL : COL_DIM, i == menuSel ? COL_YEL : COL_DIM, 80);
    if (i == menuSel) lv_arc_set_value(a, 100);
    const float mid = (starts[i] + 30) * PI / 180.f;
    const int lx = CX + (int)(115.f * cosf(mid)) - 36;
    const int ly = CY + (int)(115.f * sinf(mid)) - 6;
    lv_obj_t *l = label(content, kWedgeName[i], &lv_font_unscii_8, i == menuSel ? COL_BG : COL_YEL);
    // Selected wedge is filled yellow; draw its label in black-ish via dim+bg.
    lv_obj_set_style_text_color(l, C(i == menuSel ? COL_BG : COL_YEL), 0);
    lv_obj_set_pos(l, lx, ly);
  }
  lv_obj_t *hub = box(content, CX - 70, CY - 70, 140, 140, COL_BG);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(hub, C(COL_YEL), 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_t *hm = label(hub, "MENU", &lv_font_montserrat_16, COL_YEL);
  lv_obj_align(hm, LV_ALIGN_CENTER, 0, -16);
  lv_obj_t *hb = label(hub, "B: OK", &lv_font_unscii_8, COL_CYAN);
  lv_obj_align(hb, LV_ALIGN_CENTER, 0, 8);
  lv_obj_t *ha = label(hub, "A: BACK", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(ha, LV_ALIGN_CENTER, 0, 24);
  lv_obj_add_flag(content, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(content, onMenuTouch, LV_EVENT_PRESSED, nullptr);
}

void onSig(lv_event_t *) { ui::show(ui::ABOUT); }
void buildCounters() {
  label(content, "COUNTERS W / B / R", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 40);
  cntArc[0] = mkArc(content, CX - 140, CY - 140, 280, 150, 270, COL_TRACK, COL_CYAN, 14);
  cntArc[1] = mkArc(content, CX - 140, CY - 140, 280, 270, 30, COL_TRACK, COL_YEL, 14);
  cntArc[2] = mkArc(content, CX - 140, CY - 140, 280, 30, 150, COL_TRACK, COL_DRONE, 14);
  cntNum[0] = label(content, "0000", &lv_font_montserrat_24, COL_CYAN);
  lv_obj_set_pos(cntNum[0], 118, 190);  // gauges at r 126-140, clear of the tab band
  label(content, "AP", &lv_font_unscii_8, COL_MISS);
  lv_obj_set_pos(lv_obj_get_child(content, -1), 138, 222);
  cntNum[1] = label(content, "0000", &lv_font_montserrat_24, COL_YEL);
  lv_obj_set_pos(cntNum[1], 288, 190);
  label(content, "BEACONS", &lv_font_unscii_8, COL_MISS);
  lv_obj_set_pos(lv_obj_get_child(content, -1), 286, 222);
  cntNum[2] = label(content, "0000", &lv_font_montserrat_24, COL_DRONE);
  lv_obj_align(cntNum[2], LV_ALIGN_CENTER, 0, 80);
  label(content, "DRONES", &lv_font_unscii_8, COL_MISS);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_CENTER, 0, 108);
  lv_obj_t *hub = box(content, CX - 40, CY - 40, 80, 80, COL_BG);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(hub, C(COL_YEL), 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_add_flag(hub, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(hub, onSig, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *sg = label(hub, "SIG", &lv_font_unscii_8, COL_YEL);
  lv_obj_center(sg);
  label(content, "WIFI", &lv_font_unscii_8, COL_CYAN);
  lv_obj_set_pos(lv_obj_get_child(content, -1), 70, 150);
  label(content, "BLE", &lv_font_unscii_8, COL_YEL);
  lv_obj_set_pos(lv_obj_get_child(content, -1), 360, 150);
  label(content, "RID", &lv_font_unscii_8, COL_DRONE);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_CENTER, 0, 56);
}
void refreshCounters() {
  const det::Counts c = det::counts();
  char t[8];
  snprintf(t, sizeof t, "%04u", (unsigned)c.wifi); lv_label_set_text(cntNum[0], t);
  snprintf(t, sizeof t, "%04u", (unsigned)c.ble); lv_label_set_text(cntNum[1], t);
  snprintf(t, sizeof t, "%04u", (unsigned)c.drone); lv_label_set_text(cntNum[2], t);
  auto pct = [](unsigned v) { return v > 40 ? 100 : (int)(v * 100 / 40); };
  lv_arc_set_value(cntArc[0], pct(c.wifi));
  lv_arc_set_value(cntArc[1], pct(c.ble));
  lv_arc_set_value(cntArc[2], pct(c.drone));
}

void buildAbout() {
  label(content, "ABOUT", &lv_font_unscii_8, COL_YEL);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 48);
  char t[480];
  snprintf(t, sizeof t,
           "CYD32 - passive RF sonar\n(receive only)\n\n"
           "Version  %s\nCommit   %s\nLicence  %s\nTarget   %s\n"
           "Status   %s\nKey      %s\n\n%s\n\n"
           "ABSOLUTELY NO WARRANTY.",
           CYD32_VERSION, CYD32_COMMIT, CYD32_LICENSE, CYD32_TARGET,
           fw_status::kindStr(), fw_status::keyFingerprint(), CYD32_SOURCE_URL);
  lv_obj_t *l = label(content, t, &lv_font_unscii_8, COL_TEXT);
  lv_obj_set_width(l, 340);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, 10);
}
void buildTorpedo() {
  label(content, "TORPEDO!", &lv_font_unscii_8, COL_CYAN);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 48);
  label(content, "CODE A1 B2 C3 D4 <3S", &lv_font_unscii_8, COL_CYAN);
  lv_obj_align(lv_obj_get_child(content, -1), LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_t *s = label(content, "SUNK!", &lv_font_montserrat_24, COL_HIT);
  lv_obj_align(s, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t *x = label(content, "+50 XP", &lv_font_montserrat_16, COL_YEL);
  lv_obj_align(x, LV_ALIGN_CENTER, 0, 0);
  lv_obj_t *ship = box(content, CX - 70, 300, 140, 50, COL_GRID);
  lv_obj_set_style_radius(ship, 4, 0);
}

void goBack() {
  if (cur == ui::BOOT || cur == ui::OTA) return;
  if (cur == ui::UNSIGNED_CONFIRM) { unsignedDecision = 0; return; }
  if (cur == ui::HOME) return;
  if (cur == ui::EVOLUTION) { ui::show(ui::CREATURE); return; }
  if (cur == ui::TORPEDO) { ui::show(ui::GRID); return; }
  ui::show(prevSc == cur ? ui::HOME : prevSc);
}
void doSelect() {
  switch (cur) {
    case ui::MENU: ui::show(kWedge[menuSel]); break;
    case ui::UPDATE_CONFIRM: updateReq = true; break;
    case ui::UNSIGNED_CONFIRM: unsignedDecision = 0; break;  // default Cancel
    case ui::GRID: if (haveSel) ui::show(ui::DETAILS); break;
    case ui::DETAILS: onTrack(nullptr); break;
    case ui::COUNTERS: ui::show(ui::ABOUT); break;
    case ui::EVOLUTION: ui::show(ui::CREATURE); break;
    case ui::TORPEDO: ui::show(ui::GRID); break;
    case ui::ABOUT: ui::show(prevSc == cur ? ui::COUNTERS : prevSc); break;
    default: break;
  }
}

void clearContent() {
  freeCreature();
  lv_obj_clean(content);
  if (fxTimer) { lv_timer_delete(fxTimer); fxTimer = nullptr; }
  radarObj = bootLogo = bootTitle = scanLine = bootLoad = nullptr;
  hudW = hudB = hudR = hudSel = hudSel2 = wdRecDot = nullptr;
  gridPlate = gridPlateLbl = detBody = nullptr;
  otaQr = otaInfo = otaMsg = otaPct = otaConfirm = nullptr;
  wdTimerLbl = wdRecLbl = creaImg = creaInfo = flash = nullptr;
  memset(bootMsg, 0, sizeof bootMsg);
}
}  // namespace

namespace ui {
void begin() {
  buildChrome();
  if (!btnTimer) btnTimer = lv_timer_create(btnTick, 50, nullptr);
  show(BOOT);
}
void show(Screen s) {
  if (cur != s && cur != BOOT) prevSc = cur;
  clearContent();
  cur = s;
  phosphor = s == BOOT || s == SONAR || s == WARDRIVING;
  // Curved 4-tab band on every screen except boot, OTA and the unsigned confirm.
  const bool tabs = s != BOOT && s != OTA && s != UNSIGNED_CONFIRM;
  setTabs(tabs);
  styleChrome();
  switch (s) {
    case BOOT: buildBoot(); break;
    case SONAR: buildSonar(); break;
    case GRID: buildGrid(); break;
    case CREATURE: buildCreature(); break;
    case MENU: buildMenu(); break;
    case DETAILS: buildDetails(); break;
    case WARDRIVING: buildWardriving(); break;
    case UPDATE_CONFIRM: buildUpdateConfirm(); break;
    case OTA: buildOta(); break;
    case ABOUT: buildAbout(); break;
    case TORPEDO: buildTorpedo(); break;
    case UNSIGNED_CONFIRM: buildUnsignedConfirm(); break;
    case COUNTERS: buildCounters(); break;
    case EVOLUTION: buildEvolution(); break;
    case TRACKERS:
    case DRONES:
      buildAbout();
      break;
  }
  refresh();
}
void refresh() {
  nowMs = lv_tick_get();
  refreshStatus();
  switch (cur) {
    case SONAR: refreshSonar(); break;
    case GRID: refreshGrid(); break;
    case CREATURE: refreshCreature(); break;
    case DETAILS: refreshDetails(); break;
    case WARDRIVING: refreshWardriving(); break;
    case OTA: refreshOta(); break;
    case COUNTERS: refreshCounters(); break;
    default: break;
  }
}
bool takeUpdateRequest() { bool r = updateReq; updateReq = false; return r; }
int takeUnsignedDecision() { int d = unsignedDecision; unsignedDecision = -1; return d; }
void setUnsignedBanner(bool on) { unsignedBanner = on; refreshStatus(); }
void setBootProgress(int pct, const char *msg) {
  if (cur != BOOT || !radarObj) return;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  radar::setProgress(radarObj, pct);
  char t[16];
  snprintf(t, sizeof t, "LOAD %d%%", pct);
  if (bootLoad) lv_label_set_text(bootLoad, t);
  // Light BLE / WIFI / RID as the self-test advances ("BLE OK", "WIFI OK", "RID OK").
  const int step = msg && msg[0] == 'W' ? 1 : msg && msg[0] == 'R' ? 2 : msg && msg[0] == 'B' ? 0 : -1;
  static const char *ok[3] = {"BLE OK", "WIFI OK", "RID OK"};
  for (int i = 0; i < 3; i++) {
    if (!bootMsg[i] || i > step) continue;
    lv_label_set_text(bootMsg[i], ok[i]);
    lv_obj_set_style_text_color(bootMsg[i], C(CYD32_SONAR_G4), 0);
  }
}
}
#endif
