// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Opens a Bluetooth HCI socket and programs a passive LE scan (scan type 0).
#include "ble_sock.hpp"
#include "radio.hpp"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif
#ifndef BTPROTO_HCI
#define BTPROTO_HCI 1
#endif
namespace {
struct sockaddr_hci {
  sa_family_t hci_family;
  unsigned short hci_dev;
  unsigned short hci_channel;
};
int hci_index(const std::string &name) {
  if (name.size() < 4 || name.compare(0, 3, "hci") != 0) return -1;
  int n = 0;
  for (size_t i = 3; i < name.size(); i++) {
    if (name[i] < '0' || name[i] > '9') return -1;
    n = n * 10 + (name[i] - '0');
  }
  return n;
}
}  // namespace
namespace cz {
int open_passive_hci(const std::string &hci_name, std::string &err) {
  const int idx = hci_index(hci_name);
  if (idx < 0) { err = "bad hci name"; return -1; }
  const int fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) { err = "hci socket"; return -1; }
  sockaddr_hci addr;
  memset(&addr, 0, sizeof addr);
  addr.hci_family = AF_BLUETOOTH;
  addr.hci_dev = (unsigned short)idx;
  addr.hci_channel = 0;
  if (bind(fd, (sockaddr *)&addr, sizeof addr) != 0) {
    err = "hci bind";
    close(fd);
    return -1;
  }
  const HciCmd params = le_set_scan_params_passive();
  const HciCmd enable = le_set_scan_enable(true);
  if (write(fd, params.bytes, params.len) != (ssize_t)params.len ||
      write(fd, enable.bytes, enable.len) != (ssize_t)enable.len) {
    err = "hci write";
    close(fd);
    return -1;
  }
  err.clear();
  return fd;
}

void close_passive_hci(int fd) {
  if (fd < 0) return;
  const HciCmd off = le_set_scan_enable(false);
  (void)write(fd, off.bytes, off.len);
  close(fd);
}
}  // namespace cz
