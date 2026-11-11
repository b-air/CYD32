// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// CYD32: educational, passive (receive-only) RF sonar for the ESP32-2432S028.
#include <Arduino.h>
#include <esp_ota_ops.h>
#include "lvgl.h"
#include "creature.h"
#include "detections.h"
#include "fw_status.h"
#include "hw.h"
#include "ota_portal.h"
#include "privacy.h"
#include "scanner.h"
#include "ui.h"

// Keep the new image in PENDING_VERIFY until our own self-test passes, so the
// bootloader rolls back to the previous slot if this firmware cannot start.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
constexpr int kBufLines = 24;
uint8_t drawBuf[320 * kBufLines * 2];
bool otaMode = false, unsignedArmed = false, selfTestDone = false;
uint32_t bootMs = 0, lastRefresh = 0;

uint32_t tick() { return millis(); }
void flushCb(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  hw::flush(a->x1, a->y1, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1, px);
  lv_display_flush_ready(d);
}
void touchCb(lv_indev_t *, lv_indev_data_t *data) {
  int16_t x, y;
  if (hw::touch(x, y)) { data->point.x = x; data->point.y = y; data->state = LV_INDEV_STATE_PRESSED; }
  else data->state = LV_INDEV_STATE_RELEASED;
}
void pump(uint32_t ms) { const uint32_t e = millis() + ms; while ((int32_t)(millis() - e) < 0) { lv_timer_handler(); delay(5); } }

void selfTest() {
  // Runs once the UI has been alive for 8 s: display pipeline, LVGL heap and system heap.
  if (selfTestDone || millis() - bootMs < 8000) return;
  selfTestDone = true;
  lv_mem_monitor_t m; lv_mem_monitor(&m);
  const bool ok = ESP.getFreeHeap() > 20000 && m.free_size > 4096 && lv_display_get_default() != nullptr &&
                  (otaMode || scanner::healthy());
  esp_ota_img_states_t s;
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(run, &s) == ESP_OK && s == ESP_OTA_IMG_PENDING_VERIFY) {
    if (ok) esp_ota_mark_app_valid_cancel_rollback();
    else esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  bootMs = millis();
  privacy::begin();
  det::begin();
  creature::begin();
  fw_status::begin();
  hw::begin();
  lv_init();
  lv_tick_set_cb(tick);
  lv_display_t *disp = lv_display_create(320, 240);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, drawBuf, nullptr, sizeof drawBuf, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_indev_t *in = lv_indev_create();
  lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(in, touchCb);
  ui::begin();

  // Physical gesture for the one-shot unsigned mode: hold BOOT during the 3 s splash
  // (pressing BOOT *before* reset enters the ROM USB bootloader instead).
  uint32_t held = 0;
  for (int i = 0; i <= 30; i++) {
    ui::setBootProgress(i * 100 / 30, i < 10 ? "BLE OK" : i < 20 ? "WIFI OK" : "RID OK");
    held = hw::bootButtonPressed() ? held + 100 : 0;
    pump(100);
    if (held >= 2500) break;
  }
  if (held >= 2500) {
    ui::show(ui::UNSIGNED_CONFIRM);
    int d = -1;
    const uint32_t t0 = millis();
    while (d < 0 && millis() - t0 < 30000) { pump(20); d = ui::takeUnsignedDecision(); }
    unsignedArmed = d == 1;  // RAM only: valid for this boot and one update
    ui::setUnsignedBanner(unsignedArmed);
  }
  scanner::begin();
  ui::show(ui::SONAR);
}

void loop() {
  lv_timer_handler();
  const uint32_t now = millis();
  if (otaMode) {
    ota_portal::loop();
  } else {
    scanner::loop();
    if (ui::takeUpdateRequest()) {
      scanner::stop();
      ota_portal::start(unsignedArmed);
      unsignedArmed = false;
      otaMode = true;
      ui::show(ui::OTA);
    }
  }
  if (now - lastRefresh >= 250) {
    lastRefresh = now;
    const det::Counts c = det::counts();
    creature::update(c.unique24h, (uint16_t)c.droneEver, c.seenTraits);
    ui::refresh();
  }
  selfTest();
  delay(2);
}
