#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
# Sign a firmware image locally by appending the 96-byte CYD32 trailer
# (format: include/ota_sig.h). The device verifies it before switching slots.
#   scripts/sign.sh firmware.bin [signed.bin] [slot]
# Use the same key slot for every target (firmware-cyd.bin and
# firmware-m5stack-stopwatch.bin). The signature covers the whole file,
# including the embedded CYD32-TARGET:<env> marker.
# Default key: ~/.config/cyd32/keys/ota_priv_<slot>.pem (override with CYD32_SIGN_KEY).
#
# First USB flash (see docs/RELEASING.md):
#   CYD (ESP32): bootloader at 0x1000, partitions 0x8000, boot_app0 0xe000,
#                app 0x10000.
#   StopWatch (ESP32-S3): bootloader at 0x0 (not 0x1000), partitions 0x8000,
#                boot_app0 0xe000, app 0x10000. There is no merged image.
set -euo pipefail
in="${1:?usage: sign.sh firmware.bin [out.bin] [slot]}"
out="${2:-${in%.bin}-signed.bin}"
slot="${3:-0}"
key="${CYD32_SIGN_KEY:-$HOME/.config/cyd32/keys/ota_priv_${slot}.pem}"
[[ -f "$key" ]] || { echo "No private key at $key (run scripts/gen-key.sh)" >&2; exit 1; }
if tail -c 8 "$in" | grep -q "CYD32END"; then echo "$in is already signed" >&2; exit 1; fi
[[ "$(head -c1 "$in" | od -An -tx1 | tr -d ' ')" == "e9" ]] || { echo "$in is not an ESP32 app image" >&2; exit 1; }
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
openssl dgst -sha256 -sign "$key" -out "$tmp/sig.der" "$in"
len=$(wc -c < "$tmp/sig.der" | tr -d ' ')
(( len >= 8 && len <= 72 )) || { echo "unexpected signature length $len" >&2; exit 1; }
{
  printf 'CYD32SG1'
  printf "\\$(printf '%03o' "$slot")\\$(printf '%03o' "$len")"
  head -c 6 /dev/zero
  cat "$tmp/sig.der"
  head -c $((72 - len)) /dev/zero
  printf 'CYD32END'
} > "$tmp/trailer"
[[ $(wc -c < "$tmp/trailer" | tr -d ' ') == 96 ]] || { echo "trailer size error" >&2; exit 1; }
cat "$in" "$tmp/trailer" > "$out"
# Self-check with the matching public key
openssl ec -in "$key" -pubout -out "$tmp/pub.pem" 2>/dev/null
openssl dgst -sha256 -verify "$tmp/pub.pem" -signature "$tmp/sig.der" "$in" >/dev/null
echo "Signed: $out"
shasum -a 256 "$out" 2>/dev/null || sha256sum "$out"
