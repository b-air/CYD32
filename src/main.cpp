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
#include "ui_layout.h"
#include "boot_log.h"
#include "cyd32_build_gen.h"
#ifdef CYD32_HW_M5STACK_STOPWATCH
#include "wardriving.h"
#include <esp_heap_caps.h>
#endif

// Keep the new image in PENDING_VERIFY until our own self-test passes, so the
// bootloader rolls back to the previous slot if this firmware cannot start.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
#ifdef CYD32_HW_M5STACK_STOPWATCH
constexpr int kBufLines = 233;  // half-height; allocated from PSRAM
static uint8_t *drawBuf = nullptr;
static size_t drawBufSz = 0;
#else
constexpr int kBufLines = 24;
// LVGL 9 asserts that the draw buffer is LV_DRAW_BUF_ALIGN-aligned. A plain
// uint8_t array has 1-byte alignment and was linked at an odd address in
// v0.2.0-test1, so lv_display_set_buffers() halted in LVGL's default
// while(1) assert handler: backlight on, black screen, no further output.
alignas(LV_DRAW_BUF_ALIGN) uint8_t drawBuf[CYD32_SCREEN_W * kBufLines * 2];
static_assert(LV_DRAW_BUF_ALIGN <= 16, "unexpected LVGL draw buffer alignment");
#endif
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
bool bufAligned(const void *p) { return p && ((uintptr_t)p % LV_DRAW_BUF_ALIGN) == 0; }
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
  BOOT_LOG("CYD32 %s (%s) target=%s", CYD32_VERSION, CYD32_COMMIT, CYD32_TARGET);
  BOOT_LOG("heap free=%u", (unsigned)ESP.getFreeHeap());
  BOOT_STEP("privacy", privacy::begin());
  BOOT_STEP("detections", det::begin());
  BOOT_STEP("creature", creature::begin());
  BOOT_STEP("fw_status", fw_status::begin());
  BOOT_STEP("hw", hw::begin());
  uint8_t *buf = nullptr;
#ifdef CYD32_HW_M5STACK_STOPWATCH
  BOOT_STEP("wardriving", wardriving::begin());
  drawBufSz = (size_t)CYD32_SCREEN_W * kBufLines * 2;
  drawBuf = (uint8_t *)heap_caps_malloc(drawBufSz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!drawBuf) drawBuf = (uint8_t *)malloc(drawBufSz);
  buf = drawBuf;
  const size_t bufSz = drawBufSz;
#else
  buf = drawBuf;
  const size_t bufSz = sizeof drawBuf;
#endif
  BOOT_LOG("draw buffer %p, %u bytes", (void *)buf, (unsigned)bufSz);
  if (!bufAligned(buf)) {
    // Never hand LVGL a bad buffer: its assert would abort and reset in a loop.
    BOOT_LOG("draw buffer missing or not %d-byte aligned, trying an aligned heap buffer", LV_DRAW_BUF_ALIGN);
    buf = (uint8_t *)heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bufSz, MALLOC_CAP_8BIT);
    BOOT_LOG("heap draw buffer %p", (void *)buf);
    if (!bufAligned(buf)) hw::fatal("draw buffer allocation failed");
#ifdef CYD32_HW_M5STACK_STOPWATCH
    drawBuf = buf;
#endif
  }
  BOOT_STEP("lv_init", lv_init());
  lv_tick_set_cb(tick);
  lv_display_t *disp = nullptr;
  BOOT_STEP("lv_display_create", disp = lv_display_create(CYD32_SCREEN_W, CYD32_SCREEN_H));
  lv_display_set_flush_cb(disp, flushCb);
  BOOT_STEP("lv_display_set_buffers",
            lv_display_set_buffers(disp, buf, nullptr, bufSz, LV_DISPLAY_RENDER_MODE_PARTIAL));
  lv_indev_t *in = lv_indev_create();
  lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(in, touchCb);
  BOOT_STEP("ui", ui::begin());
  BOOT_STEP("first frame", pump(1));

  // Physical gesture for the one-shot unsigned mode: hold the board button
  // (BOOT / GPIO0 on CYD, KEYA / GPIO2 on StopWatch) for 3 s during the splash.
  // The splash waits while the button is held, so a full 3 s hold always fits.
  // On CYD, pressing BOOT *before* reset enters the ROM USB bootloader instead.
  constexpr uint32_t kHoldMs = 3000;
  uint32_t held = 0;
  for (int i = 0; i <= 30 || (held > 0 && held < kHoldMs); i++) {
    const int p = i < 30 ? i : 30;
    ui::setBootProgress(p * 100 / 30, p < 10 ? "BLE OK" : p < 20 ? "WIFI OK" : "RID OK");
    held = hw::bootButtonPressed() ? held + 100 : 0;
    pump(100);
    if (held >= kHoldMs) break;
  }
  if (held >= kHoldMs) {
    ui::show(ui::UNSIGNED_CONFIRM);
    int d = -1;
    const uint32_t t0 = millis();
    while (d < 0 && millis() - t0 < 30000) { pump(20); d = ui::takeUnsignedDecision(); }
    unsignedArmed = d == 1;  // RAM only: valid for this boot and one update
    ui::setUnsignedBanner(unsignedArmed);
  }
  BOOT_STEP("scanner", scanner::begin());
  BOOT_STEP("ui sonar", ui::show(ui::SONAR));
  BOOT_LOG("setup done in %lu ms, heap free=%u", (unsigned long)(millis() - bootMs), (unsigned)ESP.getFreeHeap());
}

void loop() {
  lv_timer_handler();
  const uint32_t now = millis();
  if (otaMode) {
    ota_portal::loop();
  } else {
    scanner::loop();
#ifdef CYD32_HW_M5STACK_STOPWATCH
    wardriving::loop();
#endif
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
