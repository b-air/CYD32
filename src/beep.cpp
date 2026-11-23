// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "beep.h"
#include "detections.h"
#include "hw.h"
#include "prox_beep.h"
namespace {
bool on = false;
uint32_t lastBeep = 0, lastCheck = 0;
}
namespace beep {
void setEnabled(bool v) {
  if (v == on) return;
  on = v;
  hw::beepPower(v);  // StopWatch: M5.Speaker begin / end; CYD: GPIO26 attach / release
  lastBeep = lastCheck = 0;
}
bool enabled() { return on; }
void toggle() { setEnabled(!on); }
void tick(uint32_t now, bool hasPinned, uint32_t pinnedId) {
  if (!on || now - lastCheck < 20) return;  // 50 Hz is plenty for a >= 150 ms cadence
  lastCheck = now;
  int8_t dbm;
  if (!det::beepTarget(det::sonarCat(), hasPinned, pinnedId, dbm)) return;
  const uint32_t iv = beep_rules::intervalMs(dbm);
  if (!iv || (lastBeep && now - lastBeep < iv)) return;
  lastBeep = now;
  hw::beepTone(beep_rules::kToneHz, beep_rules::kToneMs);
}
}
