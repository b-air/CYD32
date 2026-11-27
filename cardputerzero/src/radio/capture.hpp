/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
#pragma once
#include <string>
namespace cz {
// AF_PACKET socket used only with recv(). CAP_NET_RAW.
int open_rx_socket(const std::string &ifname, std::string &err);
}
