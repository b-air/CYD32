// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "big_alloc.h"
#include <stdlib.h>
extern "C" void *cyd32_big_alloc(size_t size) { return calloc(1, size); }
extern "C" void *cyd32_lv_pool_alloc(size_t size) { return calloc(1, size); }
