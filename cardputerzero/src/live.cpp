// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Talks to cyd32-awus-helper. This process has no CAP_NET_ADMIN / CAP_NET_RAW.
#include "live.hpp"
#include "radio.hpp"
#include "Arduino.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
namespace {
int to_child = -1, from_child = -1;
pid_t child = -1;
std::string pending, status = "SIM";
bool up = false;
size_t hop_i = 0;
uint32_t next_hop = 0;
bool skip6 = false;

int nyb(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool unhex(const std::string &s, std::vector<uint8_t> &out) {
  if (s.size() < 2 || (s.size() & 1)) return false;
  out.resize(s.size() / 2);
  for (size_t i = 0; i < out.size(); i++) {
    const int hi = nyb(s[i * 2]), lo = nyb(s[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}
void ingest_line(const std::string &line) {
  if (line.compare(0, 6, "FRAME ") == 0) {
    std::vector<uint8_t> raw;
    if (!unhex(line.substr(6), raw)) return;
    cz::RxFrame rx;
    if (cz::parse_monitor_frame(raw.data(), raw.size(), rx)) cz::ingest_frame(rx);
  } else if (line.compare(0, 4, "ADV ") == 0) {
    std::vector<uint8_t> raw;
    std::vector<cz::AdvReport> adv;
    if (unhex(line.substr(4), raw) && cz::parse_le_adv_report(raw.data(), raw.size(), adv))
      for (const auto &a : adv) cz::ingest_adv(a);
  }
}
bool read_some() {
  if (from_child < 0) return false;
  char buf[1024];
  const ssize_t n = read(from_child, buf, sizeof buf);
  if (n > 0) { pending.append(buf, buf + n); return true; }
  if (n == 0) { up = false; status = "helper closed"; return false; }
  return errno == EAGAIN || errno == EWOULDBLOCK;
}
// Pull complete lines. Reply lines (not FRAME/ADV) are returned via `reply` when non-null.
bool take_line(std::string &line) {
  const size_t nl = pending.find('\n');
  if (nl == std::string::npos) return false;
  line = pending.substr(0, nl);
  pending.erase(0, nl + 1);
  if (!line.empty() && line.back() == '\r') line.pop_back();
  return true;
}
bool transact(const std::string &cmd, std::string &reply, int timeout_ms) {
  reply.clear();
  if (to_child < 0) return false;
  const std::string msg = cmd + "\n";
  if (write(to_child, msg.data(), msg.size()) != (ssize_t)msg.size()) return false;
  const uint32_t end = millis() + (uint32_t)timeout_ms;
  while ((int32_t)(millis() - end) < 0) {
    read_some();
    std::string line;
    while (take_line(line)) {
      if (line.compare(0, 6, "FRAME ") == 0 || line.compare(0, 4, "ADV ") == 0) { ingest_line(line); continue; }
      reply = line;
      return true;
    }
    usleep(2000);
  }
  return false;
}
}  // namespace

bool live_start(const std::string &helper, const std::string &sysfs, std::string &err) {
  (void)sysfs;  // helper privileged modes are hard-coded to /sys

  int in[2], out[2];
  if (pipe(in) != 0 || pipe(out) != 0) { err = "pipe"; status = err; return false; }
  child = fork();
  if (child < 0) { err = "fork"; status = err; return false; }
  if (child == 0) {
    dup2(in[0], 0);
    dup2(out[1], 1);
    close(in[0]); close(in[1]); close(out[0]); close(out[1]);
    execl(helper.c_str(), helper.c_str(), "serve", (char *)nullptr);
    _exit(127);
  }
  close(in[0]);
  close(out[1]);
  to_child = in[1];
  from_child = out[0];
  fcntl(from_child, F_SETFL, O_NONBLOCK);
  std::string reply;
  if (!transact("DETECT", reply, 1500)) { err = "helper did not answer DETECT"; status = err; live_stop(); return false; }
  if (reply.compare(0, 7, "CHOSEN ") != 0) { err = reply.empty() ? "no AWUS" : reply; status = err; live_stop(); return false; }
  const std::string rest = reply.substr(7);
  const auto sp = rest.find(' ');
  const std::string ifname = sp == std::string::npos ? rest : rest.substr(0, sp);
  if (!transact("ARM " + ifname, reply, 4000) || reply.compare(0, 2, "OK") != 0) {
    err = reply.empty() ? "arm failed" : reply;
    status = err;
    live_stop();
    return false;
  }
  transact("OPEN", reply, 1500);
  transact("BLE", reply, 1500);
  status = "AWUS " + ifname;
  up = true;
  next_hop = millis() + 125;
  hop_i = 0;
  err.clear();
  return true;
}

void live_poll() {
  if (!up) return;
  read_some();
  std::string line;
  while (take_line(line)) ingest_line(line);
  if ((int32_t)(millis() - next_hop) < 0) return;
  next_hop = millis() + 125;
  const auto &table = cz::hop_table();
  if (table.empty()) return;
  for (size_t n = 0; n < table.size(); n++) {
    const auto &ch = table[hop_i++ % table.size()];
    if (skip6 && ch.band == 60) continue;
    std::string reply;
    if (!transact("HOP " + std::to_string(ch.mhz), reply, 400)) break;
    if (reply.compare(0, 2, "OK") == 0) break;
    if (ch.band == 60) { skip6 = true; status += " (6 GHz hop failed)"; }
    break;
  }
}

void live_stop() {
  if (to_child >= 0) {
    const char *msg = "RESTORE\nQUIT\n";
    (void)write(to_child, msg, strlen(msg));
    close(to_child);
    to_child = -1;
  }
  if (from_child >= 0) { close(from_child); from_child = -1; }
  if (child > 0) {
    int st = 0;
    for (int i = 0; i < 20 && waitpid(child, &st, WNOHANG) == 0; i++) usleep(10000);
    if (waitpid(child, &st, WNOHANG) == 0) { kill(child, SIGTERM); waitpid(child, &st, 0); }
    child = -1;
  }
  up = false;
}

void live_set_status(const std::string &s) { if (!s.empty()) status = s; }
const char *live_status() { return status.c_str(); }
