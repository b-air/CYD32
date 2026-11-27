// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Runs ip / iw / nmcli without a shell. The UI process does not call this.
#include "radio.hpp"
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
namespace cz {
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
      std::vector<char *> av;
      av.reserve(argv.size() + 1);
      for (const auto &s : argv) av.push_back(const_cast<char *>(s.c_str()));
      av.push_back(nullptr);
      execvp(av[0], av.data());
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
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    err = "signal";
    return 128;
  }

 private:
  static bool ifname_ok_argv(const std::vector<std::string> &argv) {
    if (argv[0] != "ip" && argv[0] != "iw" && argv[0] != "nmcli") return false;
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
