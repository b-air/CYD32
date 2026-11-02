// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "fw_status.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <string.h>
#include "cyd32_build_gen.h"
namespace {
fw_status::Kind k = fw_status::FORK;
char fpr[20] = CYD32_KEY0_FPR;
}
namespace fw_status {
void begin() {
  k = CYD32_OFFICIAL ? OFFICIAL : FORK;
  Preferences p;
  if (!p.begin("cyd32fw", true)) return;
  const String part = p.getString("part", "");
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (run && part == run->label) {
    if (p.getBool("unsigned", false)) { k = UNSIGNED; strcpy(fpr, "----------------"); }
    else if (p.getInt("slot", 0) == 1) strcpy(fpr, CYD32_KEY1_FPR);
  }
  p.end();
}
Kind kind() { return k; }
const char *kindStr() { return k == OFFICIAL ? "Official" : k == FORK ? "Fork" : "Unsigned"; }
const char *keyFingerprint() { return fpr; }
void recordNextBoot(const char *label, bool uns, int slot) {
  Preferences p;
  if (!p.begin("cyd32fw", false)) return;
  p.putString("part", label);
  p.putBool("unsigned", uns);
  p.putInt("slot", slot);
  p.end();
}
}
