<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# CYD32

Educational, **passive (receive-only)** radio "sonar" firmware. Two board
targets are built from this repository:

- `cyd` — ESP32-2432S028 "Cheap Yellow Display" (ILI9341 320x240, XPT2046
  touch, 4 MB flash).
- `m5stack-stopwatch` — M5Stack StopWatch Dev Kit, SKU C152 (ESP32-S3R8,
  16 MB flash, 8 MB OPI PSRAM, 466x466 round CO5300 AMOLED, CST820B touch).

It listens to Wi-Fi management frames and Bluetooth Low Energy advertisements,
draws them on a naval-sonar UI and feeds a small creature that grows as it
meets distinct radio sources. Four creatures (FENNI, LUMO, KRABBI, POULPI)
hatch, idle and react; they grow with unique detections (24 h address dedup)
and take on a Cthulhu form as they meet drones broadcasting Remote ID.
On the StopWatch the v7 sprites are reused and upscaled with an integer
nearest-neighbour blit at 3x (192 px) into a PSRAM buffer.

## Download (current test build: `v0.2.0-test6`)

**[`v0.2.0-test6`](https://github.com/b-air/CYD32/releases/tag/v0.2.0-test6)** — pre-release, 24 Nov 2026,
commit `8bb5917118a2`. **Unsigned (USB install only), not tested on hardware
yet.** The repository is **private**: sign in to GitHub with an account that
has access, otherwise every link below shows 404. There is no "Latest"
release (all builds are pre-releases), so use these links directly.

| Board | Firmware (direct link) | Size | SHA-256 |
|---|---|---|---|
| CYD | [`firmware-cyd.bin`](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/firmware-cyd.bin) | 1,633,872 bytes | `9c528f006a57f094d5de708a91558fea0fe16e89b14dbd16b80c64d7c0cb3734` |
| StopWatch | [`firmware-m5stack-stopwatch.bin`](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/firmware-m5stack-stopwatch.bin) | 1,740,544 bytes | `be186d7d7c4b9694d8b941ee7b02dd2e8e52dae651d81c5db26913a951f7cd27` |

- **Already on `v0.2.0-test5`?** Flash only the firmware file above at
  `0x10000`, without Erase all (the other three files did not change):
  [docs/INSTALL.md](docs/INSTALL.md#already-on-v020-test5-update-with-one-file).
- **First install:** four files per board (bootloader, partitions, boot_app0,
  firmware), step by step in [docs/INSTALL.md](docs/INSTALL.md).
- All checksums: [`SHA256SUMS`](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/SHA256SUMS).
  Version history: [CHANGELOG.md](CHANGELOG.md).

## Screens (`v0.2.0-test6`)

Host renders of the real firmware UI code (LVGL 9.2.2 on a PC, example data,
not photos of the hardware). All of them are in
[`docs/screenshots/v9/firmware-host-b6/`](docs/screenshots/v9/firmware-host-b6/).

| CYD | StopWatch |
|---|---|
| ![CYD sonar, all devices](docs/screenshots/v9/firmware-host-b6/cyd-01-sonar-all.png) | ![StopWatch sonar, all devices](docs/screenshots/v9/firmware-host-b6/sw-01-sonar-all.png) |
| SONAR, mode ALL | SONAR, mode ALL |
| ![CYD sonar, TRK mode, beep on](docs/screenshots/v9/firmware-host-b6/cyd-02-sonar-filtered-trk.png) | ![StopWatch sonar, TRK mode, beep on](docs/screenshots/v9/firmware-host-b6/sw-02-sonar-filtered-trk.png) |
| SONAR, mode TRK, proximity beep on | SONAR, mode TRK, proximity beep on |
| ![CYD dashboard, WIFI view](docs/screenshots/v9/firmware-host-b6/cyd-04-dash-wifi.png) | ![StopWatch dashboard counters](docs/screenshots/v9/firmware-host-b6/sw-03-dash-counters.png) |
| DASH, WIFI view | DASH, COUNTERS page |
| ![CYD menu](docs/screenshots/v9/firmware-host-b6/cyd-07-menu-beep-on.png) | ![StopWatch menu](docs/screenshots/v9/firmware-host-b6/sw-07-menu-beep-on.png) |
| MENU (6 entries, BEEP ON) | MENU (6 entries, BEEP ON) |
| ![CYD creature, size 4](docs/screenshots/v9/firmware-host-b6/cyd-13-creature-stage4.png) | ![StopWatch creature, FEED](docs/screenshots/v9/firmware-host-b6/sw-14b-creature-feed-chomp.png) |
| CREA, size 4 | CREA, FEED (+2 XP) |

From `v0.2.0-test3` the boot, sonar and (StopWatch) wardriving screens use the
v9 green phosphor radar theme: a procedural PPI scope with a 3 s clockwise
sweep, a 40 degree trailing wedge and fading echoes; yellow is reserved for
drones (#FCEE0A on both boards). The StopWatch boot screen shows the M5Stack
logo; every other StopWatch screen is phosphor green on black, and the screens
with the 4 tabs (SONAR / DASH / CREA / MENU) use the curved tab band. On the CYD the remaining
screens use the same phosphor tokens (README section 7 of the design). The
first screen after boot is the SONAR; the creature (CREA tab) starts as an egg
that hatches at the first device, then grows with detections. Design
reference renders and older host renders are archived in
[`docs/screenshots/v9/`](docs/screenshots/v9/) (EXAMPLE DATA).

## What it does (and does not do)

- Wi-Fi: promiscuous mode filtered to **management frames only** (beacons, probe
  requests/responses). DATA frames are dropped by the driver filter. Frame
  contents are not kept.
- BLE: **passive** scan (no scan requests). The NimBLE central/peripheral/broadcaster
  roles are compiled out, so no GATT connection is possible.
- Never transmits on its own: no `esp_wifi_80211_tx`, no deauth, no spam, no jamming.
  The only radio emission is the temporary update hotspot you start yourself.
- MAC addresses are replaced by `SHA-256(random per-boot salt || MAC)`. The salt
  lives in RAM only, so IDs cannot be linked across reboots.
- **Nothing is stored by default**: no flash or SD logging. On CYD, wardriving
  export (phase 3) will only write to the local microSD card on request, never
  upload. The StopWatch has no microSD slot: wardriving there logs to a fixed
  RAM table of salted IDs (cleared at reboot, oldest dropped when full), never
  to flash, NVS or a network.
- Remote ID (ASTM F3411) beacons over Wi-Fi and BLE are counted as drones.
- The sonar radius is the smoothed RSSI. The angle is **indicative**: an
  unsalted FNV-1a hash of the address, identical on every CYD32, not a
  direction (see [docs/SONAR.md](docs/SONAR.md)).
- TRK / META / FLOCK badges come from a small documented signature set
  ([docs/SIGNATURES.md](docs/SIGNATURES.md)). Navigation, menu and dashboard:
  [docs/NAVIGATION.md](docs/NAVIGATION.md).

## Status (phase 2 MVP)

| Feature | State |
|---|---|
| Boot, display, touch, tab bar | done |
| Sonar (7 modes: ALL / WIFI / BLE / RID / FLOCK / TRK / META), device details, proximity beep | done (modes and beep hardware untested) |
| Passive Wi-Fi mgmt + BLE scan, hashed IDs | done |
| Creature (4 lineages; hatches at 1 device, sizes 2/3/4 at 30/60/120 devices in the last 3 min, shrinks again; Cthulhu at 1/3/6/10/15 drones) | done |
| About screen (licence, version, commit, source) | done |
| Signed OTA from an iPhone, A/B slots, rollback | done |
| Trackers / Meta / Flock badges, dashboard (counters; WIFI / BLE / RID / TAGGED / LOG views) | done (hardware untested) |
| Remote ID field decoding (position, operator) | phase 3 |
| Wardriving on StopWatch (RAM-only, salted IDs, cap 1024) | done (no GPS / no export; hardware untested) |
| M5Stack StopWatch target (round HUD, dual firmware) | done (v8 round HUD; hardware untested) |

## Hardware notes

CYD, pin map (single-USB ESP32-2432S028): TFT on HSPI (SCK 14, MOSI 13, MISO 12,
CS 15, DC 2, BL 21), touch on its own SPI (SCK 25, MOSI 32, MISO 39, CS 33,
IRQ 36). Touch calibration is in `src/hw.cpp`. The optional proximity beep uses
the SPEAK connector (GPIO 26): plug a small speaker or buzzer there; the board
has none of its own.

StopWatch (verified against the official M5Stack documentation): ESP32-S3R8,
16 MB flash, 8 MB OPI PSRAM (`qio_opi`). CO5300 QSPI on G39 CS, G40 SCK,
G38 TE, G41/G42/G46/G45 data; display reset on M5IOE1 PYG5. CST820B touch on
G47 SDA, G48 SCL, G13 INT; touch reset on M5IOE1 PYG4. KEYA (yellow) is GPIO2 and is NEXT; KEYB (blue) is GPIO1 and is SELECT (held: see docs/NAVIGATION.md).
The power button is not mapped (the PMIC handles on/off). There is no microSD.
M5Unified and M5GFX drive the panel, touch and buttons. The native 466x466
round HUD is in `src/ui_round.cpp` (metrics in `include/ui_layout_round.h`).
Approved layout mockups (example data, not device captures) are in
`docs/design/stopwatch-v8/`. The CYD 320x240 layout stays in
`include/ui_layout_cyd.h` and `src/ui.cpp`.

## Build

```bash
pip install --require-hashes -r requirements.txt
pio run -e cyd                      # .pio/build/cyd/firmware.bin
pio run -e m5stack-stopwatch        # .pio/build/m5stack-stopwatch/firmware.bin
```

Versions are pinned in `platformio.ini`. Build-time options (environment
variables): `CYD32_OTA_PUBKEY`, `CYD32_OTA_PUBKEY2` (paths to P-256 public keys,
default `keys/ota_pub_0.pem` and `keys/ota_pub_1.pem`), `CYD32_SOURCE_URL`,
`CYD32_OFFICIAL=1` (upstream releases only).

## First flash (USB, from a computer)

Step-by-step guide for both boards (web flasher, every field, serial console,
troubleshooting): **[docs/INSTALL.md](docs/INSTALL.md)**.

The first install must be done over USB from a computer:

```bash
pio run -e cyd -t upload                       # CYD
pio run -e m5stack-stopwatch -t upload         # StopWatch (bootloader at 0x0)
```

or with esptool and the four release files for the board (bootloader,
partitions, boot_app0, firmware; see `docs/RELEASING.md`). There is no merged
image. The bootloader goes at 0x1000 on the CYD (ESP32) and at 0x0 on the
StopWatch (ESP32-S3); on both, partitions go at 0x8000, boot_app0 at 0xe000 and
the app at 0x10000. Flash settings: CYD `dio` / `40m` / `4MB`, StopWatch
`dio` / `80m` / `16MB` (as in the image headers). Browser
flashers need Web Serial (Chrome or Edge on a computer); iOS has no Web Serial.
USB flashing always remains possible: CYD32 burns **no eFuse**, and uses no
Secure Boot and no flash encryption.

## Updating from an iPhone (OTA)

1. MENU > OTA > *Start update mode*. Scanning stops; a WPA2 hotspot with a
   random password starts for 10 minutes and its `WIFI:` QR code is shown.
2. Scan the QR code with the iPhone camera to join the hotspot.
3. Open `http://192.168.4.1` in Safari and upload the **signed** `.bin` from Files.
4. The device streams the file to the inactive slot, verifies the ECDSA P-256
   signature appended to it, and only then switches the boot slot. On any
   failure the update is aborted and the slot is erased.
5. After reboot the new firmware must pass a self-test, otherwise the
   bootloader rolls back to the previous slot.

The portal shows licence, version, commit and source URL; it never exposes scan data.
It only answers requests whose `Host` is `192.168.4.1`, and uploads must carry a
random per-session token embedded in the page (CSRF protection). Uploads larger
than the OTA slot are rejected. A file built for the other board (the image
embeds `CYD32-TARGET:<env>`) is rejected before the boot slot is switched,
with a message on the device and on the portal. If the uploaded firmware is
older than the running one (or carries no version marker), it is not activated
until you confirm the DOWNGRADE on the touchscreen.

### Firmware status: Official / Fork / Unsigned

- **Official**: upstream release build whose key fingerprint matches the
  upstream key in `keys/ota_pub_0.pem`.
- **Fork**: built by anyone else with their own key (`scripts/gen-key.sh`).
  Forks set `CYD32_OTA_PUBKEY` to their own public key.
- **Unsigned**: installed through the one-shot unsigned mode. To arm it, hold
  the board button for 3 s **during the boot splash** (BOOT / GPIO0 on CYD,
  not before reset, which enters the USB bootloader; KEYA / GPIO2 on
  StopWatch), then confirm on the touchscreen. It accepts one unsigned update
  during that boot only and shows a persistent red banner.

The About screen shows the status and the fingerprint of the verifying key
(first 16 hex digits of SHA-256 of the raw public key). Two key slots allow key rotation.

## Known TODOs

- Hardware test of `v0.2.0-test6` on both boards (see the checklist in the
  release notes): StopWatch Wi-Fi start-up fix, sonar modes, dashboard views,
  proximity beep (CYD: small speaker on SPEAK), creature PET / FEED, heap use.
- Still to confirm on hardware: OTA end to end with rollback (needs a signed
  release), StopWatch battery reading, RAM-only wardriving, RF silence outside
  update mode (monitor-mode capture).
- Migrate `src/ota_sig.cpp` and `src/privacy.cpp` to the mbedTLS 3 API (the
  `*_ret` SHA-256 functions and direct `grp`/`Q` access go away) before moving to
  arduino-esp32 3.x / ESP-IDF 5.

## Signing and releases

Signing happens **locally** on the maintainer's machine; no private key is
ever stored in GitHub. See [docs/RELEASING.md](docs/RELEASING.md).

## Installation Information (AGPL-3.0 section 6)

The application signature only controls OTA updates; anyone can always
reflash a modified firmware over USB. If Secure Boot is ever enabled on a
device given to someone else, the means to install modified firmware on it
(keys or procedure) must be provided with it.

## Contributing

Install the pre-commit hook: `ln -s ../../hooks/pre-commit .git/hooks/pre-commit`.
It checks staged changes against a private deny-list in
`~/.config/cyd32/denylist.txt` (one fixed string per line) if present.

## Licence

Copyright (C) 2026 CYD32 contributors. AGPL-3.0-only, see `LICENSE` and `NOTICE`.
