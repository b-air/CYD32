// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
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

namespace {
lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }
constexpr int kTop = 18, kTabY = 200, kContentH = kTabY - kTop;

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
int flashLeft = 0;
lv_obj_t *detList;
lv_obj_t *otaQr, *otaInfo, *otaBar, *otaMsg;
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
  header = box(scr, 0, 0, 320, kTop, CYD32_COL_GRID);
  headerLbl = label(header, "", &lv_font_unscii_8, CYD32_COL_AMBER);
  lv_obj_align(headerLbl, LV_ALIGN_LEFT_MID, 4, 0);
  statusLbl = label(header, "", &lv_font_unscii_8, CYD32_COL_CYAN);
  lv_obj_align(statusLbl, LV_ALIGN_RIGHT_MID, -4, 0);
  banner = box(scr, 0, 0, 320, kTop, CYD32_COL_HIT);
  lv_obj_t *bl = label(banner, "UNSIGNED FIRMWARE MODE", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_center(bl);
  lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
  content = box(scr, 0, kTop, 320, kContentH, CYD32_COL_BG);
  tabbar = box(scr, 0, kTabY, 320, 40, CYD32_COL_PANEL);
  const char *tabs[4] = {"SONAR", "GRID", "CREA", "MENU"};
  const ui::Screen ts[4] = {ui::SONAR, ui::GRID, ui::CREATURE, ui::MENU};
  for (int i = 0; i < 4; i++) button(tabbar, tabs[i], i * 80, 0, 80, 40, onShow, (void *)(intptr_t)ts[i]);
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
  lv_obj_t *h = label(content, "Hold BOOT 3 s: unsigned update", &lv_font_unscii_8, CYD32_COL_WATER);
  lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -6);
}

uint32_t mix(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; return x; }

