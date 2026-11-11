<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# StopWatch HUD design mockups (v8)

**Design mockup, example data, not a device capture.**

These 466x466 images (and the planche) are the approved round-HUD layouts for
the M5Stack StopWatch. Every number, identifier, battery reading and progress
value on them is EXAMPLE DATA and must not be treated as a measurement or as
something the firmware hard-codes.

The firmware draws the chrome at run time from live detections, creature state
and OTA status. It does not embed these PNGs.

Files:

- `screens/` — boot, sonar, grid, detail, wardriving, OTA, creature, evolution,
  easter egg, radial menu, W/B/R counters
- `overlays/` — button mapping and 48 px touch zones
- `planche-stopwatch.png` — overview board
