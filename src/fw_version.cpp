// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "fw_version.h"
#include <stdlib.h>
#include "cyd32_build_gen.h"
namespace fw_version {
void parse(const char *s, uint16_t &ma, uint16_t &mi, uint16_t &pa) {
  ma = mi = pa = 0;
  if (*s == 'v' || *s == 'V') s++;
  char *e;
  ma = (uint16_t)strtoul(s, &e, 10); if (*e != '.') return;
  mi = (uint16_t)strtoul(e + 1, &e, 10); if (*e != '.') return;
  pa = (uint16_t)strtoul(e + 1, &e, 10);
}
int compare(const Marker &a, const Marker &b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  return 0;
}
// Numeric parts are filled in by the build script (CYD32_VER_MAJOR/MINOR/PATCH).
__attribute__((used)) const Marker running = {
    {'C', 'Y', 'D', '3', '2', '-', 'V', 'E', 'R', 'S', 'I', 'O', 'N', '-', '0', '1'},
    CYD32_VER_MAJOR, CYD32_VER_MINOR, CYD32_VER_PATCH, 0};
// Searchable marker; the OTA path rejects an image whose env does not match.
__attribute__((used)) const char running_target[] = "CYD32-TARGET:" CYD32_TARGET;
}
