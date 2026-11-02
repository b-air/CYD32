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
bool sameSha(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 32) == 0; }
}
namespace fw_status {
void begin() {
  // Default for a USB-flashed image: identified by the build's slot-0 key.
  int slot = 0;
  bool uns = false;
  Preferences p;
  if (p.begin("cyd32fw", false)) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_app_desc_t d;
    uint8_t rec[32] = {0};
    const bool have = p.getBytes("sha", rec, 32) == 32;
    if (run && have && esp_ota_get_partition_description(run, &d) == ESP_OK &&
        p.getString("part", "") == run->label && sameSha(rec, d.app_elf_sha256)) {
      uns = p.getBool("unsigned", false);
      slot = p.getInt("slot", 0);
    } else if (have) {
      p.clear();  // running image is not the one the OTA record describes (e.g. USB reflash)
    }
    p.end();
  }
  if (uns) { k = UNSIGNED; strcpy(fpr, "----------------"); return; }
  strcpy(fpr, slot == 1 ? CYD32_KEY1_FPR : CYD32_KEY0_FPR);
  const bool keyPresent = slot == 1 ? CYD32_KEY1_PRESENT : CYD32_KEY0_PRESENT;
  k = (CYD32_OFFICIAL && keyPresent && strcmp(fpr, CYD32_OFFICIAL_FPR) == 0) ? OFFICIAL : FORK;
}
Kind kind() { return k; }
const char *kindStr() { return k == OFFICIAL ? "Official" : k == FORK ? "Fork" : "Unsigned"; }
const char *keyFingerprint() { return fpr; }
void recordNextBoot(const esp_partition_t *target, bool uns, int slot) {
  esp_app_desc_t d;
  if (esp_ota_get_partition_description(target, &d) != ESP_OK) return;
  Preferences p;
  if (!p.begin("cyd32fw", false)) return;
  p.putString("part", target->label);
  p.putBytes("sha", d.app_elf_sha256, 32);
  p.putBool("unsigned", uns);
  p.putInt("slot", slot);
  p.end();
}
}
