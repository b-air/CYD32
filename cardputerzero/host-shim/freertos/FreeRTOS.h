/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* pthread stand-in for the detection-table lock in src/detections.cpp. */
#pragma once
#include <pthread.h>
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(m) pthread_mutex_lock(m)
#define portEXIT_CRITICAL(m) pthread_mutex_unlock(m)
