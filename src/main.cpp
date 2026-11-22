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
#include "ota_rollback.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include "cyd32_build_gen.h"
#ifdef CYD32_HW_M5STACK_STOPWATCH
#include "wardriving.h"
#endif

// Keep the new image in PENDING_VERIFY until our own self-test passes, so the
// bootloader rolls back to the previous slot if this firmware cannot start.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
#ifdef CYD32_HW_M5STACK_STOPWATCH
// 40 lines (37 KB) from internal DMA-capable SRAM: faster to render into than
// PSRAM. Even, so every band starts on an even row (CO5300 needs even bounds).
constexpr int kBufLines = 40;
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

// First-frame diagnostics: the first few flushes are logged, and a one-shot
// timer reports (and rolls back an unverified OTA image) if the first frame
// has not completed after kFirstFrameTimeoutUs.
constexpr uint32_t kFlushLogs = 3;
constexpr uint64_t kFirstFrameTimeoutUs = 5 * 1000 * 1000;
volatile uint32_t flushCount = 0;
volatile bool firstFrameDone = false;
esp_timer_handle_t firstFrameTimer = nullptr;

void flushCb(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  const uint32_t n = ++flushCount;
  const int32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  if (n <= kFlushLogs) BOOT_LOG("flush #%u x=%ld y=%ld %ldx%ld ...", (unsigned)n, (long)a->x1, (long)a->y1, (long)w, (long)h);
  if (px != nullptr && w > 0 && h > 0) hw::flush(a->x1, a->y1, w, h, px);
  if (n <= kFlushLogs) BOOT_LOG("flush #%u ok", (unsigned)n);
  lv_display_flush_ready(d);  // on every path, or LVGL waits forever
}
void firstFrameTimeout(void *) {
  if (firstFrameDone) return;
  // Runs in the esp_timer task while the main loop is stuck inside LVGL.
  BOOT_LOG("first frame NOT done after %u s: %u flushes, internal free=%u, psram free=%u",
           (unsigned)(kFirstFrameTimeoutUs / 1000000), (unsigned)flushCount,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  ota_rollback::ifPendingVerify("first frame hang", 0);
}
void armFirstFrameTimer() {
  const esp_timer_create_args_t args = {firstFrameTimeout, nullptr, ESP_TIMER_TASK, "first_frame", false};
  if (esp_timer_create(&args, &firstFrameTimer) == ESP_OK) esp_timer_start_once(firstFrameTimer, kFirstFrameTimeoutUs);
}
#ifdef CYD32_HW_M5STACK_STOPWATCH
// The CO5300 needs even start coordinates and even sizes: widen every
// invalidated area to even x1/y1 and odd x2/y2 (x2, y2 are inclusive).
void evenAreaCb(lv_event_t *e) {
  lv_area_t *a = static_cast<lv_area_t *>(lv_event_get_param(e));
  a->x1 &= ~1; a->y1 &= ~1; a->x2 |= 1; a->y2 |= 1;
}
// LVGL's printf goes to UART0; the StopWatch console is USB CDC. A failing
// render retries in a tight loop, so keep at most 10 lines per second.
void lvLogCb(lv_log_level_t, const char *buf) {
  static uint32_t windowMs = 0, lines = 0;
  const uint32_t now = millis();
  if (now - windowMs >= 1000) { windowMs = now; lines = 0; }
  if (++lines > 10) return;
  Serial.print("[lvgl] ");
  Serial.print(buf);
}
#endif
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
  drawBuf = (uint8_t *)heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, drawBufSz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  const char *where = "internal DMA";
  if (!drawBuf) {
    // Fallback: PSRAM is fine too, hw::flush() copies with the CPU (no DMA from this buffer).
    drawBuf = (uint8_t *)heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, drawBufSz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    where = "PSRAM";
  }
  BOOT_LOG("draw buffer in %s, %d lines", where, kBufLines);
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
#ifdef CYD32_HW_M5STACK_STOPWATCH
  lv_log_register_print_cb(lvLogCb);
#endif
  lv_display_t *disp = nullptr;
  BOOT_STEP("lv_display_create", disp = lv_display_create(CYD32_SCREEN_W, CYD32_SCREEN_H));
  lv_display_set_flush_cb(disp, flushCb);
#ifdef CYD32_HW_M5STACK_STOPWATCH
  lv_display_add_event_cb(disp, evenAreaCb, LV_EVENT_INVALIDATE_AREA, nullptr);
#endif
  BOOT_STEP("lv_display_set_buffers",
            lv_display_set_buffers(disp, buf, nullptr, bufSz, LV_DISPLAY_RENDER_MODE_PARTIAL));
  lv_indev_t *in = lv_indev_create();
  lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(in, touchCb);
  BOOT_STEP("ui", ui::begin());
  armFirstFrameTimer();
  BOOT_STEP("first frame", lv_refr_now(disp));
  firstFrameDone = true;
  if (firstFrameTimer) { esp_timer_stop(firstFrameTimer); esp_timer_delete(firstFrameTimer); firstFrameTimer = nullptr; }
  BOOT_LOG("first frame: %u flushes", (unsigned)flushCount);

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
  BOOT_STEP("ui home", ui::show(ui::HOME));
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
    static uint32_t lastWd = 0;  // the RAM log does not need more than 2 Hz
    if (now - lastWd >= 500) { lastWd = now; wardriving::loop(); }
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
    creature::update(c.recent3m, c.unique24h, (uint16_t)c.droneEver, c.seenTraits);
    ui::refresh();
  }
  selfTest();
  delay(2);
}
