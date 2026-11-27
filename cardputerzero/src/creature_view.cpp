// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// I4 playback for the shared creature sprites. The frame format matches
// src/ui.cpp; this file is the 320x170 layout (zoom stays near 1.0).
#include "creature_view.hpp"
#include "cyd32_palette.h"
#include "creature.h"
#include "ui_common.h"
#include "ui_feed.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
namespace {
constexpr int kSpr = 64;
constexpr size_t kI4 = kSpr * kSpr / 2;
constexpr size_t kRgb = kSpr * kSpr * 3;
const uint16_t kZoom[4] = {200, 230, 256, 288};
uint8_t *pool, *i4, *draw, *rgb;
lv_obj_t *img, *info, *toast, *parent;
lv_image_dsc_t dsc;
lv_timer_t *tmr;
const cyd32_anim_t *anim = nullptr;
uint16_t frame = 0;
bool happy = false, hatching = false, egg = false, react = false;
int focus = 0;
uint32_t toast_until = 0, egg_until = 0, pop_start = 0;
uint8_t popping = 0;
lv_obj_t *pet, *feed_btn;

lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }

void roll(uint8_t *buf, int8_t dx, int8_t dy) {
  if (dy) {
    uint8_t *tmp = rgb;
    memcpy(tmp, buf, kI4);
    for (int y = 0; y < kSpr; y++) memcpy(buf + y * 32, tmp + ((y - dy) & (kSpr - 1)) * 32, 32);
  }
  if (!dx) return;
  for (int y = 0; y < kSpr; y++) {
    uint8_t *row = buf + y * 32, pix[kSpr], out[kSpr];
    for (int x = 0; x < kSpr; x++) { const uint8_t b = row[x / 2]; pix[x] = (x & 1) ? (b & 0x0f) : (b >> 4); }
    for (int x = 0; x < kSpr; x++) out[x] = pix[(x - dx) & (kSpr - 1)];
    for (int x = 0; x < kSpr; x += 2) row[x / 2] = (uint8_t)((out[x] << 4) | out[x + 1]);
  }
}
void blit(uint8_t *dst, int x, int y, int w, int h, const uint8_t *packed) {
  const int sw = (w + 1) / 2;
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
    uint8_t v = packed[j * sw + i / 2];
    v = (i & 1) ? (v & 0x0f) : (v >> 4);
    const int X = x + i, Y = y + j;
    if ((unsigned)X >= kSpr || (unsigned)Y >= kSpr) continue;
    uint8_t *d = &dst[Y * 32 + X / 2];
    *d = (X & 1) ? (uint8_t)((*d & 0xF0) | v) : (uint8_t)((*d & 0x0F) | (v << 4));
  }
}
size_t slice(const cyd32_anim_t *a, uint16_t idx) {
  if (!a || !a->off || !a->delta || a->n < 2 || idx >= (uint16_t)(a->n - 1)) return 0;
  const uint32_t start = a->off[idx];
  const uint32_t next = (idx + 1u < (uint16_t)(a->n - 1)) ? a->off[idx + 1] : a->dlen;
  if (next < start || next > a->dlen) return 0;
  return (size_t)(next - start);
}
void apply(const uint8_t *d, size_t cap) {
  if (!d || cap < 3) return;
  const uint8_t nrect = d[2];
  const uint8_t *p = d + 3, *end = d + cap;
  roll(i4, (int8_t)d[0], (int8_t)d[1]);
  for (uint8_t r = 0; r < nrect; r++) {
    if (p + 4 > end) return;
    const int x = p[0], y = p[1], w = p[2], h = p[3];
    p += 4;
    const size_t nb = (size_t)((w + 1) / 2) * (size_t)h;
    if (p + nb > end) return;
    if (w && h && x < kSpr && y < kSpr && x + w <= kSpr && y + h <= kSpr) blit(i4, x, y, w, h, p);
    p += nb;
  }
}
void goto_frame(const cyd32_anim_t *a, uint16_t i) {
  if (!a || !i4) return;
  memcpy(i4, a->key, kI4);
  if (i >= a->n) i = a->n ? (uint16_t)(a->n - 1) : 0;
  for (uint16_t f = 1; f <= i; f++) apply(a->delta + a->off[f - 1], slice(a, (uint16_t)(f - 1)));
  frame = i;
  anim = a;
}
void blit_trait(uint8_t *dst, const cyd32_trait_t *tr, int fr, float scale) {
  if (!tr->px || !tr->anchor || scale <= 0) return;
  const int nw = (int)(tr->w * scale + 0.5f), nh = (int)(tr->h * scale + 0.5f);
  if (nw < 1 || nh < 1) return;
  const int sw = (tr->w + 1) / 2, inst = tr->inst ? tr->inst : 1;
  for (int k = 0; k < inst; k++) {
    const int ax = tr->anchor[fr * 4 + k * 2], ay = tr->anchor[fr * 4 + k * 2 + 1];
    const int x0 = ax + ((int)tr->w - nw) / 2, y0 = ay + ((int)tr->h - nh) / 2;
    for (int j = 0; j < nh; j++) for (int i = 0; i < nw; i++) {
      const int sx = i * tr->w / nw, sy = j * tr->h / nh;
      uint8_t v = tr->px[sy * sw + sx / 2];
      v = (sx & 1) ? (v & 0x0f) : (v >> 4);
      if (!v) continue;
      const int X = x0 + i, Y = y0 + j;
      if ((unsigned)X >= kSpr || (unsigned)Y >= kSpr) continue;
      uint8_t *d = &dst[Y * 32 + X / 2];
      *d = (X & 1) ? (uint8_t)((*d & 0xF0) | v) : (uint8_t)((*d & 0x0F) | (v << 4));
    }
  }
}
uint16_t zoom_now() {
  const uint8_t s = creature::size();
  return kZoom[s ? s - 1 : 0];
}
void paint() {
  if (!i4 || !rgb || !anim || !img) return;
  memcpy(draw, i4, kI4);
  const uint8_t bits = creature::unlockedTraits();
  const auto *spec = creature::spec();
  if (bits && spec && creature::size() >= 2) {
    const int nAnc = spec->adult_idle ? spec->adult_idle->n : 12;
    const int fr = nAnc ? (int)(frame % nAnc) : 0;
    const float sc = (popping && pop_start) ? (float)(lv_tick_get() - pop_start) / 560.f : 1.f;
    for (int i = 0; i < 8; i++) if (bits & (1u << i)) blit_trait(draw, &spec->traits[i], fr, sc > 1 ? 1 : sc);
  }
  uint16_t *px = (uint16_t *)rgb;
  uint8_t *al = rgb + kSpr * kSpr * 2;
  for (int y = 0; y < kSpr; y++) for (int x = 0; x < kSpr; x++) {
    const uint8_t b = draw[y * 32 + x / 2];
    const uint8_t idx = (x & 1) ? (b & 0x0f) : (b >> 4);
    const uint32_t c = anim->pal[idx];
    const int o = y * kSpr + x;
    px[o] = (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) | ((c >> 3) & 0x1F));
    al[o] = (uint8_t)(c >> 24);
  }
  dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
  dsc.header.flags = 0;
  dsc.header.w = kSpr;
  dsc.header.h = kSpr;
  dsc.header.stride = kSpr * 2;
  dsc.data_size = kRgb;
  dsc.data = rgb;
  lv_image_cache_drop(&dsc);
  lv_image_set_src(img, &dsc);
  uint16_t z = zoom_now(), sx = 256, sy = 256;
  feed::creatureScale(sx, sy);
  lv_image_set_pivot(img, kSpr / 2, kSpr);
  lv_image_set_scale_x(img, (uint16_t)((uint32_t)z * sx / 256u));
  lv_image_set_scale_y(img, (uint16_t)((uint32_t)z * sy / 256u));
}
void idle() {
  happy = hatching = false;
  const auto *spec = creature::spec();
  if (!spec) return;
  const uint8_t cth = creature::cthulhuStage();
  if (cth >= 5 && spec->cth_idle) goto_frame(spec->cth_idle, 0);
  else if (cth >= 1 && spec->cth_keys) goto_frame(spec->cth_keys, (uint16_t)(2 * cth - 1));
  else if (creature::size() <= 1 && spec->baby_idle) goto_frame(spec->baby_idle, 0);
  else if (spec->adult_idle) goto_frame(spec->adult_idle, 0);
}
void tick(lv_timer_t *) {
  const auto *spec = creature::spec();
  if (egg && creature::hatched()) { egg = false; hatching = true; if (spec) goto_frame(spec->hatch, 0); egg_until = lv_tick_get() + 400; }
  const bool hold = hatching && (int32_t)(lv_tick_get() - egg_until) < 0;
  if (i4 && anim && spec && anim->ms && anim != spec->cth_keys && !hold) {
    if ((uint16_t)(frame + 1) < anim->n) apply(anim->delta + anim->off[frame], slice(anim, frame)), frame++;
    else if (hatching || happy) idle();
    else { memcpy(i4, anim->key, kI4); frame = 0; }
  }
  if (feed::consumeChomp()) { creature::addXp(2); react = true; }
  if (react && !hatching && !happy && spec && creature::cthulhuStage() < 1) {
    react = false; happy = true;
    goto_frame(creature::size() <= 1 ? spec->baby_happy : spec->adult_happy, 0);
  }
  if (toast && toast_until && (int32_t)(lv_tick_get() - toast_until) >= 0) { toast_until = 0; lv_label_set_text(toast, ""); }
  if (info) {
    char b[96], tr[40] = "";
    const char *tn[8] = {"TRK", "AP", "HOT", "OPEN", "2.4", "5", "6", "7"};
    const uint8_t bits = creature::unlockedTraits();
    for (int i = 0; i < 8; i++) if (bits & (1u << i)) { if (tr[0]) strncat(tr, " ", sizeof tr - strlen(tr) - 1); strncat(tr, tn[i], sizeof tr - strlen(tr) - 1); }
    snprintf(b, sizeof b, "%s\nsize %u  xp %lu\n3m %lu  24h %lu\n%s", creature::name(), creature::size(),
             (unsigned long)creature::xp(), (unsigned long)creature::recentCount(), (unsigned long)creature::uniqueCount(),
             tr[0] ? tr : (creature::hatched() ? "hatched" : "egg"));
    lv_label_set_text(info, b);
  }
  if (pet) lv_obj_set_style_text_color(pet, C(focus == 0 ? CYD32_COL_AMBER : CYD32_COL_CYAN), 0);
  if (feed_btn) lv_obj_set_style_text_color(feed_btn, C(focus == 1 ? CYD32_COL_AMBER : CYD32_COL_CYAN), 0);
  paint();
}
lv_obj_t *lab(lv_obj_t *p, const char *t, int x, int y) {
  lv_obj_t *o = lv_label_create(p);
  lv_label_set_text(o, t);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_text_font(o, &lv_font_montserrat_12, 0);
  return o;
}
}  // namespace

