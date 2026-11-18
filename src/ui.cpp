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
#include "fw_status.h"
#include "ota_portal.h"
#include "privacy.h"
#include "ui_layout.h"

namespace {
lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }
constexpr int kTop = CYD32_HEADER_H, kTabY = CYD32_TAB_Y, kContentH = CYD32_CONTENT_H;

lv_obj_t *scr, *header, *headerLbl, *statusLbl, *banner, *content, *tabbar;
ui::Screen cur = ui::BOOT;
bool updateReq = false, unsignedBanner = false;
int unsignedDecision = -1;
// per-screen widgets
lv_obj_t *bootBar, *bootMsg;
lv_obj_t *sweep, *blips[24], *sonarCnt;
lv_point_precise_t sweepPts[2];
float sweepAngle = 0;
lv_obj_t *gridMarks[80], *gridLog;
lv_obj_t *creaImg, *creaInfo, *creaBar, *flash;
lv_obj_t *detList;
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

void buildChrome() {
  scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, C(CYD32_COL_BG), 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  header = box(scr, CYD32_HEADER_X, CYD32_HEADER_Y, CYD32_HEADER_W, CYD32_HEADER_H, CYD32_COL_GRID);
  headerLbl = label(header, "", &lv_font_unscii_8, CYD32_COL_AMBER);
  lv_obj_align(headerLbl, LV_ALIGN_LEFT_MID, 4, 0);
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
  for (int i = 0; i < 4; i++) button(tabbar, tabs[i], i * CYD32_TAB_BTN_W, 0, CYD32_TAB_BTN_W, CYD32_TAB_H, onShow, (void *)(intptr_t)ts[i]);
}

void buildBoot() {
  lv_obj_t *t = label(content, "CYD32", &lv_font_montserrat_24, CYD32_COL_AMBER);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_t *s = label(content, "passive RF sonar - " CYD32_VERSION, &lv_font_montserrat_12, CYD32_COL_WATER);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 62);
  bootBar = lv_bar_create(content);
  lv_obj_set_size(bootBar, 200, 10); lv_obj_align(bootBar, LV_ALIGN_TOP_MID, 0, 95);
  lv_obj_set_style_bg_color(bootBar, C(CYD32_COL_GRID), 0);
  lv_obj_set_style_bg_color(bootBar, C(CYD32_COL_CYAN), LV_PART_INDICATOR);
  bootMsg = label(content, "", &lv_font_unscii_8, CYD32_COL_OK);
  lv_obj_align(bootMsg, LV_ALIGN_TOP_MID, 0, 115);
  lv_obj_t *h = label(content, CYD32_HOLD_HINT, &lv_font_unscii_8, CYD32_COL_WATER);
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -6);
}

uint32_t mix(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; return x; }

