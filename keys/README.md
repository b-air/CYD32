<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# OTA public keys

`ota_pub_0.pem` and `ota_pub_1.pem` (ECDSA P-256, PEM) are built into the firmware
as the two signature-verification slots. Create them with `scripts/gen-key.sh 0|1`.
Only PUBLIC keys belong here; the build refuses to read a private key.
Without any key, the firmware refuses every signed update (only the one-shot
unsigned mode works).
