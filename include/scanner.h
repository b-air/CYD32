// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Passive receive-only scanners. This module never transmits:
//  - Wi-Fi: promiscuous mode filtered to MANAGEMENT frames only (DATA/CTRL are
//    discarded by the driver filter and re-checked in the callback). Only
//    beacons, probe responses and probe requests are counted.
//  - BLE: passive scan (no scan requests), observer role only. The NimBLE
//    central/peripheral/broadcaster roles are compiled out, so no GATT
//    connection is possible.
// Wi-Fi and BLE share the radio; they are time-sliced.
#pragma once
namespace scanner {
void begin();
void loop();   // call from the main loop
void stop();   // before OTA mode
bool wifiActive();
// Self-test: Wi-Fi promiscuous start and NimBLE init succeeded, and the BLE
// scan has been started at least once.
bool healthy();
}
