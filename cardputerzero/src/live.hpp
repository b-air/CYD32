/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
#pragma once
#include <string>
bool live_start(const std::string &helper, const std::string &sysfs, std::string &err);
void live_poll();
void live_stop();
void live_set_status(const std::string &s);
const char *live_status();
