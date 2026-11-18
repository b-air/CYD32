// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// M5Stack StopWatch Dev Kit (SKU C152, ESP32-S3R8). Pins verified against
// https://docs.m5stack.com/en/core/StopWatch (KEYA=GPIO2, KEYB=GPIO1,
// CO5300 QSPI, CST820B on G47/G48/G13). Display reset and touch reset are
// on the M5IOE1 expander and are handled by M5Unified / M5GFX.
#ifdef CYD32_HW_M5STACK_STOPWATCH
#include "hw.h"
#include "ui_layout.h"
#include <Arduino.h>
#include <M5Unified.h>
#include "boot_log.h"
#include "ota_rollback.h"
#include "lvgl.h"
#include <math.h>

namespace {
// One M5.update() per poll window. Every M5.update() recomputes the button
// edges, so the former pattern (M5.update() inside each getter, called from
// the LVGL touch read and from the 50 ms button timer) dropped KEYA / KEYB
// clicks and touch edges that were seen by another getter's update. Edges are
// latched here and consumed by btnAClicked() / btnBClicked().
uint32_t lastPollMs = 0;
bool polled = false, latchA = false, latchB = false;
void poll() {
  const uint32_t now = millis();
  if (polled && now - lastPollMs < 8) return;
  polled = true;
  lastPollMs = now;
  M5.update();
  if (M5.BtnA.wasClicked()) latchA = true;
  if (M5.BtnB.wasClicked()) latchB = true;
}
// Touch serial log: press / release edges, plus one line every 500 ms while held.
bool wasDown = false;
uint32_t downMs = 0, lastHoldLog = 0;
int16_t lastX = 0, lastY = 0;
}  // namespace

namespace hw {
void begin() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  BOOT_STEP("M5.begin (CO5300, CST820B, PMIC)", M5.begin(cfg));
  M5.Display.setRotation(0);  // native round panel
  BOOT_STEP("brightness", M5.Display.setBrightness(200));
  BOOT_STEP("fillScreen", M5.Display.fillScreen(0x0000));
  auto *t = M5.Display.touch();
  if (t) {
    const auto c = t->config();
    BOOT_LOG("touch: init ok=%d i2c=0x%02X int=%d range x %d..%d y %d..%d rot=%d panel=%dx%d",
             (int)M5.Touch.isEnabled(), (unsigned)c.i2c_addr, (int)c.pin_int, (int)c.x_min, (int)c.x_max,
             (int)c.y_min, (int)c.y_max, (int)c.offset_rotation, (int)M5.Display.width(), (int)M5.Display.height());
  } else {
    BOOT_LOG("touch: init FAILED (no touch controller found)");
  }
}
bool touch(int16_t &x, int16_t &y) {
  poll();
  const auto t = M5.Touch.getDetail();
  const bool down = M5.Touch.getCount() > 0 && t.isPressed();
  const uint32_t now = millis();
  if (down) {
    // Whole panel accepted (the curved tabs reach r = 225; a press that
    // slides onto the bezel must not turn into a release mid-gesture).
    x = (int16_t)LV_CLAMP(0, (int)t.x, CYD32_SCREEN_W - 1);
    y = (int16_t)LV_CLAMP(0, (int)t.y, CYD32_SCREEN_H - 1);
    lastX = x; lastY = y;
    if (!wasDown || now - lastHoldLog >= 500) {
      const auto r = M5.Touch.getTouchPointRaw(0);
      const int dx = x - CYD32_CX, dy = y - CYD32_CY;
      BOOT_LOG("touch: %s raw=%d,%d xy=%d,%d r=%d", wasDown ? "hold" : "down", (int)r.x, (int)r.y, (int)x, (int)y,
               (int)sqrtf((float)(dx * dx + dy * dy)));
      lastHoldLog = now;
      if (!wasDown) downMs = now;
    }
  } else if (wasDown) {
    BOOT_LOG("touch: up xy=%d,%d after %lu ms", (int)lastX, (int)lastY, (unsigned long)(now - downMs));
  }
  wasDown = down;
  return down;
}
void touchHit(const char *what) { BOOT_LOG("touch: hit %s", what); }
void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *px) {
  // LVGL renders RGB565 in native (little-endian) byte order. M5GFX treats a
  // plain uint16_t* as swap565 (big-endian, "already swapped") unless
  // setSwapBytes(true), so the former uint16_t* cast byte-swapped every pixel
  // (green -> blue / violet, dim green -> tan, v0.2.0-test3). rgb565_t* is
  // the native-order type, the same one the CYD flush uses.
  // Synchronous CPU copy into M5GFX's PSRAM frame buffer (Panel_AMOLED_Framebuffer);
  // auto-display then streams the dirty rectangle, rounded to even bounds,
  // to the CO5300 through internal DMA line buffers. No DMA reads our buffer,
  // so there is nothing to wait for before lv_display_flush_ready().
  M5.Display.pushImage(x, y, w, h, reinterpret_cast<const m5gfx::rgb565_t *>(px));
}
bool bootButtonPressed() {
  poll();
  return M5.BtnA.isPressed();  // KEYA (yellow), GPIO2, active low
}
bool btnAClicked() { poll(); const bool c = latchA; latchA = false; return c; }
bool btnBClicked() { poll(); const bool c = latchB; latchB = false; return c; }
bool btnAPressed() { poll(); return M5.BtnA.isPressed(); }
bool btnBPressed() { poll(); return M5.BtnB.isPressed(); }
int batteryPct() { return (int)M5.Power.getBatteryLevel(); }
void fatal(const char *msg) {
  BOOT_LOG("FATAL: %s. Halted.", msg);
  M5.Display.fillScreen(0x0000);
  M5.Display.setTextColor(0xF800, 0x0000);  // red on black
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("CYD32 FATAL ERROR", CYD32_CX, CYD32_CY - 30);
  M5.Display.setTextSize(1);
  M5.Display.drawString(msg, CYD32_CX, CYD32_CY + 4);
  M5.Display.drawString("Halted. See the serial console.", CYD32_CX, CYD32_CY + 24);
  ota_rollback::ifPendingVerify("fatal error", 3000);
  for (;;) delay(1000);  // stay on this screen: no reset loop
}
}  // namespace hw
#endif
