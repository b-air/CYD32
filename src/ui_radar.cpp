// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "ui_radar.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cyd32_sonar_theme.h"
#include "rssi_track.h"

namespace radar {
namespace {
constexpr float kPi = 3.14159265f;
#ifdef CYD32_HW_M5STACK_STOPWATCH
constexpr uint32_t kFrameMs = CYD32_SONAR_FRAME_MS;  // 20 fps, 6 deg per frame
#else
constexpr uint32_t kFrameMs = 66;                    // 15 fps on the CYD (SPI budget), 8 deg per frame
#endif
constexpr int kStepDeg = (int)(360 * kFrameMs / CYD32_SONAR_SWEEP_PERIOD_MS);

struct State {
  Style s;
  int16_t sweep = 0;
  int16_t prog = -1;
  int n = 0, sel = -1;
  Contact c[kMaxContacts];
  uint16_t brg[kMaxContacts];
  uint8_t lvl[kMaxContacts];
  int16_t labX[12], labY[12];  // bearing label top-left, relative to the centre
  lv_timer_t *timer = nullptr;
};

lv_color_t C(uint16_t v) { return lv_color_make(((v >> 11) & 0x1f) << 3, ((v >> 5) & 0x3f) << 2, (v & 0x1f) << 3); }
State *st(lv_obj_t *o) { return static_cast<State *>(lv_obj_get_user_data(o)); }
float sinD(float d) { return sinf(d * kPi / 180.f); }
float cosD(float d) { return cosf(d * kPi / 180.f); }
int wrap(int d) { d %= 360; return d < 0 ? d + 360 : d; }

uint8_t levelOf(const State &S, uint16_t brg) {
  if (!S.s.sweep) return 3;
  const int d = wrap(S.sweep - brg);
  if (d < CYD32_SONAR_DECAY_L4_DEG) return 4;
  if (d < CYD32_SONAR_DECAY_L3_DEG) return 3;
  if (d < CYD32_SONAR_DECAY_L2_DEG) return 2;
  return 1;
}
bool meets(const lv_area_t &clip, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
  return x1 <= clip.x2 && x2 >= clip.x1 && y1 <= clip.y2 && y2 >= clip.y1;
}
int16_t fontH(const lv_font_t *f) { return f ? (int16_t)lv_font_get_line_height(f) : 0; }
int16_t textW(const lv_font_t *f, const char *t) {
  lv_point_t p;
  lv_text_get_size(&p, t, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return (int16_t)p.x;
}
void circle(lv_layer_t *L, const lv_area_t &clip, int32_t x, int32_t y, int32_t r, lv_color_t col) {
  if (r < 1 || !meets(clip, x - r, y - r, x + r, y + r)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = col;
  d.radius = LV_RADIUS_CIRCLE;
  const lv_area_t a = {x - r, y - r, x + r, y + r};
  lv_draw_rect(L, &d, &a);
}
void line(lv_layer_t *L, const lv_area_t &clip, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int w, lv_color_t col) {
  if (!meets(clip, LV_MIN(x1, x2) - w, LV_MIN(y1, y2) - w, LV_MAX(x1, x2) + w, LV_MAX(y1, y2) + w)) return;
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = col;
  d.width = w;
  d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
  lv_draw_line(L, &d);
}
void ring(lv_layer_t *L, const lv_area_t &clip, int32_t x, int32_t y, int32_t r, int w, lv_color_t col, float a0 = 0, float a1 = 360) {
  if (r < 1 || !meets(clip, x - r, y - r, x + r, y + r)) return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = col;
  d.width = w;
  d.center.x = x; d.center.y = y;
  d.radius = (uint16_t)r;
  d.start_angle = (lv_value_precise_t)(a0 - 90.f);  // LVGL: 0 = 3 o'clock; bearing 0 = up
  d.end_angle = (lv_value_precise_t)(a1 - 90.f);
  lv_draw_arc(L, &d);
}
void text(lv_layer_t *L, const lv_area_t &clip, const char *t, const lv_font_t *f, int32_t x, int32_t y, lv_color_t col) {
  const int32_t w = textW(f, t), h = fontH(f);
  if (!meets(clip, x, y, x + w, y + h)) return;
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.font = f;
  d.color = col;
  d.text = t;
  d.text_local = 1;  // the string lives on the stack; LVGL renders later
  const lv_area_t a = {x, y, x + w, y + h - 1};
  lv_draw_label(L, &d, &a);
}
void pt(const State &S, float r, float brg, int32_t &x, int32_t &y) {
  x = (int32_t)lroundf(S.s.cx + r * sinD(brg));
  y = (int32_t)lroundf(S.s.cy - r * cosD(brg));
}

// Contact bounding box (blob, halo, label, box), relative to the widget.
lv_area_t contactArea(const State &S, int i) {
  const Contact &c = S.c[i];
  const int r = c.r ? c.r : S.s.echoR;
  const int halo = r * 2 + 1;
  const int box = S.s.boxSize / 2 + S.s.boxW + 1;
  const int lw = c.id[0] ? textW(S.s.idFont, c.id) : 0, lh = c.id[0] ? fontH(S.s.idFont) : 0;
  const int x = S.s.cx + c.dx, y = S.s.cy + c.dy;
  const int m = LV_MAX(halo + 3, box);  // +3: CLOSE highlight ring
  lv_area_t a = {x - m, y - m - lh - 2, x + m + lw + 2, y + m};
  for (int t = 0; t < c.tn && t < kTrailPts; t++) {
    const int tx = S.s.cx + c.tdx[t], ty = S.s.cy + c.tdy[t];
    a.x1 = LV_MIN(a.x1, tx - 3); a.y1 = LV_MIN(a.y1, ty - 3); a.x2 = LV_MAX(a.x2, tx + 3); a.y2 = LV_MAX(a.y2, ty + 3);
  }
  return a;
}
void invalidateRel(lv_obj_t *o, lv_area_t a) {
  lv_area_t co;
  lv_obj_get_coords(o, &co);
  lv_area_move(&a, co.x1, co.y1);
  lv_obj_invalidate_area(o, &a);
}
// Bounding box of the sector [b0, b1] (clockwise) of radius r, relative to the widget.
lv_area_t sectorArea(const State &S, int b0, int b1, int r) {
  lv_area_t a = {S.s.cx, S.s.cy, S.s.cx, S.s.cy};
  for (int b = b0;; b += 5) {
    const int bb = b > b1 ? b1 : b;
    int32_t x, y;
    pt(S, (float)r, (float)bb, x, y);
    a.x1 = LV_MIN(a.x1, x); a.y1 = LV_MIN(a.y1, y); a.x2 = LV_MAX(a.x2, x); a.y2 = LV_MAX(a.y2, y);
    if (bb == b1) break;
  }
  const int pad = S.s.underW + 2;
  a.x1 -= pad; a.y1 -= pad; a.x2 += pad; a.y2 += pad;
  return a;
}

void tick(lv_timer_t *t) {
  lv_obj_t *o = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
  State &S = *st(o);
  const int old = S.sweep;
  S.sweep = (int16_t)wrap(S.sweep + kStepDeg);
  invalidateRel(o, sectorArea(S, old - CYD32_SONAR_WEDGE_DEG, old + kStepDeg, S.s.rim));
  for (int i = 0; i < S.n; i++) {
    const uint8_t l = levelOf(S, S.brg[i]);
    if (l != S.lvl[i]) { S.lvl[i] = l; invalidateRel(o, contactArea(S, i)); }
  }
}

void drawCb(lv_event_t *e) {
  lv_obj_t *o = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  State *P = st(o);
  if (!P) return;
  State &S = *P;
  const Style &s = S.s;
  lv_layer_t *L = lv_event_get_layer(e);
  const lv_area_t clip = L->_clip_area;
  lv_area_t co;
  lv_obj_get_coords(o, &co);
  const int32_t ox = co.x1, oy = co.y1, X = ox + s.cx, Y = oy + s.cy;
  int32_t x1, y1, x2, y2;

  // 1. Sweep wedge: 8 triangles, tail first.
  if (s.sweep && meets(clip, X - s.rim, Y - s.rim, X + s.rim, Y + s.rim)) {
    for (int i = CYD32_SONAR_WEDGE_STEPS - 1; i >= 0; i--) {
      const float b1 = (float)(S.sweep - i * CYD32_SONAR_WEDGE_STEP_DEG), b0 = b1 - CYD32_SONAR_WEDGE_STEP_DEG;
      pt(S, (float)s.rim, b0, x1, y1);
      pt(S, (float)s.rim, b1 + 0.5f, x2, y2);
      x1 += ox; y1 += oy; x2 += ox; y2 += oy;
      if (!meets(clip, LV_MIN(X, LV_MIN(x1, x2)), LV_MIN(Y, LV_MIN(y1, y2)), LV_MAX(X, LV_MAX(x1, x2)), LV_MAX(Y, LV_MAX(y1, y2)))) continue;
      lv_draw_triangle_dsc_t d;
      lv_draw_triangle_dsc_init(&d);
      d.bg_color = C(s.wedge[i]);
      d.bg_opa = LV_OPA_COVER;
      d.p[0].x = X; d.p[0].y = Y;
      d.p[1].x = x1; d.p[1].y = y1;
      d.p[2].x = x2; d.p[2].y = y2;
      lv_draw_triangle(L, &d);
    }
  }
  // 2. Crosshair, range rings, rim.
  if (s.crossR) {
    line(L, clip, X - s.crossR, Y, X + s.crossR, Y, 1, C(CYD32_SONAR_G1));
    line(L, clip, X, Y - s.crossR, X, Y + s.crossR, 1, C(CYD32_SONAR_G1));
  }
  for (int r : s.rings) if (r) ring(L, clip, X, Y, r, 1, C(s.ringCol));
  // Dotted CLOSE (G3) and VERY CLOSE (G4) rings: finer marking near the centre.
  const int16_t dotted[2] = {s.closeR, s.veryCloseR};
  for (int k = 0; k < 2; k++) {
    const int r = dotted[k];
    if (r < 4 || !meets(clip, X - r - 1, Y - r - 1, X + r + 1, Y + r + 1)) continue;
    const int step = r > 40 ? 6 : 12;
    for (int b = 0; b < 360; b += step) {
      pt(S, (float)r, (float)b, x1, y1);
      circle(L, clip, ox + x1, oy + y1, 1, C(k ? CYD32_SONAR_G4 : CYD32_SONAR_G3));
    }
  }
  ring(L, clip, X, Y, s.rim + s.rimW / 2, s.rimW, C(CYD32_SONAR_G3));
  // 3. Ticks, outward from the rim.
  if (s.tick30) {
    for (int b = 0; b < 360; b += CYD32_SONAR_TICK_STEP_DEG) {
      if (s.skip0 <= s.skip1 && b >= s.skip0 && b <= s.skip1) continue;
      const bool b30 = b % 30 == 0, b10 = b % 10 == 0;
      const int len = b30 ? s.tick30 : b10 ? s.tick10 : s.tick5;
      if (!len) continue;
      pt(S, (float)s.rim, (float)b, x1, y1);
      pt(S, (float)(s.rim + len), (float)b, x2, y2);
      line(L, clip, ox + x1, oy + y1, ox + x2, oy + y2, 1,
           C(b30 ? CYD32_SONAR_G4 : b10 ? CYD32_SONAR_G3 : CYD32_SONAR_G2));
    }
  }
  if (s.keyTicks) {
    static const int kKeyBrg[2] = {90, 270};
    for (int b : kKeyBrg) {
      pt(S, SW_KEYTAG_TICK_R0, (float)b, x1, y1);
      pt(S, SW_KEYTAG_TICK_R1, (float)b, x2, y2);
      line(L, clip, ox + x1, oy + y1, ox + x2, oy + y2, 2, C(CYD32_SONAR_G4));
    }
  }
  // 4. Sweep line over its G3 underlay, hub dot.
  if (s.sweep) {
    pt(S, (float)s.rim, (float)S.sweep, x2, y2);
    line(L, clip, X, Y, ox + x2, oy + y2, s.underW, C(CYD32_SONAR_G3));
    line(L, clip, X, Y, ox + x2, oy + y2, s.lineW, C(CYD32_SONAR_G4));
    circle(L, clip, X, Y, 2, C(CYD32_SONAR_G4));
  }
  // 5. Echoes: halo, core and two sub-circles (deterministic from the hash).
  static const uint16_t kCore[5] = {0, CYD32_SONAR_G2, CYD32_SONAR_G3, CYD32_SONAR_G4, CYD32_SONAR_G5};
  static const uint16_t kHalo[5] = {0, CYD32_SONAR_G1, CYD32_SONAR_G1, CYD32_SONAR_G2, CYD32_SONAR_G3};
  static const lv_opa_t kDroneHalo[5] = {0, 26, 46, 71, 102};  // 10 / 18 / 28 / 40 %
  for (int i = 0; i < S.n; i++) {
    const Contact &c = S.c[i];
    const int r = c.r ? c.r : s.echoR;
    const int32_t x = X + c.dx, y = Y + c.dy;
    // Trail: previous positions as small dim dots (newest brightest).
    for (int t = 0; t < c.tn && t < kTrailPts; t++) {
      const uint16_t tc = c.kind == DRONE ? s.droneRamp[0] : (t == 0 ? CYD32_SONAR_G3 : CYD32_SONAR_G2);
      circle(L, clip, X + c.tdx[t], Y + c.tdy[t], r > 3 ? 2 : 1, C(tc));
    }
    if (!meets(clip, x - 2 * r - 4, y - 2 * r - 4, x + 2 * r + 4, y + 2 * r + 4)) continue;
    const uint8_t l = (c.flags & STALE) ? 1 : S.lvl[i];
    lv_color_t core, halo;
    if (c.kind == DRONE) {
      core = C(s.droneRamp[l - 1]);
      halo = lv_color_mix(C(CYD32_SONAR_DRONE), C(s.bg), kDroneHalo[l]);
    } else {
      core = C(kCore[l]);
      halo = C(kHalo[l]);
    }
    circle(L, clip, x, y, r * 2 - (r > 2 ? 1 : 0), halo);
    const int sub = (r * 3 + 2) / 4;
    const int jx = (int)(c.seed % 5) - 2, jy = (int)((c.seed >> 3) % 3) - 1;
    circle(L, clip, x + jx * r / 3, y + jy * r / 3, sub, core);
    circle(L, clip, x - jx * r / 4, y - jy * r / 4, sub, core);
    circle(L, clip, x, y, r, core);
    if (c.flags & CLOSE) ring(L, clip, x, y, r * 2 + 3, 1, C(c.kind == DRONE ? CYD32_SONAR_DRONE : CYD32_SONAR_G5));
  }
  // 6. Target box: four corner brackets.
  if (S.sel >= 0 && S.sel < S.n && s.boxSize) {
    const int32_t x = X + S.c[S.sel].dx, y = Y + S.c[S.sel].dy, h = s.boxSize / 2, k = (s.boxSize * 32 + 50) / 100;
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sy = -1; sy <= 1; sy += 2) {
        const int32_t cx = x + sx * h, cy = y + sy * h;
        line(L, clip, cx, cy, cx, cy - sy * k, s.boxW, C(CYD32_SONAR_G5));
        line(L, clip, cx, cy, cx - sx * k, cy, s.boxW, C(CYD32_SONAR_G5));
      }
  }
  // 7. A-J letters along the horizontal axis.
  if (s.lettersFont) {
    for (int c = 0; c < 10; c++) {
      const char t[2] = {(char)('A' + c), 0};
      const int32_t cx = X + ((2 * c - 9) * s.colPitchX10) / 20;
      text(L, clip, t, s.lettersFont, cx - textW(s.lettersFont, t) / 2, Y + s.lettersDy, C(CYD32_SONAR_G2));
    }
  }
  // 8. Contact labels (they do not decay).
  for (int i = 0; i < S.n; i++) {
    const Contact &c = S.c[i];
    if (!c.id[0] || !s.idFont) continue;
    const int r = c.r ? c.r : s.echoR;
    const lv_color_t col = c.kind == DRONE ? C(CYD32_SONAR_DRONE) : C(S.lvl[i] >= 3 ? CYD32_SONAR_G4 : CYD32_SONAR_G3);
    const int32_t x = X + c.dx, y = Y + c.dy, h = fontH(s.idFont);
    if (i == S.sel) text(L, clip, c.id, s.idFont, x - textW(s.idFont, c.id) / 2, y - s.boxSize / 2 - h - 1, col);
    else text(L, clip, c.id, s.idFont, x + r * 2 + 1, y - r - h + 2, col);
  }
  // 9. Bearing labels every 30 deg.
  if (s.bearR && s.bearFont) {
    for (int i = 0; i < 12; i++) {
      if (s.bearSkip & (1u << i)) continue;
      char t[4];
      snprintf(t, sizeof t, "%03d", i * 30);
      text(L, clip, t, s.bearFont, X + S.labX[i], Y + S.labY[i], C(i % 3 == 0 ? s.bearMaj : s.bearMin));
    }
  }
  // 10. Rim progress arc (boot).
  if (S.prog > 0) ring(L, clip, X, Y, s.rim + 2, 3, C(CYD32_SONAR_G4), 0.f, 360.f * S.prog / 100.f);
}

void deleteCb(lv_event_t *e) {
  lv_obj_t *o = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
  State *S = st(o);
  if (!S) return;
  if (S->timer) lv_timer_delete(S->timer);
  lv_obj_set_user_data(o, nullptr);
  delete S;
}
}  // namespace

void polar(int r, int brgDeg, int16_t &dx, int16_t &dy) {
  dx = (int16_t)lroundf(r * sinD((float)brgDeg));
  dy = (int16_t)lroundf(-r * cosD((float)brgDeg));
}
int bearingOf(int dx, int dy) {
  const float a = atan2f((float)dx, (float)-dy) * 180.f / kPi;
  return wrap((int)lroundf(a));
}
int rssiRadius(const Style &s, int dbm) {
  const int er = s.echoR ? s.echoR : 2;
  return rssi::radius(dbm, s.rim, er * 2 + 2, s.rim - er * 2 - 2);
}
void placeRssi(const Style &s, int dbm, int brgDeg, int16_t &dx, int16_t &dy) { polar(rssiRadius(s, dbm), brgDeg, dx, dy); }
void cellOffset(const Style &s, int col, int row, int16_t &dx, int16_t &dy) {
  // Same mapping as the GRID screen: x = (col - 4.5) * pitch, y = (row - 3.5) * pitch.
  float x = (2 * col - 9) * s.colPitchX10 / 20.f, y = (2 * row - 7) * s.rowPitch / 2.f;
  const float lim = (float)(s.rim - s.echoR * 2 - 2), d = sqrtf(x * x + y * y);
  if (d > lim) { x *= lim / d; y *= lim / d; }
  dx = (int16_t)lroundf(x);
  dy = (int16_t)lroundf(y);
}

lv_obj_t *create(lv_obj_t *parent, int x, int y, int w, int h, const Style &s) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  State *S = new State();
  S->s = s;
  // Bearing label positions: centred at bearR, pulled inward until the far
  // corner is within bearRmax.
  if (s.bearFont) {
    const int lw = textW(s.bearFont, "000"), lh = fontH(s.bearFont);
    for (int i = 0; i < 12; i++) {
      float r = (float)s.bearR, cx = 0, cy = 0;
      for (; r > 0; r -= 0.5f) {
        cx = r * sinD(i * 30.f); cy = -r * cosD(i * 30.f);
        float far = 0;
        for (int sx = -1; sx <= 1; sx += 2)
          for (int sy = -1; sy <= 1; sy += 2) far = fmaxf(far, hypotf(cx + sx * lw / 2.f, cy + sy * lh / 2.f));
        if (far <= s.bearRmax) break;
      }
      S->labX[i] = (int16_t)lroundf(cx - lw / 2.f);
      S->labY[i] = (int16_t)lroundf(cy - lh / 2.f);
    }
  }
  lv_obj_set_user_data(o, S);
  lv_obj_add_event_cb(o, drawCb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(o, deleteCb, LV_EVENT_DELETE, nullptr);
  if (s.sweep) S->timer = lv_timer_create(tick, kFrameMs, o);
  return o;
}

void setContacts(lv_obj_t *o, const Contact *c, int n, int sel) {
  State *P = st(o);
  if (!P) return;
  State &S = *P;
  if (n > kMaxContacts) n = kMaxContacts;
  const int m = LV_MAX(n, S.n);
  for (int i = 0; i < m; i++) {
    const bool had = i < S.n, has = i < n;
    const bool same = had && has && memcmp(&S.c[i], &c[i], sizeof(Contact)) == 0 && (S.sel == i) == (sel == i);
    if (same) continue;
    if (had) invalidateRel(o, contactArea(S, i));
    if (has) {
      S.c[i] = c[i];
      S.brg[i] = (uint16_t)bearingOf(c[i].dx, c[i].dy);
      S.lvl[i] = levelOf(S, S.brg[i]);
      invalidateRel(o, contactArea(S, i));
    }
  }
  S.n = n;
  S.sel = sel;
}

void setProgress(lv_obj_t *o, int pct) {
  State *S = st(o);
  if (!S) return;
  if (pct > 100) pct = 100;
  if (pct == S->prog) return;
  S->prog = (int16_t)pct;
  const int r = S->s.rim + 3;
  invalidateRel(o, {S->s.cx - r, S->s.cy - r, S->s.cx + r, S->s.cy + r});
}
}  // namespace radar
