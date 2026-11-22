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

## Angle (indicative)

Before (v0.2): the sonar position was `mix(id) % 80` mapped to a grid cell,
where `id = SHA-256(random per-boot salt || MAC)`. The position was therefore
different on every device and after every reboot, and RSSI was not used on the
sonar. The StopWatch wardriving view used `crc8(id) * 360 / 256` for the angle.

Now (`src/bearing.cpp`), shared by both targets:

- `h = FNV-1a 32-bit` (offset 0x811C9DC5, prime 0x01000193) over the 6
  address bytes in display order (BLE addresses are reversed from NimBLE's
  little-endian order first).
- angle = `(h * 360) >> 32` (0..359); GRID cell = `(h >> 8) % 80`.
- Deterministic and unsalted: the same address gets the same angle and cell on
  every CYD32 and after reboots, so two devices side by side agree. The UI says
  "ANGLE INDICATIVE" / "angle is indicative".
- Privacy: at most ~15 bits combined (angle + cell) of the address are
  exposed; the address is never stored.
- Rule: the angle and grid cell (unsalted FNV-1a values) are display-only and must never be written to any SD export, serial output, log or file; `scripts/guard.sh` fails if they appear in a Serial / log / printf / file-write call. Device IDs shown in lists stay salted
  per boot (`privacy::macId`).
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
