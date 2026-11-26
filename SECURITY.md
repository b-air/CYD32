<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->

# Security Policy

CYD32 is educational, receive-only (passive) radio sonar firmware for the
ESP32-2432S028 ("Cheap Yellow Display") and the M5Stack StopWatch.

## Supported versions

Only the latest published test prerelease (`v0.2.0-testN`) is tracked.
The current tag is `v0.2.0-test6`. Earlier test prereleases and untagged
or locally built images are not tracked. There is no stable release yet;
every published build is a prerelease.

## Reporting a vulnerability

Use GitHub private vulnerability reporting: open the Security tab on this
repository, then **Report a vulnerability**, or go to
https://github.com/b-air/CYD32/security/advisories/new

Do not open a public issue for a security report. Reports are handled on
a best-effort basis. There is no response SLA and no bug bounty.

## Test prereleases, signatures and OTA

Current test prereleases are **unsigned** and are intended for **USB
flashing only**. They ship `firmware-*.bin` files and no `*-signed.bin`
images. Each release includes a `SHA256SUMS` file so the downloads can
be checked for integrity.

OTA updates require a signed image: the firmware `.bin` plus a 96-byte
trailer (ECDSA P-256 over SHA-256 of the image). The firmware verifies
that trailer against the public keys built into the running image and
rejects the upload if verification fails (`include/ota_sig.h`,
`src/ota_sig.cpp`). End-to-end OTA with rollback is implemented in the
firmware but has not yet been confirmed on hardware.

A one-shot unsigned OTA is accepted only when armed on that boot (hold
the board button for 3 seconds during the splash screen, then confirm
on the touchscreen). It applies to a single update and shows a
persistent banner. USB flashing remains possible at all times: CYD32
does not enable Secure Boot or flash encryption, and it burns no eFuse.

## Scope

In scope: vulnerabilities in this repository's firmware that affect a
device running the latest tracked test prerelease, including a bypass
of OTA signature verification and any transmit behaviour outside the
user-started update hotspot.

Out of scope: third-party hardware and unofficial board variants;
user-modified firmware, unofficial signing keys, or images not built
from this repository; issues that require physical access, including
USB reflashing; and social engineering.

## Receive-only behaviour

The firmware is designed to receive only. It listens to Wi-Fi management
frames and BLE advertisements and does not send deauth frames, probe
requests, or other unsolicited radio traffic. The documented exception
is update mode, which starts a short-lived WPA2 hotspot so an image can
be uploaded. RF silence outside update mode is still to be confirmed on
hardware.
