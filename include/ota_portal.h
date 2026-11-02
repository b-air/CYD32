// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// "Update" mode: temporary WPA2 access point + HTTP upload portal (192.168.4.1).
// The portal serves only firmware metadata (licence, version, commit, source URL,
// signature status). It never exposes scan data: the scanner is stopped and the
// detection table is not reachable from here.
#pragma once
#include <stdint.h>
namespace ota_portal {
enum Phase : uint8_t { IDLE, WAITING, RECEIVING, VERIFYING, SUCCESS, FAILED, TIMEOUT };
struct State {
  Phase phase;
  char ssid[16];
  char pass[16];
  char qr[64];            // "WIFI:T:WPA;S:...;P:...;;"
  uint32_t received;      // bytes
  uint32_t expected;      // Content-Length if known, else 0
  uint32_t remainingS;    // seconds before the AP shuts down
  bool unsignedArmed;     // one-shot unsigned mode
  char msg[64];
};
constexpr uint32_t kWindowMs = 10UL * 60UL * 1000UL;
void start(bool unsignedArmed);  // scanner must be stopped first
void loop();
const State &state();
}
