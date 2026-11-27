// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "radio.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

namespace cz {
namespace {
std::string read_trim(const fs::path &p) {
  std::ifstream in(p);
  std::string s;
  std::getline(in, s);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s;
}
std::string lower(std::string s) {
  for (char &c : s) c = (char)tolower((unsigned char)c);
  return s;
}
bool internal_driver(const std::string &driver) {
  // CYW43439 on the CM0 uses the in-tree Broadcom full-MAC driver.
  return driver == "brcmfmac" || driver == "brcmfmac_wcc";
}
std::string usb_id_from(fs::path p) {
  std::error_code ec;
  for (int i = 0; i < 8; i++) {
    const auto ven = p / "idVendor";
    const auto prod = p / "idProduct";
    if (fs::is_regular_file(ven, ec) && fs::is_regular_file(prod, ec))
      return lower(read_trim(ven)) + ":" + lower(read_trim(prod));
    if (!p.has_parent_path() || p.parent_path() == p) break;
    p = p.parent_path();
  }
  return "";
}
}  // namespace

bool ifname_ok(const std::string &name) {
  if (name.empty() || name.size() > 15) return false;
  if (!std::isalpha((unsigned char)name[0])) return false;
  for (char c : name)
    if (!std::isalnum((unsigned char)c) && c != '_' && c != '-') return false;
  return true;
}

DetectResult detect_ifaces(const std::string &sys_root) {
  DetectResult r;
  const fs::path net = fs::path(sys_root) / "class" / "net";
  std::error_code ec;
  if (!fs::is_directory(net, ec)) {
    r.note = "no " + net.string();
    return r;
  }
  const bool module = fs::is_directory(fs::path(sys_root) / "module" / "mt7921u", ec);
  std::string taint;
  if (module) taint = read_trim(fs::path(sys_root) / "module" / "mt7921u" / "taint");
  const bool oot = taint.find('O') != std::string::npos || taint.find('o') != std::string::npos;

  for (const auto &ent : fs::directory_iterator(net, ec)) {
    if (!ent.is_directory()) continue;
    Iface iface;
    iface.name = ent.path().filename().string();
    if (!ifname_ok(iface.name)) continue;
    const fs::path dev = ent.path() / "device";
    if (!fs::exists(dev, ec)) continue;
    const fs::path drv = dev / "driver";
    if (fs::is_symlink(drv, ec)) iface.driver = fs::read_symlink(drv, ec).filename().string();
    iface.internal = internal_driver(iface.driver);
    fs::path resolved = dev;
    if (fs::is_symlink(dev, ec)) {
      const fs::path link = fs::read_symlink(dev, ec);
      resolved = link.is_relative() ? fs::weakly_canonical(dev.parent_path() / link, ec) : link;
    } else {
      resolved = fs::weakly_canonical(dev, ec);
    }
    iface.usb_id = usb_id_from(ec ? dev : resolved);
    if (iface.driver == "mt7921u") {
      iface.in_kernel = module && !oot;
      iface.out_of_tree = module && oot;
    }
    r.ifaces.push_back(iface);
  }
  std::sort(r.ifaces.begin(), r.ifaces.end(), [](const Iface &a, const Iface &b) { return a.name < b.name; });

  const Iface *exact = nullptr, *driver_only = nullptr;
  for (const auto &iface : r.ifaces) {
    if (iface.internal || iface.driver != "mt7921u") continue;
    if (!driver_only) driver_only = &iface;
    if (iface.usb_id == "0e8d:7961") { exact = &iface; break; }
  }
  if (exact) {
    r.chosen = exact->name;
    if (!module) r.note = "mt7921u sysfs driver is linked but /sys/module/mt7921u is absent";
    else if (oot) r.note = "mt7921u is loaded out-of-tree (module taint O); prefer the in-kernel driver";
    else r.note = "in-kernel mt7921u, usb 0e8d:7961";
  } else if (driver_only) {
    r.chosen = driver_only->name;
    r.note = "mt7921u is bound but the USB id is not 0e8d:7961 (confirm with lsusb; the kernel table lists that id)";
    if (oot) r.note += "; module is out-of-tree";
  } else {
    r.note = "no mt7921u netdev (internal CYW43439 is ignored)";
  }
  return r;
}

const Iface *find_iface(const DetectResult &d, const std::string &name) {
  for (const auto &iface : d.ifaces)
    if (iface.name == name) return &iface;
  return nullptr;
}

bool arm_allowed(const DetectResult &d, const std::string &ifname, std::string &why) {
  if (!ifname_ok(ifname)) { why = "interface name rejected"; return false; }
  const Iface *iface = find_iface(d, ifname);
  if (!iface) { why = "interface is not in the detect result"; return false; }
  if (iface->internal) { why = "refusing to change the internal CYW43439"; return false; }
  if (iface->driver != "mt7921u") { why = "driver is not mt7921u"; return false; }
  why.clear();
  return true;
}
}  // namespace cz
