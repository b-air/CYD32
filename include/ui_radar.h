// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Procedural green-phosphor PPI radar (v9 sonar theme), shared by the CYD and
// StopWatch UIs. One LVGL object draws everything in LV_EVENT_DRAW_MAIN: range
// rings, rim, 5/10/30 deg ticks, bearing labels, crosshair, the
// 40 deg / 8-step sweep wedge with its leading line, echoes with level decay,
// the corner-bracket target box and an optional rim progress arc. No child
// objects, no opacity and no transform on the object, so LVGL never needs an
// intermediate layer for it. A 50 ms timer advances the sweep (3.0 s per turn)
// and invalidates only the old/new wedge bounding box plus echoes whose decay
// level changed.
#pragma once
#include <stdint.h>
#include "lvgl.h"

namespace radar {
enum Kind : uint8_t { WIFI = 0, BLE = 1, DRONE = 2 };

struct Style {
  int16_t cx, cy;               // centre, relative to the widget
  int16_t rings[3];             // inner range rings (0 = unused)
  int16_t rim;                  // rim radius; ticks start here and point outward
  uint16_t bg, ringCol;         // RGB565
  const uint16_t *wedge;        // 8 colours pre-blended over bg (theme header)
  const uint16_t *droneRamp;    // 4 drone core colours L1..L4 (theme header)
  uint8_t rimW;
  uint8_t tick5, tick10, tick30;  // tick lengths in px (0 = no ticks)
  int16_t skip0, skip1;         // no ticks for bearings in [skip0, skip1] (tab band); skip0 > skip1 = none
  int16_t crossR;               // crosshair half length (0 = none)
  int16_t bearR, bearRmax;      // bearing labels (bearR 0 = none); pulled in so the far corner <= bearRmax
  const lv_font_t *bearFont;
  uint16_t bearMaj, bearMin;    // colours of 000/090/180/270 and of the others
  uint16_t bearSkip;            // bit i set: no label at i * 30 deg
  const lv_font_t *idFont;      // contact labels
  int16_t echoR, boxSize;
  uint8_t boxW, lineW, underW;  // target box, sweep line and its G3 underlay
  bool sweep;                   // animated sweep (false: static scope, echoes at L3)
  bool keyTicks;                // 2 px G4 marks at 090 / 270 (StopWatch key tags)
  int16_t closeR, veryCloseR;   // dotted CLOSE / VERY CLOSE rings (0 = none), see rssi_track.h
};

enum ContactFlag : uint8_t { CLOSE = 0x01, STALE = 0x02 };
constexpr int kTrailPts = 3;
struct Contact {
  int16_t dx, dy;   // from the centre
  uint8_t kind;     // Kind
  uint8_t r;        // core radius, 0 = style echoR
  char id[8];       // label ("" = none)
  uint32_t seed;    // stable hash (sub-circle offsets)
  uint8_t flags;    // ContactFlag: CLOSE = highlight ring, STALE = drawn at the dimmest level
  uint8_t tn;       // trail points used
  int16_t tdx[kTrailPts], tdy[kTrailPts];  // previous positions, newest first (dim dots)
};

constexpr int kMaxContacts = 32;

lv_obj_t *create(lv_obj_t *parent, int x, int y, int w, int h, const Style &s);
// sel: index of the contact that gets the target box, -1 = none.
void setContacts(lv_obj_t *o, const Contact *c, int n, int sel);
void setProgress(lv_obj_t *o, int pct);  // rim progress arc, clockwise from 000; < 0 hides it
void polar(int r, int brgDeg, int16_t &dx, int16_t &dy);  // bearing 0 = up, clockwise
int bearingOf(int dx, int dy);                            // 0..359
// Sonar position from RSSI and the indicative bearing: radius linear in dBm
// (rssi::radius, ring 1 = -50 dBm), kept off the hub and inside the rim.
void placeRssi(const Style &s, int dbm, int brgDeg, int16_t &dx, int16_t &dy);
int rssiRadius(const Style &s, int dbm);
}  // namespace radar
