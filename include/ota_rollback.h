// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Roll back to the previous firmware when the running image is still
// unverified (first boot after an OTA update) and cannot start. No-op for an
// image flashed over USB or already marked valid.
#pragma once
#include <Arduino.h>
#include <esp_ota_ops.h>
#include "boot_log.h"

namespace ota_rollback {
// hold_ms keeps an error message on screen before the reboot (0 = no wait).
inline void ifPendingVerify(const char *why, uint32_t hold_ms) {
  esp_ota_img_states_t s;
  const esp_partition_t *run = esp_ota_get_running_partition();
  if (run == nullptr || esp_ota_get_state_partition(run, &s) != ESP_OK || s != ESP_OTA_IMG_PENDING_VERIFY) return;
  BOOT_LOG("%s on an unverified OTA image: rolling back to the previous firmware", why);
  if (hold_ms) delay(hold_ms);
  esp_ota_mark_app_invalid_rollback_and_reboot();  // does not return on success
}
}  // namespace ota_rollback
