# v9 phosphor sonar screenshots

**UNTESTED ON HARDWARE.** All counters, contacts, IDs, RSSI values, timers and
version strings in these images are **EXAMPLE DATA**.

- `reference/`: the design reference renders (1x, RGB565-quantised) for the
  screens that use the full phosphor theme.
- `firmware-host/`: the real `src/ui.cpp` / `src/ui_round.cpp` /
  `src/ui_radar.cpp` rendered on a PC with LVGL 9.2.2 (32-bit build, the
  board's `lv_conf.h`, 40-line partial draw buffer) and stubbed data. On the
  StopWatch, pixels outside the r = 233 circle are blanked, as on the round
  panel.
