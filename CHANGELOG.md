<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# Changelog

All builds so far are **pre-releases** for testing, with two targets each:
`cyd` (ESP32-2432S028) and `m5stack-stopwatch` (M5Stack StopWatch, C152).
The firmware is receive-only in every build. The repository is private: the
release links work only when you are signed in to GitHub with access.
Install steps: [docs/INSTALL.md](docs/INSTALL.md).

## v0.2.0-test6 — 24 Nov 2026 (commit `8bb5917118a2`, #13)

Unsigned (USB install only). **Not tested on hardware yet.**

- StopWatch Wi-Fi start-up fix: Wi-Fi starts before BLE, real error codes and
  failing step logged, retry every 30 s; LVGL pool, detection tables and
  wardriving log in PSRAM (about 160 KB of internal RAM freed). ABOUT shows
  the Wi-Fi status and free RAM on both boards.
- Tabs SONAR / DASH / CREA / MENU; 6-entry menu (SONAR, COUNTERS, WARDRIVE,
  CREATURE, BEEP, OTA).
- 7 sonar modes (ALL, WIFI, BLE, RID, FLOCK, TRK, META): swipe on the scope,
  or hold A / B on the StopWatch. Display-only.
- Dashboard with one view at a time: WIFI, BLE, RID, TAGGED, LOG.
- Proximity beep (MENU > BEEP): StopWatch speaker; CYD on the SPEAK connector
  (GPIO 26) with a small speaker plugged in.
- Creature: larger sizes, PET forgets the 25 oldest devices, FEED drone
  animation.
- Bootloader, partitions and boot_app0 identical to `v0.2.0-test5`: a board
  on `v0.2.0-test5` updates with the firmware file only.
- [Release](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test6)

## v0.2.0-test5 — 22 Nov 2026 (commit `ad2c8c5e9939`, #11)

Unsigned. Hardware: installed over USB on both boards (23 Nov 2026), Wi-Fi OK on the CYD; on the
StopWatch Wi-Fi did not start (`W!`), fixed in `v0.2.0-test6`.

- StopWatch keys: A = next, B = select, held = back / home, A + B = menu.
- SONAR as the home screen; sonar distance from the smoothed RSSI with
  CLOSE / VERY CLOSE rings and trails; indicative angle.
- TRK / META / FLOCK badges, DEVICE DETAIL, a first dashboard, creature size
  from the devices seen in the last 3 minutes.
- Faster scanner slices (Wi-Fi 3 s / BLE 1.5 s, 125 ms channel hops).
- [Release](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test5)

## v0.2.0-test4 — 18 Nov 2026 (commit `9b467d5b01b2`, #8)

Unsigned. Hardware: the StopWatch received Wi-Fi with this build.

- StopWatch colours fixed (RGB565 byte order), full-panel layout, larger
  text, tabs on every screen, outlined menu, A / B tags at the keys, more
  reliable touch and buttons. CYD application code unchanged.
- Releases are marked pre-release, never "Latest" (#9).
- [Release](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test4)

## v0.2.0-test3 — 18 Nov 2026 (commit `c9042fc8dafe`, #6)

Unsigned. Hardware: worked on the CYD; on the StopWatch it booted but the
colours, layout, menu and touch were wrong (fixed in `v0.2.0-test4`).

- StopWatch black screen fixed (first frame drawn), Wi-Fi reception fixed
  (receive-only, management frames only, `W!` on failure), CYD touch X axis
  fixed.
- Green phosphor sonar theme on both boards.
- [Release](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test3)

## v0.2.0-test2 — 18 Nov 2026 (commit `13e5b8ab543a`, #5)

Unsigned. Hardware: StopWatch screen stayed black (fixed in `v0.2.0-test3`).

- CYD boot hang fixed (LVGL draw buffer alignment), start-up logs on the
  serial console, visible fatal errors, unsigned-update hold of 3 s.
- [Release](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test2)

## v0.2.0-test1 — 13 Nov 2026 (commit `d79be3f4d7e4`, #1 to #3)

The only build with signed images (`*-signed.bin`, for OTA). Hardware: on the
CYD the backlight came on but the screen stayed black (fixed in
`v0.2.0-test2`).

- First passive sonar firmware with signed OTA, the creatures, and the
  M5Stack StopWatch target.
- [Release](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test1)
