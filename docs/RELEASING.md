<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# Releasing CYD32

Requirements on the signing machine: `git`, `openssl`, `bash`, PlatformIO only
if you build locally.

## 1. One-time: create the signing key (locally)

```bash
scripts/gen-key.sh 0          # private: ~/.config/cyd32/keys/ota_priv_0.pem (600)
git add keys/ota_pub_0.pem && git commit -m "Add OTA public key (slot 0)"
```

Back up the private key offline. Never commit or upload it. Slot 1
(`scripts/gen-key.sh 1`) is for rotation: ship a firmware containing both public
keys, then start signing with the new one.

## 2. Tag the release

```bash
git tag -a v0.2.0 -m "v0.2.0" && git push origin v0.2.0
```

The `release` workflow builds the **unsigned** images
`firmware-cyd.bin` and `firmware-m5stack-stopwatch.bin`, plus each target's
`bootloader-*.bin`, `partitions-*.bin` and `boot_app0-*.bin`,
`SHA256SUMS`, an SPDX SBOM, the full source archive
(`cyd32-<tag>-source.tar.gz`, AGPL section 6) and a build provenance
attestation, and publishes a GitHub release. The job fails if the source
archive is missing.

## 3. Sign locally and upload

```bash
gh release download v0.2.0 -p firmware-cyd.bin -p firmware-m5stack-stopwatch.bin -p SHA256SUMS -D dist/v0.2.0
cd dist/v0.2.0 && sha256sum -c SHA256SUMS --ignore-missing && cd -
gh attestation verify dist/v0.2.0/firmware-cyd.bin -R <owner>/CYD32
gh attestation verify dist/v0.2.0/firmware-m5stack-stopwatch.bin -R <owner>/CYD32
scripts/sign.sh dist/v0.2.0/firmware-cyd.bin dist/v0.2.0/firmware-cyd-signed.bin 0
scripts/sign.sh dist/v0.2.0/firmware-m5stack-stopwatch.bin dist/v0.2.0/firmware-m5stack-stopwatch-signed.bin 0
gh release upload v0.2.0 dist/v0.2.0/firmware-cyd-signed.bin dist/v0.2.0/firmware-m5stack-stopwatch-signed.bin
```

The `SHA256SUMS` published by the workflow is generated before signing, so it
does not list the `-signed.bin` files. After uploading them, regenerate it from
every release asset and replace it:

```bash
gh release download v0.2.0 -D dist/v0.2.0-all
cd dist/v0.2.0-all && rm SHA256SUMS && LC_ALL=C sha256sum * > SHA256SUMS && sha256sum -c --strict SHA256SUMS && cd -
gh release upload v0.2.0 dist/v0.2.0-all/SHA256SUMS --clobber
```

A signed image is the unsigned `.bin` plus the 96-byte trailer (format in
`include/ota_sig.h`). Use the signed file for OTA; the unsigned file is for
USB. Sign both targets with the same slot-0 key. Each image embeds
`CYD32-TARGET:<env>`; the device rejects a file built for the other board
before `set_boot_partition`.

## Flash a release over USB

There is no merged image: each release ships four separate files per board
(`bootloader-<env>.bin`, `partitions-<env>.bin`, `boot_app0-<env>.bin` and
`firmware-<env>.bin`). Flash all four at these offsets:

| File            | CYD (ESP32) | StopWatch (ESP32-S3) |
|-----------------|-------------|----------------------|
| bootloader      | 0x1000      | 0x0                  |
| partitions      | 0x8000      | 0x8000               |
| boot_app0       | 0xe000      | 0xe000               |
| firmware (app)  | 0x10000     | 0x10000              |

The image headers carry these flash settings (checked with
`esptool image-info` on the v0.2.0-test1 bootloader and firmware files):

| Setting         | CYD (ESP32) | StopWatch (ESP32-S3) |
|-----------------|-------------|----------------------|
| Flash mode      | dio         | dio                  |
| Flash frequency | 40m         | 80m                  |
| Flash size      | 4MB         | 16MB                 |

esptool keeps the header's values by default; pass them explicitly anyway
(below), and choose the same values in the web flasher. For the web flasher
steps, see [INSTALL.md](INSTALL.md). The StopWatch env's `qio_opi` memory type does not change the flash
mode written to the image, and esptool's `--flash_mode` flag does not take
`qio_opi`.

CYD (ESP32, bootloader at 0x1000):

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 bootloader-cyd.bin 0x8000 partitions-cyd.bin \
  0xe000 boot_app0-cyd.bin 0x10000 firmware-cyd.bin
```

StopWatch (ESP32-S3, bootloader at 0x0, not 0x1000):

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader-m5stack-stopwatch.bin 0x8000 partitions-m5stack-stopwatch.bin \
  0xe000 boot_app0-m5stack-stopwatch.bin 0x10000 firmware-m5stack-stopwatch.bin
```

Hold the power button until the green LED blinks to enter download mode
(see the official StopWatch documentation).
