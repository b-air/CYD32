// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "radio.hpp"
namespace cz {
namespace {
int run_all(Exec &ex, const std::vector<Cmd> &cmds, std::string &err) {
  for (const auto &c : cmds) {
    std::string out, e;
    const int rc = ex.run(c.argv, out, e);
    if (rc != 0) {
      err = join_cmd(c);
      if (!e.empty()) { err += ": "; err += e; }
      else err += " failed";
      return rc;
    }
  }
  return 0;
}
}  // namespace

std::string join_cmd(const Cmd &c) {
  std::string s;
  for (const auto &a : c.argv) {
    if (!s.empty()) s += ' ';
    s += a;
  }
  return s;
}

std::vector<Cmd> plan_arm(const SavedLink &s) {
  const std::string &i = s.ifname;
  return {
      Cmd{{"nmcli", "dev", "set", i, "managed", "no"}},
      Cmd{{"ip", "link", "set", "dev", i, "down"}},
      Cmd{{"iw", "dev", i, "set", "type", "monitor"}},
      Cmd{{"ip", "link", "set", "dev", i, "up"}},
  };
}

std::vector<Cmd> plan_restore(const SavedLink &s) {
  const std::string &i = s.ifname;
  const std::string type = s.type.empty() ? "managed" : s.type;
  std::vector<Cmd> cmds = {
      Cmd{{"ip", "link", "set", "dev", i, "down"}},
      Cmd{{"iw", "dev", i, "set", "type", type}},
      Cmd{{"ip", "link", "set", "dev", i, "up"}},
  };
  if (s.nm_managed) cmds.push_back(Cmd{{"nmcli", "dev", "set", i, "managed", "yes"}});
  return cmds;
}

Cmd plan_hop(const std::string &ifname, int mhz) {
  return Cmd{{"iw", "dev", ifname, "set", "freq", std::to_string(mhz)}};
}

bool Session::arm(const DetectResult &d, const SavedLink &saved, std::string &err) {
  if (!arm_allowed(d, saved.ifname, err)) return false;
  saved_ = saved;
  if (saved_.type.empty()) saved_.type = "managed";
  const auto cmds = plan_arm(saved_);
  std::vector<Cmd> done;
  for (const auto &c : cmds) {
    std::string out, e;
    const int rc = ex_.run(c.argv, out, e);
    if (rc != 0) {
      err = join_cmd(c);
      if (!e.empty()) { err += ": "; err += e; }
      else err += " failed";
      // Put the iface back even when arm stopped halfway.
      std::string ignore;
      run_all(ex_, plan_restore(saved_), ignore);
      armed_ = false;
      return false;
    }
    done.push_back(c);
  }
  armed_ = true;
  err.clear();
  return true;
}

bool Session::restore(std::string &err) {
  if (saved_.ifname.empty()) { err = "nothing to restore"; return false; }
  const int rc = run_all(ex_, plan_restore(saved_), err);
  armed_ = false;
  return rc == 0;
}

bool Session::hop(int mhz, std::string &err) {
  if (!armed_) { err = "not armed"; return false; }
  std::string out, e;
  const Cmd c = plan_hop(saved_.ifname, mhz);
  const int rc = ex_.run(c.argv, out, e);
  if (rc != 0) {
    err = e.empty() ? join_cmd(c) + " failed" : e;
    if (mhz >= 5925) band6_failed_ = true;
    return false;
  }
  err.clear();
  return true;
}
}  // namespace cz
