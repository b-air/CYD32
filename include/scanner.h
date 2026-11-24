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
#include <stdint.h>
namespace scanner {
void begin();
void loop();   // call from the main loop
void stop();   // before OTA mode
bool wifiActive();
// Self-test: Wi-Fi promiscuous start and NimBLE init succeeded, and the BLE
// scan has been started at least once.
bool healthy();
// Wi-Fi was started but promiscuous mode is not running: the header shows "W!".
bool wifiFailed();
// Wi-Fi bring-up status for ABOUT (RAM only). While not ready the bring-up is
// retried every 30 s. Heap numbers are sampled at the last attempt.
struct WifiDiag {
  bool ready;
  int err;              // real esp_err_t of the failing step (0 = ESP_OK)
  const char *errName;  // esp_err_to_name()
  const char *step;     // "init", "start", "promisc" or "" (no failure)
  uint16_t attempts;
  uint32_t retryInMs;   // 0 when ready
  uint32_t freeInt, largestInt, largestDma, freePsram;
};
WifiDiag wifiDiag();
}