void buildSonar() {
  const int d = 176, cx = CYD32_CONTENT_W / 2, cy = kContentH / 2;
  for (int r = 1; r <= 3; r++) {
    lv_obj_t *c = box(content, cx - d * r / 6, cy - d * r / 6, d * r / 3, d * r / 3, CYD32_COL_BG);
    lv_obj_set_style_bg_opa(c, r == 3 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(c, C(r == 3 ? CYD32_COL_AMBER : CYD32_COL_GRID), 0);
    lv_obj_set_style_border_width(c, r == 3 ? 2 : 1, 0);
  }
  for (int i = 0; i < 10; i++) {
    char s[2] = {(char)('A' + i), 0};
    lv_obj_t *l = label(content, s, &lv_font_unscii_8, CYD32_COL_WATER);
    lv_obj_set_pos(l, cx - d / 2 + 4 + i * (d - 8) / 10 + 4, cy - 4);
  }
  sweep = lv_line_create(content);
  lv_obj_set_style_line_color(sweep, C(CYD32_COL_OK), 0);
  lv_obj_set_style_line_width(sweep, 2, 0);
  for (auto &b : blips) {
    b = box(content, 0, 0, 6, 6, CYD32_COL_CYAN);
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
  }
  sonarCnt = label(content, "", &lv_font_unscii_8, CYD32_COL_AMBER);
  lv_obj_set_pos(sonarCnt, 4, 4);
}
void refreshSonar() {
  const int cx = CYD32_CONTENT_W / 2, cy = kContentH / 2, R = 86;
  sweepPts[0] = {cx, cy};
  sweepPts[1] = {(lv_value_precise_t)(cx + R * cosf(sweepAngle)), (lv_value_precise_t)(cy + R * sinf(sweepAngle))};
  lv_line_set_points(sweep, sweepPts, 2);
  sweepAngle += 0.35f; if (sweepAngle > 6.2832f) sweepAngle -= 6.2832f;
  static det::Entry e[24];
  const int n = det::snapshot(e, 24);
  for (int i = 0; i < 24; i++) {
    if (i >= n) { lv_obj_add_flag(blips[i], LV_OBJ_FLAG_HIDDEN); continue; }
    const float a = (mix(e[i].id) % 6283) / 1000.0f;
    int rr = 100 + e[i].rssi;  if (rr < 0) rr = 0; if (rr > 70) rr = 70;  // strong = near centre
    const float dist = R - 6 - rr * (R - 12) / 70.0f;
    const bool drone = e[i].kind == det::DRONE;
    lv_obj_set_pos(blips[i], cx + dist * cosf(a) - 3, cy + dist * sinf(a) - 3);
    lv_obj_set_style_bg_color(blips[i], C(e[i].kind == det::BLE ? CYD32_COL_CYAN : CYD32_COL_AMBER), 0);
    lv_obj_set_style_bg_opa(blips[i], drone ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_radius(blips[i], drone ? LV_RADIUS_CIRCLE : 0, 0);
    lv_obj_set_style_border_width(blips[i], drone ? 2 : 0, 0);
    lv_obj_set_style_border_color(blips[i], C(CYD32_COL_HIT), 0);
    lv_obj_remove_flag(blips[i], LV_OBJ_FLAG_HIDDEN);
  }
  const det::Counts c = det::counts();
  char t[64]; snprintf(t, sizeof t, "WIFI %u\nBLE  %u\nRID  %u", c.wifi, c.ble, c.drone);
  lv_label_set_text(sonarCnt, t);
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
void onGridTouch(lv_event_t *e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t a; lv_obj_get_coords((lv_obj_t *)lv_event_get_target(e), &a);
  const int c = (p.x - a.x1) / kCw, r = (p.y - a.y1) / kCh;
  if (c < 0 || c > 9 || r < 0 || r > 7) return;
  char cell[3] = {(char)('A' + c), (char)('1' + r), 0};
  eggStep(cell);
}
void buildGrid() {
  for (int c = 0; c < 10; c++) { char s[2] = {(char)('A' + c), 0}; lv_obj_set_pos(label(content, s, &lv_font_unscii_8, CYD32_COL_WATER), kGx + c * kCw + 8, 2); }
  lv_obj_t *g = box(content, kGx, kGy, 10 * kCw, 8 * kCh, CYD32_COL_PANEL);
  lv_obj_set_style_border_color(g, C(CYD32_COL_GRID), 0); lv_obj_set_style_border_width(g, 1, 0);
  lv_obj_add_flag(g, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g, onGridTouch, LV_EVENT_PRESSED, nullptr);
  for (int r = 0; r < 8; r++) {
    char s[2] = {(char)('1' + r), 0};
    lv_obj_set_pos(label(content, s, &lv_font_unscii_8, CYD32_COL_WATER), 10, kGy + r * kCh + 5);
    for (int c = 0; c < 10; c++) {
      gridMarks[r * 10 + c] = label(g, ".", &lv_font_unscii_8, CYD32_COL_GRID);
      lv_obj_set_pos(gridMarks[r * 10 + c], c * kCw + 8, r * kCh + 4);
    }
  }
  gridLog = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(gridLog, 266, 12);
  lv_obj_set_width(gridLog, 56);
}
void refreshGrid() {
  static det::Entry e[det::kMax];
  const int n = det::snapshot(e, det::kMax);
  uint8_t mark[80] = {0};
  for (int i = 0; i < n; i++) { uint8_t &m = mark[mix(e[i].id) % 80]; m = e[i].kind == det::DRONE ? 2 : (m ? m : 1); }
  for (int i = 0; i < 80; i++) {
    lv_label_set_text(gridMarks[i], mark[i] ? "X" : ".");
    lv_obj_set_style_text_color(gridMarks[i], C(mark[i] == 2 ? CYD32_COL_HIT : mark[i] ? CYD32_COL_AMBER : CYD32_COL_GRID), 0);
  }
  char t[96] = "LOG\n"; char id[9];
  for (int i = 0; i < n && i < 5; i++) {
    privacy::idToStr(e[i].id, id); id[5] = 0;
    char l[16]; snprintf(l, sizeof l, "%c%s\n", e[i].kind == det::WIFI ? 'W' : e[i].kind == det::BLE ? 'B' : 'D', id);
    strncat(t, l, sizeof t - strlen(t) - 1);
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
  if (i4Buf && anim && curSpec && anim->ms && anim != curSpec->cth_keys) {
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
  if (creature::consumeHatched()) {
    playingHatch = true;
    setAnim(curSpec->hatch, 0);
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
  snprintf(t, sizeof t, "%s\nsize %u\nuniq %lu\nRID  %lu\nnext %s%lu\nCth  %u\nXP   %lu\n%s",
           creature::name(), s, (unsigned long)uniq, (unsigned long)drones,
           next ? "" : "max ", (unsigned long)next, cth, (unsigned long)creature::xp(), traits);
  lv_label_set_text(creaInfo, t);
  const uint32_t lo = creature::sizeFloor();
  int pct = next ? (int)(100 * (uniq - lo) / (next - lo)) : 100;
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  lv_bar_set_value(creaBar, pct, LV_ANIM_OFF);
  if (uniq > lastUniques) { lastUniques = uniq; reactReq = true; }
  if (creature::consumeGrew()) {
    growFrom = growTo;
    growTo = CYD32_SIZE_ZOOM[s ? s - 1 : 0];
    growStart = nowMs ? nowMs : lv_tick_get();
    if (!playingHatch) startIdle();
  }
  if (creature::consumeCthulhuUp() && !playingHatch) startIdle();
  const uint8_t fresh = creature::consumeNewTraits();
  if (fresh) { popping = fresh; popStart = nowMs ? nowMs : lv_tick_get(); }
}

void buildMenu() {
#ifdef CYD32_HW_M5STACK_STOPWATCH
  // No microSD on the StopWatch: wardriving export is compiled out.
  struct { const char *t; ui::Screen s; } items[5] = {
      {"Details", ui::DETAILS}, {"Trackers", ui::TRACKERS}, {"Drones", ui::DRONES},
      {"Update", ui::UPDATE_CONFIRM}, {"About", ui::ABOUT}};
  const int n = 5;
#else
  struct { const char *t; ui::Screen s; } items[6] = {
      {"Details", ui::DETAILS}, {"Trackers", ui::TRACKERS}, {"Drones", ui::DRONES},
      {"Wardriving", ui::WARDRIVING}, {"Update", ui::UPDATE_CONFIRM}, {"About", ui::ABOUT}};
  const int n = 6;
#endif
  for (int i = 0; i < n; i++)
    button(content, items[i].t, 10 + (i % 2) * 155, 8 + (i / 2) * 58, 145, 48, onShow, (void *)(intptr_t)items[i].s);
}
void buildDetails() {
  label(content, "VISIBLE NOW (salted ids, reset at boot)", &lv_font_unscii_8, CYD32_COL_AMBER);
  detList = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(detList, 0, 14);
}
void refreshDetails() {
  static det::Entry e[14];
  const int n = det::snapshot(e, 14);
  char t[640] = ""; char id[9];
  for (int i = 0; i < n; i++) {
    privacy::idToStr(e[i].id, id);
    char l[48];
    snprintf(l, sizeof l, "%-5s %s %4d dBm ch%-2u %lus\n", e[i].kind == det::WIFI ? "WIFI" : e[i].kind == det::BLE ? "BLE" : "RID",
             id, e[i].rssi, e[i].channel, (unsigned long)((nowMs - e[i].lastSeen) / 1000));
    strncat(t, l, sizeof t - strlen(t) - 1);
  }
  lv_label_set_text(detList, n ? t : "nothing yet...");
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
  char t[48]; snprintf(t, sizeof t, "CYD32 W%u B%u R%u", c.wifi, c.ble, c.drone);
  lv_label_set_text(headerLbl, t);
  lv_label_set_text(statusLbl, fw_status::kindStr());
  if (unsignedBanner || fw_status::kind() == fw_status::UNSIGNED) lv_obj_remove_flag(banner, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

namespace ui {
void begin() { buildChrome(); show(BOOT); }
void show(Screen s) {
  lv_obj_clean(content);
  if (animTimer) { lv_timer_delete(animTimer); animTimer = nullptr; }
  if (pool) { lv_image_cache_drop(&rgbDsc); free(pool); pool = i4Buf = i4Draw = rgbBuf = nullptr; }
  anim = nullptr; curSpec = nullptr; playingHappy = playingHatch = false;
  cur = s;
  const bool tabs = s != BOOT && s != OTA && s != UNSIGNED_CONFIRM;
  if (tabs) lv_obj_remove_flag(tabbar, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(tabbar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_height(content, tabs ? kContentH : CYD32_SCREEN_H - CYD32_HEADER_Y - CYD32_HEADER_H);
  switch (s) {
    case BOOT: buildBoot(); break;
    case SONAR: buildSonar(); break;
    case GRID: buildGrid(); break;
    case CREATURE: buildCreature(); break;
    case MENU: buildMenu(); break;
    case DETAILS: buildDetails(); break;
    case TRACKERS: buildStub("TRACKERS", "Passive detection of BLE item trackers (AirTag, SmartTag, Tile) seen following you over time."); break;
    case DRONES: buildStub("DRONES", "Remote ID beacons (ASTM F3411, Wi-Fi and BLE) are already counted on the sonar. Phase 3 decodes position and operator fields."); break;
#ifndef CYD32_HW_M5STACK_STOPWATCH
    case WARDRIVING: buildStub("WARDRIVING", "On request only: export to the local microSD card, hashed MACs, never uploaded anywhere."); break;
#endif
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
    case OTA: refreshOta(); break;
    default: break;
  }
}
bool takeUpdateRequest() { bool r = updateReq; updateReq = false; return r; }
int takeUnsignedDecision() { int d = unsignedDecision; unsignedDecision = -1; return d; }
void setUnsignedBanner(bool on) { unsignedBanner = on; refreshHeader(); }
void setBootProgress(int pct, const char *msg) {
  if (cur != BOOT) return;
  lv_bar_set_value(bootBar, pct, LV_ANIM_OFF);
  lv_label_set_text(bootMsg, msg);
}
}
#endif
