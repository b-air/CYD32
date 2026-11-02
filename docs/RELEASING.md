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

The `release` workflow builds the **unsigned** `firmware.bin`, plus
`bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `SHA256SUMS`, an SPDX SBOM,
the full source archive (`cyd32-<tag>-source.tar.gz`, AGPL section 6) and a
build provenance attestation, and publishes a GitHub release. The job fails if
the source archive is missing.

## 3. Sign locally and upload

```bash
gh release download v0.2.0 -p firmware.bin -p SHA256SUMS -D dist/v0.2.0
cd dist/v0.2.0 && sha256sum -c SHA256SUMS --ignore-missing && cd -
gh attestation verify dist/v0.2.0/firmware.bin -R <owner>/CYD32
scripts/sign.sh dist/v0.2.0/firmware.bin dist/v0.2.0/firmware-signed.bin 0
gh release upload v0.2.0 dist/v0.2.0/firmware-signed.bin
```

`firmware-signed.bin` = unsigned image + 96-byte trailer (format in
`include/ota_sig.h`). Use it for OTA; the unsigned `firmware.bin` is for USB.

## Flash a release over USB

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash \
  0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```
