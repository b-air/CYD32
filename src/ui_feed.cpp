// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "ui_feed.h"
#include <math.h>
namespace {
lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }
lv_obj_t *ov = nullptr, *xpLbl = nullptr, *par = nullptr;
lv_timer_t *tmr = nullptr;
feed::Spec sp;
uint32_t t0 = 0;
int32_t frozen = -1;
bool chompPending = false, chompDone = false;

int32_t elapsed() { return frozen >= 0 ? frozen : (int32_t)(lv_tick_get() - t0); }
float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float easeInOut(float t) { return t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t); }

void line(lv_layer_t *L, float x1, float y1, float x2, float y2, int w, uint16_t col) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = C(col); d.width = w; d.round_start = 1; d.round_end = 1;
  d.p1.x = (lv_value_precise_t)x1; d.p1.y = (lv_value_precise_t)y1;
  d.p2.x = (lv_value_precise_t)x2; d.p2.y = (lv_value_precise_t)y2;
  lv_draw_line(L, &d);
}
void disc(lv_layer_t *L, float x, float y, int r, uint16_t col, bool fill) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_opa = fill ? LV_OPA_COVER : LV_OPA_TRANSP;
  d.bg_color = C(col);
  d.border_color = C(col); d.border_width = fill ? 0 : 1; d.border_opa = LV_OPA_COVER;
  const lv_area_t a = {(int32_t)(x - r), (int32_t)(y - r), (int32_t)(x + r), (int32_t)(y + r)};
  lv_draw_rect(L, &d, &a);
}
// The drone: X frame, square body, four rotors with a spinning blade.
void drawDrone(lv_layer_t *L, float x, float y, float arm, float spin) {
  const int w = arm >= 10 ? 3 : 2;
  line(L, x - arm, y - arm * 0.8f, x + arm, y + arm * 0.8f, w, sp.frame);
  line(L, x - arm, y + arm * 0.8f, x + arm, y - arm * 0.8f, w, sp.frame);
  const int body = (int)(arm * 0.35f) + 1;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = C(sp.frame); d.radius = 2;
  const lv_area_t a = {(int32_t)(x - body), (int32_t)(y - body), (int32_t)(x + body), (int32_t)(y + body)};
  lv_draw_rect(L, &d, &a);
  const int r = (int)(arm * 0.55f) + 1;
  for (int i = 0; i < 4; i++) {
    const float rx = x + ((i & 1) ? arm : -arm), ry = y + ((i & 2) ? arm * 0.8f : -arm * 0.8f);
    disc(L, rx, ry, r, sp.rotor, false);
    const float ang = spin * ((i == 0 || i == 3) ? 1.f : -1.f) + i;
    const float bx = cosf(ang) * (r - 1), by = sinf(ang) * (r - 1);
    line(L, rx - bx, ry - by, rx + bx, ry + by, 2, sp.rotor);
  }
}
// Quadratic Bezier: start -> control (above, between) -> mouth, eased, with
// a slight hover wobble; the drone shrinks over the last 40 %.
void dronePos(int32_t t, float &x, float &y, float &shrink) {
  const float u = easeInOut(clamp01((float)t / feed::kFlyMs));
  const float cx = (sp.x0 + sp.mouthX) * 0.5f, cy = (float)(sp.y0 < sp.mouthY ? sp.y0 : sp.mouthY) - sp.arm * 2.f;
  const float v = 1 - u;
  x = v * v * sp.x0 + 2 * v * u * cx + u * u * sp.mouthX;
  y = v * v * sp.y0 + 2 * v * u * cy + u * u * sp.mouthY + sinf(t * 0.02f) * 2.f;
  shrink = u < 0.6f ? 1.f : 1.f - 0.65f * (u - 0.6f) / 0.4f;
}
void onDraw(lv_event_t *e) {
  lv_layer_t *L = lv_event_get_layer(e);
  // Draw in the parent's coordinates: the overlay is only the small dirty box.
  lv_area_t a;
  lv_obj_get_coords(par, &a);
  const int32_t t = elapsed();
  const float ox = (float)a.x1, oy = (float)a.y1;
  if (t < (int32_t)feed::kFlyMs) {
    float x, y, shrink;
    dronePos(t, x, y, shrink);
    drawDrone(L, ox + x, oy + y, sp.arm * shrink, t * 0.045f);
  } else {
    // Crumbs: 7 bits thrown out of the mouth, falling with gravity.
    const float s = (float)(t - feed::kFlyMs) / 1000.f;
    if (s < 0.7f) {
      for (int i = 0; i < 7; i++) {
        const float ang = -2.6f + i * 0.87f;
        const float sp0 = 60.f + 9.f * (i % 3);
        const float x = sp.mouthX + cosf(ang) * sp0 * s * (sp.arm / 10.f);
        const float y = sp.mouthY + sinf(ang) * sp0 * s * (sp.arm / 10.f) + 80.f * s * s * (sp.arm / 10.f);
        const int r = 1 + (i % 2) + (sp.arm >= 14 ? 1 : 0);
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = C(sp.crumb);
        d.bg_opa = (lv_opa_t)(255 * (1.f - s / 0.7f));
        const lv_area_t ar = {(int32_t)(ox + x - r), (int32_t)(oy + y - r), (int32_t)(ox + x + r), (int32_t)(oy + y + r)};
        lv_draw_rect(L, &d, &ar);
      }
    }
  }
}
void update() {
  if (!ov) return;
  const int32_t t = elapsed();
  // Keep the overlay (= the dirty area) small: around the drone while it
  // flies, around the mouth for the crumbs. Moving it invalidates both boxes.
  if (t < (int32_t)feed::kFlyMs) {
    float x, y, sh;
    dronePos(t, x, y, sh);
    const int half = (int)(sp.arm * 1.8f) + 4;
    lv_obj_set_pos(ov, (int32_t)x - half, (int32_t)y - half);
    lv_obj_set_size(ov, 2 * half, 2 * half);
  } else {
    const int a6 = sp.arm * 6 + 6;
    lv_obj_set_pos(ov, sp.mouthX - a6, sp.mouthY - sp.arm * 4 - 6);
    lv_obj_set_size(ov, 2 * a6, sp.arm * 10 + 12);
  }
  if (t >= (int32_t)feed::kFlyMs && !chompDone) { chompDone = true; chompPending = true; }
  if (xpLbl) {
    if (t < (int32_t)feed::kFlyMs) lv_obj_add_flag(xpLbl, LV_OBJ_FLAG_HIDDEN);
    else {
      const float s = clamp01((float)(t - feed::kFlyMs) / (feed::kTotalMs - feed::kFlyMs));
      lv_obj_remove_flag(xpLbl, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(xpLbl, sp.mouthX + sp.arm * 2, sp.mouthY - sp.arm * 2 - (int)(s * sp.arm * 2.5f));
      lv_obj_set_style_text_opa(xpLbl, (lv_opa_t)(s < 0.6f ? 255 : 255 * (1.f - (s - 0.6f) / 0.4f)), 0);
    }
  }
  lv_obj_invalidate(ov);  // spinning rotors / falling crumbs inside the small box
}
void tick(lv_timer_t *) {
  if (frozen < 0 && elapsed() >= (int32_t)feed::kTotalMs) { feed::stop(); return; }
  update();
}
}  // namespace
namespace feed {
void start(lv_obj_t *parent, const Spec &s) {
  stop();
  sp = s;
  par = parent;
  ov = lv_obj_create(parent);
  lv_obj_remove_style_all(ov);
  lv_obj_set_size(ov, 4, 4);
  lv_obj_remove_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_add_event_cb(ov, onDraw, LV_EVENT_DRAW_MAIN, nullptr);
  xpLbl = lv_label_create(parent);
  lv_label_set_text(xpLbl, "+2 XP");
  lv_obj_set_style_text_font(xpLbl, s.font, 0);
  lv_obj_set_style_text_color(xpLbl, C(s.text), 0);
  lv_obj_add_flag(xpLbl, LV_OBJ_FLAG_HIDDEN);
  t0 = lv_tick_get();
  chompPending = chompDone = false;
  tmr = lv_timer_create(tick, 30, nullptr);
  update();
}
bool active() { return ov != nullptr; }
bool consumeChomp() { const bool c = chompPending; chompPending = false; return c; }
void creatureScale(uint16_t &sx, uint16_t &sy) {
  sx = sy = 256;
  if (!ov) return;
  const int32_t t = elapsed() - (int32_t)kFlyMs + 60;  // mouth opens just before contact
  if (t < 0 || t > (int32_t)kChompMs) return;
  // Two damped chomps: squash (wide + short), then stretch, settling to 1.0.
  const float u = (float)t / kChompMs;
  const float k = sinf(u * 6.2832f * 2.f) * (1.f - u) * 0.16f;
  sx = (uint16_t)(256 * (1.f + k));
  sy = (uint16_t)(256 * (1.f - k));
}
void stop() {
  if (tmr) { lv_timer_delete(tmr); tmr = nullptr; }
  if (ov) { lv_obj_delete(ov); ov = nullptr; }
  if (xpLbl) { lv_obj_delete(xpLbl); xpLbl = nullptr; }
  par = nullptr;
  chompPending = false;
}
void freezeAt(int32_t t) { frozen = t; if (ov) update(); }
}
