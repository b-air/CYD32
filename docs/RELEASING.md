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
`bootloader-*.bin` and `partitions-*.bin`, shared `boot_app0.bin`,
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

A signed image is the unsigned `.bin` plus the 96-byte trailer (format in
`include/ota_sig.h`). Use the signed file for OTA; the unsigned file is for
USB. Sign both targets with the same slot-0 key. Each image embeds
`CYD32-TARGET:<env>`; the device rejects a file built for the other board
before `set_boot_partition`.

## Flash a release over USB

CYD offsets are unchanged (ESP32, bootloader at 0x1000):

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash \
  0x1000 bootloader-cyd.bin 0x8000 partitions-cyd.bin \
  0xe000 boot_app0.bin 0x10000 firmware-cyd.bin
```

StopWatch (ESP32-S3): the bootloader lives at 0x0, not 0x1000. First flash a
merged image at 0x0:

```bash
esptool.py --chip esp32s3 merge_bin -o firmware-m5stack-stopwatch-merged.bin \
  --flash_mode dio --flash_size 16MB \
  0x0 bootloader-m5stack-stopwatch.bin 0x8000 partitions-m5stack-stopwatch.bin \
  0xe000 boot_app0.bin 0x10000 firmware-m5stack-stopwatch.bin
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash \
  0x0 firmware-m5stack-stopwatch-merged.bin
```

Or flash the pieces separately with the bootloader at 0x0:

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash \
  0x0 bootloader-m5stack-stopwatch.bin 0x8000 partitions-m5stack-stopwatch.bin \
  0xe000 boot_app0.bin 0x10000 firmware-m5stack-stopwatch.bin
```

Hold the power button until the green LED blinks to enter download mode
(see the official StopWatch documentation).
