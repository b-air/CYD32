// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Privileged helper (CAP_NET_ADMIN, CAP_NET_RAW). The UI does not run as root.
// Commands: detect, dry-arm, dry-restore, arm, restore, hop, serve.
#include "radio.hpp"
#include "exec_posix.hpp"
#include "capture.hpp"
#include "ble_sock.hpp"
#include <cstdlib>
#include <errno.h>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <vector>
namespace {
volatile sig_atomic_t g_stop = 0;
void on_sig(int) { g_stop = 1; }

void usage() {
  std::cerr <<
      "cyd32-awus-helper detect [--sys PATH]\n"
      "cyd32-awus-helper dry-arm --if NAME --type TYPE --nm yes|no\n"
      "cyd32-awus-helper dry-restore --if NAME --type TYPE --nm yes|no\n"
      "cyd32-awus-helper arm --if NAME [--sys PATH]\n"
      "cyd32-awus-helper restore --if NAME --type TYPE --nm yes|no\n"
      "cyd32-awus-helper hop --if NAME --mhz N\n"
      "cyd32-awus-helper serve [--sys PATH]\n";
}

std::string arg(int argc, char **argv, const std::string &key, const std::string &def = "") {
  for (int i = 1; i + 1 < argc; i++)
    if (key == argv[i]) return argv[i + 1];
  return def;
}

cz::SavedLink link_from_args(int argc, char **argv) {
  cz::SavedLink s;
  s.ifname = arg(argc, argv, "--if");
  s.type = arg(argc, argv, "--type", "managed");
  s.nm_managed = arg(argc, argv, "--nm", "yes") != "no";
  return s;
}

void print_plan(const char *title, const std::vector<cz::Cmd> &cmds) {
  std::cout << "# " << title << "\n";
  for (const auto &c : cmds) std::cout << cz::join_cmd(c) << "\n";
}

std::string hex_of(const uint8_t *p, size_t n) {
  static const char *k = "0123456789abcdef";
  std::string s;
  s.resize(n * 2);
  for (size_t i = 0; i < n; i++) {
    s[i * 2] = k[p[i] >> 4];
    s[i * 2 + 1] = k[p[i] & 0xf];
  }
  return s;
}

int cmd_detect(int argc, char **argv) {
  const auto d = cz::detect_ifaces(arg(argc, argv, "--sys", "/sys"));
  for (const auto &iface : d.ifaces) {
    std::cout << (iface.internal ? "skip " : "see ") << iface.name
              << " driver=" << (iface.driver.empty() ? "-" : iface.driver)
              << " usb=" << (iface.usb_id.empty() ? "-" : iface.usb_id);
    if (iface.internal) std::cout << " reason=internal";
    else if (iface.driver == "mt7921u")
      std::cout << " module=" << (iface.out_of_tree ? "out-of-tree" : iface.in_kernel ? "in-kernel" : "unloaded");
    std::cout << "\n";
  }
  std::cout << "chosen " << (d.chosen.empty() ? "none" : d.chosen) << "\n";
  if (!d.note.empty()) std::cout << "note " << d.note << "\n";
  return d.chosen.empty() ? 1 : 0;
}

int serve(const std::string &sys_root) {
  std::unique_ptr<cz::Exec> ex(cz::new_posix_exec());
  cz::Session session(*ex);
  signal(SIGINT, on_sig);
  signal(SIGTERM, on_sig);
  signal(SIGHUP, on_sig);
  std::string line, armed_if;
  int pfd = -1, bfd = -1;
  auto restore_now = [&]() {
    if (pfd >= 0) { close(pfd); pfd = -1; }
    if (bfd >= 0) { close(bfd); bfd = -1; }
    if (session.armed()) {
      std::string err;
      if (!session.restore(err)) std::cerr << "restore: " << err << "\n";
    }
  };
  while (!g_stop) {
    pollfd fds[3];
    int nfd = 0;
    fds[nfd++] = pollfd{0, POLLIN, 0};
    if (pfd >= 0) fds[nfd++] = pollfd{pfd, POLLIN, 0};
    if (bfd >= 0) fds[nfd++] = pollfd{bfd, POLLIN, 0};
    const int pr = poll(fds, nfd, 200);
    if (pr < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) break;
    if (fds[0].revents & POLLIN) {
      char buf[512];
      const ssize_t n = read(0, buf, sizeof buf);
      if (n <= 0) break;
      line.append(buf, buf + n);
      size_t nl;
      while ((nl = line.find('\n')) != std::string::npos) {
        std::string cmd = line.substr(0, nl);
        line.erase(0, nl + 1);
        if (!cmd.empty() && cmd.back() == '\r') cmd.pop_back();
        if (cmd == "QUIT" || cmd.empty()) { restore_now(); std::cout << "BYE\n" << std::flush; return 0; }
        if (cmd == "DETECT") {
          const auto d = cz::detect_ifaces(sys_root);
          if (d.chosen.empty()) std::cout << "NONE " << d.note << "\n";
          else std::cout << "CHOSEN " << d.chosen << " " << d.note << "\n";
          std::cout << std::flush;
        } else if (cmd.compare(0, 4, "ARM ") == 0) {
          cz::SavedLink s;
          s.ifname = cmd.substr(4);
          s.type = "managed";
          s.nm_managed = true;
          const auto d = cz::detect_ifaces(sys_root);
          std::string err;
          if (!session.arm(d, s, err)) std::cout << "ERR " << err << "\n";
          else { armed_if = s.ifname; std::cout << "OK\n"; }
          std::cout << std::flush;
        } else if (cmd == "OPEN") {
          std::string err;
          if (pfd >= 0) { std::cout << "OK\n" << std::flush; continue; }
          pfd = cz::open_rx_socket(armed_if, err);
          std::cout << (pfd < 0 ? "ERR " + err : "OK") << "\n" << std::flush;
        } else if (cmd.compare(0, 4, "HOP ") == 0) {
          std::string err;
          const int mhz = std::atoi(cmd.c_str() + 4);
          if (!session.hop(mhz, err)) std::cout << "ERR " << err << "\n";
          else std::cout << "OK " << mhz << "\n";
          std::cout << std::flush;
        } else if (cmd == "BLE") {
          std::string err;
          const std::string hci = cz::find_internal_hci(sys_root);
          if (hci.empty()) std::cout << "ERR no internal hci\n";
          else {
            bfd = cz::open_passive_hci(hci, err);
            std::cout << (bfd < 0 ? "ERR " + err : "OK " + hci) << "\n";
          }
          std::cout << std::flush;
        } else if (cmd == "STOP") {
          if (pfd >= 0) { close(pfd); pfd = -1; }
          std::cout << "OK\n" << std::flush;
        } else if (cmd == "RESTORE") {
          restore_now();
          std::cout << "OK\n" << std::flush;
        } else {
          std::cout << "ERR unknown\n" << std::flush;
        }
      }
    }
    auto read_sock = [&](int &fd, int slot, bool wifi) {
      if (fd < 0 || slot >= nfd || !(fds[slot].revents & POLLIN)) return;
      uint8_t buf[2048];
      const ssize_t n = recv(fd, buf, sizeof buf, 0);
      if (n <= 0) return;
      if (wifi) {
        cz::RxFrame rx;
        if (cz::parse_monitor_frame(buf, (size_t)n, rx) && rx.accepted)
          std::cout << "FRAME " << hex_of(buf, (size_t)n) << "\n" << std::flush;
      } else {
        std::vector<cz::AdvReport> adv;
        if (cz::parse_le_adv_report(buf, (size_t)n, adv) && !adv.empty())
          std::cout << "ADV " << hex_of(buf, (size_t)n) << "\n" << std::flush;
      }
    };
    int slot = 1;
    if (pfd >= 0) read_sock(pfd, slot++, true);
    if (bfd >= 0) read_sock(bfd, slot, false);
  }
  restore_now();
  return 0;
}
}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) { usage(); return 2; }
  const std::string cmd = argv[1];
  if (cmd == "detect") return cmd_detect(argc, argv);
  if (cmd == "dry-arm" || cmd == "dry-restore") {
    const auto s = link_from_args(argc, argv);
    if (!cz::ifname_ok(s.ifname)) { std::cerr << "bad --if\n"; return 2; }
    if (cmd == "dry-arm") print_plan("arm", cz::plan_arm(s));
    print_plan("restore", cz::plan_restore(s));
    return 0;
  }
  std::unique_ptr<cz::Exec> ex(cz::new_posix_exec());
  cz::Session session(*ex);
  if (cmd == "arm") {
    cz::SavedLink link = link_from_args(argc, argv);
    if (link.type.empty()) link.type = "managed";
    const auto d = cz::detect_ifaces(arg(argc, argv, "--sys", "/sys"));
    std::string err;
    if (!session.arm(d, link, err)) { std::cerr << err << "\n"; return 1; }
    std::cout << "armed " << link.ifname << "\n";
    return 0;
  }
  if (cmd == "restore") {
    const auto s = link_from_args(argc, argv);
    // Restore does not require a prior arm in this process: replay the plan.
    std::string err;
    // Use a one-shot by arming the saved fields through plan only.
    std::unique_ptr<cz::Exec> live(cz::new_posix_exec());
    for (const auto &c : cz::plan_restore(s)) {
      std::string out, e;
      if (live->run(c.argv, out, e) != 0) { std::cerr << cz::join_cmd(c) << " " << e << "\n"; return 1; }
    }
    std::cout << "restored " << s.ifname << "\n";
    return 0;
  }
  if (cmd == "hop") {
    const auto s = link_from_args(argc, argv);
    const int mhz = std::atoi(arg(argc, argv, "--mhz", "0").c_str());
    std::string out, e;
    const auto c = cz::plan_hop(s.ifname, mhz);
    if (ex->run(c.argv, out, e) != 0) { std::cerr << e << "\n"; return 1; }
    std::cout << "hop " << mhz << "\n";
    return 0;
  }
  if (cmd == "serve") return serve(arg(argc, argv, "--sys", "/sys"));
  usage();
  return 2;
}
