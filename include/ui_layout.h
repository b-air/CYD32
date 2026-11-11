// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Display chrome metrics. The CYD 320x240 module is the production layout.
// The StopWatch round module is provisional and can be swapped when a final
// mockup is ready (replace ui_layout_round.h or the include below).
#pragma once
#if defined(CYD32_HW_M5STACK_STOPWATCH)
#include "ui_layout_round.h"
#else
#include "ui_layout_cyd.h"
#endif
