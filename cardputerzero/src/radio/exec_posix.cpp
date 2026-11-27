// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Runs /usr/sbin/ip, /usr/sbin/iw and /usr/bin/nmcli with execve, a fixed
// environment, and no shell. Paths are the Debian/Raspberry Pi OS locations;
// confirm them on the official M5 image before arming. The UI process does
// not call this. File capabilities on this helper are +ep: ip/iw would run
// unprivileged unless CAP_NET_ADMIN is raised in the ambient set just before
// exec. Only that cap is raised, and only for ip/iw (not nmcli, not CAP_NET_RAW).
#include "radio.hpp"
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/capability.h>
#include <unistd.h>
#include <vector>
#ifndef PR_CAP_AMBIENT
#define PR_CAP_AMBIENT 47
#define PR_CAP_AMBIENT_RAISE 2
#endif
namespace cz {
namespace {
constexpr const char *kIp = "/usr/sbin/ip";
constexpr const char *kIw = "/usr/sbin/iw";
constexpr const char *kNmcli = "/usr/bin/nmcli";

int raise_ambient_net_admin() {
  struct __user_cap_header_struct hdr {};
  hdr.version = _LINUX_CAPABILITY_VERSION_3;
  hdr.pid = 0;
  struct __user_cap_data_struct data[2] {};
  if (syscall(SYS_capget, &hdr, data) != 0) return -1;
  const unsigned cap = CAP_NET_ADMIN;
  data[CAP_TO_INDEX(cap)].inheritable |= CAP_TO_MASK(cap);
  if (syscall(SYS_capset, &hdr, data) != 0) return -1;
  if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, (unsigned long)cap, 0, 0) != 0) return -1;
  return 0;
}
}  // namespace

class PosixExec : public Exec {
 public:
  int run(const std::vector<std::string> &argv, std::string &out, std::string &err) override {
    out.clear();
    err.clear();
    if (argv.empty() || !ifname_ok_argv(argv)) { err = "empty command"; return 127; }
    int outp[2], errp[2];
    if (pipe(outp) != 0 || pipe(errp) != 0) { err = "pipe"; return 127; }
    const pid_t pid = fork();
    if (pid < 0) { err = "fork"; return 127; }
    if (pid == 0) {
      dup2(outp[1], 1);
      dup2(errp[1], 2);
      close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
      if (argv[0] == kIp || argv[0] == kIw) {
        if (geteuid() != 0 && raise_ambient_net_admin() != 0) _exit(126);
      }
      std::vector<char *> av;
      av.reserve(argv.size() + 1);
      for (const auto &s : argv) av.push_back(const_cast<char *>(s.c_str()));
      av.push_back(nullptr);
      char lc[] = "LC_ALL=C";
      char path[] = "PATH=/usr/sbin:/usr/bin:/sbin:/bin";
      char *envp[] = {lc, path, nullptr};
      execve(av[0], av.data(), envp);
      _exit(127);
    }
    close(outp[1]);
    close(errp[1]);
    read_all(outp[0], out);
    read_all(errp[0], err);
    close(outp[0]);
    close(errp[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) { err = "waitpid"; return 127; }
    if (WIFEXITED(st)) {
      const int rc = WEXITSTATUS(st);
      if (rc == 126 && err.empty()) err = "cannot raise CAP_NET_ADMIN for ip/iw";
      return rc;
    }
    err = "signal";
    return 128;
  }

 private:
  static bool ifname_ok_argv(const std::vector<std::string> &argv) {
    if (argv[0] != kIp && argv[0] != kIw && argv[0] != kNmcli) return false;
    return true;
  }
  static void read_all(int fd, std::string &s) {
    char buf[256];
    while (true) {
      const ssize_t n = ::read(fd, buf, sizeof buf);
      if (n > 0) s.append(buf, buf + n);
      else break;
    }
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
  }
};

Exec *new_posix_exec() { return new PosixExec(); }
}  // namespace cz
