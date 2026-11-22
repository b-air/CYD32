# v9 phosphor sonar screenshots

**UNTESTED ON HARDWARE.** All counters, contacts, IDs, RSSI values, timers and
version strings in these images are **EXAMPLE DATA**.

- `reference/`: the design reference renders (1x, RGB565-quantised) from the
  final v9 design package, including the StopWatch grid (`sw-03-grid.png`).
- `firmware-host/`: the real `src/ui.cpp` / `src/ui_round.cpp` /
  `src/ui_radar.cpp` rendered on a PC with LVGL 9.2.2 (32-bit build, the
  board's `lv_conf.h`, 40-line partial draw buffer) and stubbed data. On the
  StopWatch, pixels outside the r = 233 circle are blanked, as on the round
  panel.
- `test3-byteswap/`: the `v0.2.0-test3` StopWatch UI rendered the same way,
  but with each RGB565 pixel byte-swapped as M5GFX did on the watch
  (`pushImage()` with a plain `uint16_t *` reads the buffer as big-endian
  `swap565`). These match the hardware photos of `v0.2.0-test3`: green
  bearings in blue, dim greens in tan / pale yellow, G5 text in magenta,
  drones in violet. Fixed after `v0.2.0-test3`; kept to document the bug.
- The StopWatch `firmware-host/` images were re-rendered after
  `v0.2.0-test3` (full-panel layout, larger text, A / B key tags at 310 /
  050 deg, tab band on every screen, outlined menu).
- `firmware-host-b5/`: batch 5 (A / B navigation, 8-entry menu, RSSI sonar
  with indicative angle, CLOSE / VERY CLOSE rings and trails, TRACKERS,
  DRONES, DEVICE DETAIL, dashboard pages, creature egg / size 2 / size 4)
  for both targets, rendered the same way from the real UI, detections and
  creature code with synthetic addresses (`sw-02`: after A short on SONAR;
  `sw-16`: after A long then B long). The superseded sonar, grid, creature,
  menu, counters, details and wardriving images were removed from
  `firmware-host/`; boot, about, update, OTA and unsigned are unchanged.
