// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#ifndef CYD32_HW_M5STACK_STOPWATCH
#include "hw.h"
#include <Arduino.h>
#include "boot_log.h"
#include "ota_rollback.h"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

namespace {
// Pin map of the ESP32-2432S028 (single USB variant): display on HSPI, touch on its own SPI pins.
class LGFX_CYD : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;
 public:
  LGFX_CYD() {
    { auto c = bus_.config();
      c.spi_host = HSPI_HOST; c.spi_mode = 0; c.freq_write = 40000000; c.freq_read = 16000000;
      c.spi_3wire = false; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 14; c.pin_mosi = 13; c.pin_miso = 12; c.pin_dc = 2;
      bus_.config(c); panel_.setBus(&bus_); }
    { auto c = panel_.config();
      c.pin_cs = 15; c.pin_rst = -1; c.pin_busy = -1;
      c.panel_width = 240; c.panel_height = 320; c.offset_rotation = 0;
      c.readable = true; c.invert = false; c.rgb_order = false; c.dlen_16bit = false; c.bus_shared = false;
      panel_.config(c); }
    { auto c = light_.config();
      c.pin_bl = 21; c.invert = false; c.freq = 44100; c.pwm_channel = 7;
      light_.config(c); panel_.setLight(&light_); }
    { auto c = touch_.config();
      // Landscape (rotation 1) maps raw Y to screen X: y_min > y_max un-mirrors X.
      c.x_min = 300; c.x_max = 3900; c.y_min = 3700; c.y_max = 200;
      c.pin_int = 36; c.bus_shared = false; c.offset_rotation = 0;
      c.spi_host = VSPI_HOST; c.freq = 1000000;
      c.pin_sclk = 25; c.pin_mosi = 32; c.pin_miso = 39; c.pin_cs = 33;
      touch_.config(c); panel_.setTouch(&touch_); }
    setPanel(&panel_);
  }
};
LGFX_CYD tft;
}  // namespace

namespace hw {
void begin() {
  pinMode(0, INPUT_PULLUP);
  BOOT_STEP("tft.init (ILI9341 HSPI, XPT2046 VSPI)", tft.init());
  tft.setRotation(1);  // landscape 320x240
  BOOT_STEP("backlight", tft.setBrightness(200));
  BOOT_STEP("fillScreen", tft.fillScreen(0x0000));
}
bool touch(int16_t &x, int16_t &y) {
  lgfx::touch_point_t tp;
  if (tft.getTouch(&tp, 1)) { x = tp.x; y = tp.y; return true; }
  return false;
}
void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *px) {
  tft.pushImage(x, y, w, h, reinterpret_cast<const lgfx::rgb565_t *>(px));
}
bool bootButtonPressed() { return digitalRead(0) == LOW; }
void fatal(const char *msg) {
  BOOT_LOG("FATAL: %s. Halted.", msg);
  tft.fillScreen(0x0000);
  tft.setTextColor(0xF800, 0x0000);  // red on black
  tft.setTextSize(2);
  tft.drawString("CYD32 FATAL ERROR", 8, 8);
  tft.setTextSize(1);
  tft.drawString(msg, 8, 40);
  tft.drawString("Halted. See the serial console (115200).", 8, 56);
  ota_rollback::ifPendingVerify("fatal error", 3000);
  for (;;) delay(1000);  // stay on this screen: no reset loop
}
}  // namespace hw
#endif
