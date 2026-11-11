// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Version marker embedded in every image. The OTA code searches the received
// image for it to detect downgrades (the Arduino core's esp_app_desc_t.version
// is fixed by the prebuilt SDK, so it cannot carry our version).
#pragma once
#include <stdint.h>
namespace fw_version {
struct Marker {
  char magic[16];       // "CYD32-VERSION-01"
  uint16_t major, minor, patch, reserved;
};
constexpr char kMagic[17] = "CYD32-VERSION-01";
extern const Marker running;
// Target marker stored as the C string "CYD32-TARGET:<env>" (PIO environment
// name). It lives in the app image, so the signed trailer covers it.
constexpr char kTargetPrefix[] = "CYD32-TARGET:";
extern const char running_target[];
// Parses "v1.2.3..." / "1.2.3..."; missing parts are 0.
void parse(const char *s, uint16_t &ma, uint16_t &mi, uint16_t &pa);
// <0 if a<b, 0 if equal, >0 if a>b
int compare(const Marker &a, const Marker &b);
}