void buildSonar() {
  const int d = 176, cx = 160, cy = kContentH / 2;
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
  const int cx = 160, cy = kContentH / 2, R = 86;
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

// LVGL 9.2 cannot scale indexed (I4) images, so the 64x64 sprite is expanded
// once per change into a 128x128 RGB565A8 buffer (nearest neighbour, x2).
uint8_t *big = nullptr;  // heap, only while the creature screen is shown
constexpr size_t kBigSize = 128 * 128 * 3;
lv_image_dsc_t bigDsc;
const lv_image_dsc_t *bigSrc = nullptr;
const lv_image_dsc_t *upscale(const lv_image_dsc_t *src) {
  if (!big) return src;  // allocation failed: show the sprite at 1x
  if (src == bigSrc) return &bigDsc;
  bigSrc = src;
  const uint8_t *pal = src->data, *px = src->data + 64;
  uint16_t *rgb = (uint16_t *)big; uint8_t *alpha = big + 128 * 128 * 2;
  for (int y = 0; y < 128; y++)
    for (int x = 0; x < 128; x++) {
      const uint8_t b = px[(y / 2) * 32 + (x / 4)];
      const uint8_t idx = ((x / 2) & 1) ? (b & 0x0f) : (b >> 4);
      const uint8_t *c = pal + idx * 4;  // B,G,R,A
      rgb[y * 128 + x] = ((c[2] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[0] >> 3);
      alpha[y * 128 + x] = c[3];
    }
  bigDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  bigDsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
  bigDsc.header.w = 128; bigDsc.header.h = 128; bigDsc.header.stride = 256;
  bigDsc.data_size = kBigSize; bigDsc.data = big;
  lv_image_cache_drop(&bigDsc);
  return &bigDsc;
}
void creaAction(lv_event_t *e) { creature::addXp((uintptr_t)lv_event_get_user_data(e)); }
void buildCreature() {
  creaImg = lv_image_create(content);
  bigSrc = nullptr;
  if (!big) big = (uint8_t *)malloc(kBigSize);
  lv_image_set_src(creaImg, upscale(creature::sprite()));
  lv_obj_set_pos(creaImg, big ? 22 : 54, big ? 26 : 58);
  creaInfo = label(content, "", &lv_font_unscii_8, CYD32_COL_WHITE);
  lv_obj_set_pos(creaInfo, 176, 12);
  creaBar = lv_bar_create(content);
  lv_obj_set_size(creaBar, 130, 8); lv_obj_set_pos(creaBar, 176, 92);
  lv_obj_set_style_bg_color(creaBar, C(CYD32_COL_GRID), 0);
  lv_obj_set_style_bg_color(creaBar, C(CYD32_COL_CYAN), LV_PART_INDICATOR);
  button(content, "PET", 176, 120, 62, 32, creaAction, (void *)1, CYD32_COL_CYAN);
  button(content, "FEED", 244, 120, 62, 32, creaAction, (void *)2, CYD32_COL_CYAN);
  flash = box(content, 0, 0, 320, kContentH, CYD32_COL_WHITE);
  lv_obj_add_flag(flash, LV_OBJ_FLAG_HIDDEN);
}
void refreshCreature() {
  const uint8_t s = creature::stage();
  const det::Counts c = det::counts();
  char t[128];
  const uint32_t next = creature::threshold(s + 1);
  snprintf(t, sizeof t, "%s  stage s%u\nXP %lu\ndrones seen %lu\nnext stage %s%lu\n\nWIFI %u  BLE %u",
           creature::name(), s, (unsigned long)creature::xp(), (unsigned long)c.droneEver,
           next ? "at " : "max ", (unsigned long)next, c.wifi, c.ble);
  lv_label_set_text(creaInfo, t);
  int pct = next ? (int)(100 * (c.droneEver - creature::threshold(s)) / (next - creature::threshold(s))) : 100;
  lv_bar_set_value(creaBar, pct, LV_ANIM_OFF);
  if (creature::consumeEvolved()) flashLeft = 6;
  if (flashLeft > 0) {  // evolution flash: white on/off, then new sprite
    if (--flashLeft % 2) lv_obj_remove_flag(flash, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(flash, LV_OBJ_FLAG_HIDDEN);
  }
  if (creature::sprite() != bigSrc) lv_image_set_src(creaImg, upscale(creature::sprite()));
}

void buildMenu() {
  struct { const char *t; ui::Screen s; } items[6] = {
      {"Details", ui::DETAILS}, {"Trackers", ui::TRACKERS}, {"Drones", ui::DRONES},
      {"Wardriving", ui::WARDRIVING}, {"Update", ui::UPDATE_CONFIRM}, {"About", ui::ABOUT}};
  for (int i = 0; i < 6; i++)
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
void refreshOta() {
  const ota_portal::State &st = ota_portal::state();
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
           "CYD32 - passive RF sonar (receive only)\n\nVersion  %s\nCommit   %s\nLicence  %s\nStatus   %s\nKey      %s\n\nSource code:\n%s\n\n"
           "ABSOLUTELY NO WARRANTY. Free software:\nredistribute it under the AGPL-3.0;\nget the source at the URL above.",
           CYD32_VERSION, CYD32_COMMIT, CYD32_LICENSE, fw_status::kindStr(), fw_status::keyFingerprint(), CYD32_SOURCE_URL);
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
  lv_obj_t *s = label(content, "BOOT button held.\n\nArm UNSIGNED update mode?\nThe next update may install firmware\nwhose origin cannot be verified.\nValid for ONE update, this boot only.", &lv_font_unscii_8, CYD32_COL_WHITE);
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
  if (big) { lv_image_cache_drop(&bigDsc); free(big); big = nullptr; bigSrc = nullptr; }
  cur = s;
  const bool tabs = s != BOOT && s != OTA && s != UNSIGNED_CONFIRM;
  if (tabs) lv_obj_remove_flag(tabbar, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(tabbar, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_height(content, tabs ? kContentH : 240 - kTop);
  switch (s) {
    case BOOT: buildBoot(); break;
    case SONAR: buildSonar(); break;
    case GRID: buildGrid(); break;
    case CREATURE: buildCreature(); break;
    case MENU: buildMenu(); break;
    case DETAILS: buildDetails(); break;
    case TRACKERS: buildStub("TRACKERS", "Passive detection of BLE item trackers (AirTag, SmartTag, Tile) seen following you over time."); break;
    case DRONES: buildStub("DRONES", "Remote ID beacons (ASTM F3411, Wi-Fi and BLE) are already counted on the sonar. Phase 3 decodes position and operator fields."); break;
    case WARDRIVING: buildStub("WARDRIVING", "On request only: export to the local microSD card, hashed MACs, never uploaded anywhere."); break;
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
