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

namespace hw {
void begin() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  BOOT_STEP("M5.begin (CO5300, CST820B, PMIC)", M5.begin(cfg));
  M5.Display.setRotation(0);  // native round 466x466
  BOOT_STEP("brightness", M5.Display.setBrightness(200));
  BOOT_STEP("fillScreen", M5.Display.fillScreen(0x0000));
}
bool touch(int16_t &x, int16_t &y) {
  M5.update();
  const auto t = M5.Touch.getDetail();
  if (!t.isPressed()) return false;
  x = (int16_t)t.x;
  y = (int16_t)t.y;
  const int dx = (int)x - CYD32_CX, dy = (int)y - CYD32_CY;
  // Ignore presses in the 20 px bezel crop (outside the r=213 safe circle).
  if (dx * dx + dy * dy > CYD32_R_SAFE * CYD32_R_SAFE) return false;
  return true;
}
void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *px) {
  M5.Display.pushImage(x, y, w, h, reinterpret_cast<const uint16_t *>(px));
}
bool bootButtonPressed() {
  M5.update();
  return M5.BtnA.isPressed();  // KEYA (yellow), GPIO2, active low
}
bool btnAClicked() { M5.update(); return M5.BtnA.wasClicked(); }
bool btnBClicked() { M5.update(); return M5.BtnB.wasClicked(); }
bool btnAPressed() { M5.update(); return M5.BtnA.isPressed(); }
bool btnBPressed() { M5.update(); return M5.BtnB.isPressed(); }
int batteryPct() { return (int)M5.Power.getBatteryLevel(); }
}  // namespace hw
#endif
