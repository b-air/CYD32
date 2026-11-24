# Sonar: distance, indicative angle, creature window

There is **no direction finding**. One omnidirectional antenna gives a signal
strength, not a direction.

## Distance (radius)

- Each report updates a fast-attack EMA in 1/16 dB (`include/rssi_track.h`):
  a stronger sample moves 10/16 of the way, a weaker one 5/16. Approaching
  devices show up within about two packets; a single weak packet does not throw
  a close device to the rim. A device unseen for 5 s restarts from its next
  sample.
- Radius = rim x (-35 - dBm) / 60, clamped. Rings: -50 dBm (dotted **CLOSE**
  ring), -65, -80, rim -95 dBm. A second dotted ring at -42 dBm marks
  **VERY CLOSE**. Echoes inside CLOSE get a highlight ring (yellow for drones).
- A 3-point trail (smoothed position sampled every 700 ms) shows the movement.
- Positions refresh at 10 Hz on the StopWatch and about 7 Hz on the CYD,
  independently of the 4 Hz screen refresh.
- Scanner time slicing (`src/scanner.cpp`): Wi-Fi 3 s / BLE 1.5 s (was 12 s /
  6 s), channel hop 125 ms (was 350 ms) over a 24-step table that visits 1 / 6
  / 11 four times. BLE devices are heard at least every 4.5 s (was 18 s);
  channels 1 / 6 / 11 every 0.5 to 0.75 s (was every 4.55 s). The BLE scan is
  passive with a 100 % window during its slice.

These are estimates: walls, bodies and transmit power change RSSI by 10 dB or
more.

## Modes and proximity beep (batch 6)

The sonar draws one category at a time: ALL, WIFI, BLE, RID, FLOCK, TRK, META
(swipe on the scope; StopWatch: A / B long). Filtering is display-only; the
scanner is unchanged. The proximity beep (MENU > BEEP) follows the pinned
contact, or else the strongest non-drone device of the current mode, and uses
the smoothed RSSI above: silent below -50 dBm (CLOSE ring), 1 beep/s at
-50 dBm, 1 beep every 150 ms from -42 dBm (VERY CLOSE), linear in between
(`include/prox_beep.h`). See [NAVIGATION.md](NAVIGATION.md).

## Wi-Fi start-up (batch 6)

On the StopWatch, Wi-Fi counted on `v0.2.0-test4` and stayed at zero on
`v0.2.0-test5`. No serial log was captured; the most likely cause found in the
code is internal RAM: `v0.2.0-test5` kept the 128 KB LVGL pool in internal RAM
and added about 7 KB of static tables and 4 KB of heap tables there, brought
BLE up before Wi-Fi, and tried `WiFi.mode()` once, never again (Arduino drops
the `esp_err_t`). Now:

- Wi-Fi is initialised first, then NimBLE;
- a failure is probed again with `esp_wifi_init()` / `esp_wifi_start()` to get
  the real `esp_err_t` and the failing step (init, start, promisc), and is
  retried every 30 s while BLE keeps scanning;
- the StopWatch keeps the LVGL pool (128 KB), the detection tables and the
  wardriving log in PSRAM;
- ABOUT shows `WIFI OK` or `WIFI ERR 0x... step #attempt RETRY ns`, the error
  name, and the free internal RAM, largest internal block and largest DMA block
  at the last try. The same values go to the serial boot log.

## Angle (indicative)

Before (v0.2): the sonar position was `mix(id) % 80` mapped to a grid cell,
where `id = SHA-256(random per-boot salt || MAC)`. The position was therefore
different on every device and after every reboot, and RSSI was not used on the
sonar. The StopWatch wardriving view used `crc8(id) * 360 / 256` for the angle.

Now (`src/bearing.cpp`), shared by both targets:

- `h = FNV-1a 32-bit` (offset 0x811C9DC5, prime 0x01000193) over the 6
  address bytes in display order (BLE addresses are reversed from NimBLE's
  little-endian order first).
- angle = `(h * 360) >> 32` (0..359). The GRID screen and its cell
  (`(h >> 8) % 80`) were removed in batch 6; contact labels on the sonar are
  now the first hex digits of the salted per-boot ID, as in the lists.
- Deterministic and unsalted: the same address gets the same angle on every
  CYD32 and after reboots, so two devices side by side agree. The UI says
  "ANGLE INDICATIVE" / "angle is indicative".
- Privacy: at most ~9 bits of the address are exposed by the angle; the
  address is never stored.
- Rule: the angle (an unsalted FNV-1a value) is display-only and must never be
  written to any SD export, serial output, log or file; `scripts/guard.sh`
  fails if it appears in a Serial / log / printf / file-write call. Device IDs
  shown in lists stay salted per boot (`privacy::macId`).
- Randomized addresses (BLE resolvable private addresses, Wi-Fi MAC
  randomization) change every few minutes: the device then moves to a new
  angle and counts as a new device. Random BLE addresses and locally
  administered Wi-Fi addresses get the RND tag.

## Creature window

- Hatches at the first detection (Wi-Fi, BLE or Remote ID). Until then the egg
  waits.
- Size = distinct devices seen in the **last 180 s** (sliding window):
  size 1 from 1, size 2 at 30, size 3 at 60, size 4 at 120
  (`include/creature_rules.h`).
- The window is a fixed RAM table of 192 (salted id, last seen) pairs
  (`include/recent_window.h`), allocated once at boot. A device seen again
  refreshes its time; when full, the oldest entry is replaced. 192 is above the
  size-4 threshold, so saturation never hides growth. Nothing is written to
  flash.
- When the count drops, the creature shrinks (never back to an egg: size 1 is
  the minimum once hatched). Grow and shrink use the same zoom animation.
- PET removes the 25 oldest entries of the window (expired entries are dropped
  first and not counted), so the creature may shrink.
- A rotating random address counts as a new device each time it changes.
- Traits, XP and the Cthulhu stages (drones) are unchanged.

## Prior art and licences

Studied for ideas only; **no code was copied** into CYD32.

| Project | Licence (from its LICENSE file) | Used |
|---|---|---|
| ESP32 Marauder (github.com/justcallmekoko/ESP32Marauder) | MIT | Reviewed for the passive sniffing structure (promiscuous callback, timed channel hopping). The dwell times and hop table above are CYD32's own choices. Its transmit features are not used (CYD32 is receive only). |
| SquachWatch-CYD (github.com/skizzophrenic/SquachWatch-CYD) | GPL-3.0 | Cross-check of public tracker / Flock identifiers (see SIGNATURES.md). GPL-3.0 code could be combined with AGPL-3.0 (GPLv3 section 13), but nothing was taken. |
| Flock-You (github.com/colonelpanichacks/flock-you) | MIT | Cross-check of Flock Safety identifiers. |

The values themselves (UUIDs, company IDs, OUIs, SSID patterns) are public
facts from the Bluetooth SIG and IEEE registries.
