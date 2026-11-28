/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
#pragma once
#include <string>
namespace cz {
// Passive scan on an internal HCI adapter. Returns a socket or -1.
int open_passive_hci(const std::string &hci_name, std::string &err);
// Send LE Scan Enable=0, then close. Safe on fd < 0.
void close_passive_hci(int fd);
}
