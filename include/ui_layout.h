// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Display chrome metrics. The CYD 320x240 module is the production layout
// (ui_layout_cyd.h). The StopWatch 466x466 round HUD lives in ui_layout_round.h.
#pragma once
#if defined(CYD32_HW_M5STACK_STOPWATCH)
#include "ui_layout_round.h"
#else
#include "ui_layout_cyd.h"
#endif
