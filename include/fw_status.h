// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Firmware provenance shown on the About screen and the OTA portal.
#pragma once
#include <stdint.h>
namespace fw_status {
enum Kind : uint8_t { OFFICIAL, FORK, UNSIGNED };
void begin();                       // reads NVS record for the running slot
Kind kind();
const char *kindStr();              // "Official" / "Fork" / "Unsigned"
const char *keyFingerprint();       // fingerprint of the key that verified this image
// Called by the OTA code right before switching the boot partition.
void recordNextBoot(const char *partLabel, bool unsignedImage, int keySlot);
}