void crea_open(lv_obj_t *p) {
  parent = p;
  focus = 0;
  if (!pool) {
    pool = (uint8_t *)malloc(kI4 * 2 + kRgb);
    if (pool) { i4 = pool; draw = pool + kI4; rgb = pool + kI4 * 2; }
  }
  memset(&dsc, 0, sizeof dsc);
  img = lv_image_create(p);
  lv_obj_set_pos(img, 4, 8);
  info = lab(p, "", 96, 2);
  lv_obj_set_width(info, 210);
  pet = lab(p, "PET", 96, 100);
  feed_btn = lab(p, "FEED", 160, 100);
  toast = lab(p, "", 96, 78);
  lv_obj_set_style_text_color(toast, C(CYD32_COL_AMBER), 0);
  const auto *spec = creature::spec();
  egg = !creature::hatched();
  if (egg && spec) { hatching = true; goto_frame(spec->hatch, 0); egg_until = lv_tick_get() + 3000; }
  else idle();
  tmr = lv_timer_create(tick, 80, nullptr);
  tick(nullptr);
}
void crea_close() {
  feed::stop();
  if (tmr) { lv_timer_delete(tmr); tmr = nullptr; }
  img = info = toast = pet = feed_btn = parent = nullptr;
  anim = nullptr;
}
void crea_focus(int dir) { focus = dir < 0 ? 0 : 1; }
void crea_select() {
  if (focus == 0) {
    const int k = uic::petForget();
    creature::addXp(1);
    react = true;
    if (toast) {
      char t[24];
      snprintf(t, sizeof t, "FORGOT %d", k);
      lv_label_set_text(toast, t);
      toast_until = lv_tick_get() + 1500;
    }
  } else if (parent) {
    feed::Spec f{};
    f.x0 = 250; f.y0 = 8; f.mouthX = 36; f.mouthY = 40; f.arm = 7;
    f.frame = CYD32_COL_CYAN; f.rotor = CYD32_COL_WHITE; f.crumb = CYD32_COL_AMBER; f.text = CYD32_COL_OK;
    f.font = &lv_font_montserrat_12;
    feed::start(parent, f);
  }
}
