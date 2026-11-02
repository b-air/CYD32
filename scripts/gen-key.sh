#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
# Generate an ECDSA P-256 firmware signing key pair, locally.
#   scripts/gen-key.sh [slot]        slot = 0 (default) or 1 (rotation)
# Private key: ~/.config/cyd32/keys/ota_priv_<slot>.pem (mode 600, NEVER commit or upload it)
# Public key:  keys/ota_pub_<slot>.pem in this repo (commit it; it is built into the firmware)
set -euo pipefail
slot="${1:-0}"
[[ "$slot" == 0 || "$slot" == 1 ]] || { echo "slot must be 0 or 1" >&2; exit 2; }
kdir="${CYD32_KEY_DIR:-$HOME/.config/cyd32/keys}"
priv="$kdir/ota_priv_${slot}.pem"
repo="$(cd "$(dirname "$0")/.." && pwd)"
pub="$repo/keys/ota_pub_${slot}.pem"
if [[ -e "$priv" ]]; then echo "Refusing to overwrite existing $priv" >&2; exit 1; fi
umask 077
mkdir -p "$kdir"
openssl ecparam -name prime256v1 -genkey -noout -out "$priv"
chmod 600 "$priv"
openssl ec -in "$priv" -pubout -out "$pub" 2>/dev/null
fpr=$(openssl ec -pubin -in "$pub" -outform DER 2>/dev/null | tail -c 65 | openssl dgst -sha256 -r | cut -c1-16)
echo "Private key: $priv  (keep it offline/backed up; never commit it)"
echo "Public key:  $pub   (commit this file)"
echo "Fingerprint: $fpr   (shown on the device About screen)"
