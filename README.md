<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# CYD32

Educational, **passive (receive-only)** radio "sonar" firmware for the
ESP32-2432S028 "Cheap Yellow Display" (ILI9341 320x240, XPT2046 touch, 4 MB flash).
It listens to Wi-Fi management frames and Bluetooth Low Energy advertisements,
draws them on a naval-sonar UI and feeds a small creature that evolves as it
"meets" drones broadcasting Remote ID. The creature has idle and reaction
animations (it reacts to a new drone or a PET/FEED tap) and plays an evolution
sequence when it reaches a new stage.

![screens](docs/screenshots/board.png)

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
- **Nothing is stored by default**: no flash or SD logging. Wardriving (phase 3)
  will only export to the local microSD card on request, never upload.
- Remote ID (ASTM F3411) beacons over Wi-Fi and BLE are counted as drones.

## Status (phase 2 MVP)

| Feature | State |
|---|---|
| Boot, display, touch, tab bar | done |
| Sonar, grid (+ easter egg A1-B2-C3-D4), details | done |
| Passive Wi-Fi mgmt + BLE scan, hashed IDs | done |
| Creature (5 lineages, stages at 0/1/5/12/20 drones) | done |
| About screen (licence, version, commit, source) | done |
| Signed OTA from an iPhone, A/B slots, rollback | done |
| Trackers, drones (Remote ID decoding), wardriving | phase 3 (stub screens) |

## Hardware notes

Pin map (single-USB ESP32-2432S028): TFT on HSPI (SCK 14, MOSI 13, MISO 12,
CS 15, DC 2, BL 21), touch on its own SPI (SCK 25, MOSI 32, MISO 39, CS 33,
IRQ 36). Touch calibration is in `src/hw.cpp`.

## Build

```bash
pip install platformio==6.1.16
pio run -e cyd                      # firmware: .pio/build/cyd/firmware.bin
```

Versions are pinned in `platformio.ini`. Build-time options (environment
variables): `CYD32_OTA_PUBKEY`, `CYD32_OTA_PUBKEY2` (paths to P-256 public keys,
default `keys/ota_pub_0.pem` and `keys/ota_pub_1.pem`), `CYD32_SOURCE_URL`,
`CYD32_OFFICIAL=1` (upstream releases only).

## First flash (USB, from a computer)

The first install must be done over USB from a computer:

```bash
pio run -e cyd -t upload            # builds and flashes bootloader, partitions and app
```

or with esptool and a release `.bin` (see `docs/RELEASING.md`). Browser
flashers need Web Serial (Chrome or Edge on a computer); iOS has no Web Serial.
USB flashing always remains possible: CYD32 burns **no eFuse**, and uses no
Secure Boot and no flash encryption.

## Updating from an iPhone (OTA)

1. MENU > Update > *Start update mode*. Scanning stops; a WPA2 hotspot with a
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
than the OTA slot are rejected. If the uploaded firmware is older than the running
one (or carries no version marker), it is not activated until you confirm the
DOWNGRADE on the touchscreen.

### Firmware status: Official / Fork / Unsigned

- **Official**: upstream release build whose key fingerprint matches the
  upstream key in `keys/ota_pub_0.pem`.
- **Fork**: built by anyone else with their own key (`scripts/gen-key.sh`).
  Forks set `CYD32_OTA_PUBKEY` to their own public key.
- **Unsigned**: installed through the one-shot unsigned mode. To arm it, hold
  the BOOT button for 3 s **during the boot splash** (not before reset, which
  enters the USB bootloader), then confirm on the touchscreen. It accepts one
  unsigned update during that boot only and shows a persistent red banner.

The About screen shows the status and the fingerprint of the verifying key
(first 16 hex digits of SHA-256 of the raw public key). Two key slots allow key rotation.

## Known TODOs

- Test on real hardware: Wi-Fi `WIFI_MODE_NULL` + promiscuous capture, BLE/Wi-Fi
  time-slicing, touch calibration, OTA end to end with rollback.
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
