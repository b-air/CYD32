/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void esp_fill_random(void *buf, size_t len);
uint32_t esp_random(void);
#ifdef __cplusplus
}
#endif
