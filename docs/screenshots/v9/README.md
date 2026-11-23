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
- `firmware-host-b6/`: batch 6 (`v0.2.0-test6` candidate) for both targets,
  rendered the same way from the real UI, detections, beep and creature code
  with synthetic addresses: SONAR in ALL mode and in TRK mode with the
  proximity BEEP on (`02`), DASHBOARD (COUNTERS, then the WIFI, BLE, RID,
  TAGGED and LOG views), the 6-entry MENU (SONAR, COUNTERS, WARDRIVE,
  CREATURE, BEEP, OTA) with BEEP off and on, WARDRIVE, DEVICE DETAIL, ABOUT
  with Wi-Fi OK and with an example Wi-Fi start error (`10b`), OTA confirm /
  OTA, and the creature at size 1 (after PET), size 4, and the FEED drone in
  flight and being eaten (`14`, `14b`). The grid, TORPEDO and the old
  TRACKERS / DRONES / filter screens no longer exist and batch 5
  (`firmware-host-b5/`) was removed. The design `reference/` still shows the
  v9 grid; it is not in the firmware any more. `firmware-host/` keeps boot,
  about, update, OTA and unsigned (ABOUT now also has the Wi-Fi status
  lines shown in `firmware-host-b6/`).
