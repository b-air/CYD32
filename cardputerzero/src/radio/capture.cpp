// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Receive-only packet socket. There is no send path in this file.
#include "capture.hpp"
#include "radio.hpp"
#include <arpa/inet.h>
#include <string.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <unistd.h>
namespace cz {
int open_rx_socket(const std::string &ifname, std::string &err) {
  if (!ifname_ok(ifname)) { err = "bad ifname"; return -1; }
  const int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (fd < 0) { err = "socket"; return -1; }
  sockaddr_ll addr;
  memset(&addr, 0, sizeof addr);
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons(ETH_P_ALL);
  addr.sll_ifindex = (int)if_nametoindex(ifname.c_str());
  if (addr.sll_ifindex == 0) { err = "if_nametoindex"; close(fd); return -1; }
  if (bind(fd, (sockaddr *)&addr, sizeof addr) != 0) { err = "bind"; close(fd); return -1; }
  err.clear();
  return fd;
}
}  // namespace cz
