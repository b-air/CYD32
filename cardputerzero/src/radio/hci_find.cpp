// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "radio.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
namespace cz {
namespace {
std::string read_trim(const fs::path &p) {
  std::ifstream in(p);
  std::string s;
  std::getline(in, s);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
  return s;
}
bool is_awus(fs::path p) {
  std::error_code ec;
  for (int i = 0; i < 8; i++) {
    if (fs::is_regular_file(p / "idVendor", ec) && fs::is_regular_file(p / "idProduct", ec)) {
      auto v = read_trim(p / "idVendor"), pr = read_trim(p / "idProduct");
      for (char &c : v) c = (char)tolower((unsigned char)c);
      for (char &c : pr) c = (char)tolower((unsigned char)c);
      return v == "0e8d" && pr == "7961";
    }
    if (!p.has_parent_path() || p.parent_path() == p) break;
    p = p.parent_path();
  }
  return false;
}
}  // namespace
std::string find_internal_hci(const std::string &sys_root) {
  const fs::path root = fs::path(sys_root) / "class" / "bluetooth";
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return "";
  std::vector<std::string> names;
  for (const auto &ent : fs::directory_iterator(root, ec)) names.push_back(ent.path().filename().string());
  std::sort(names.begin(), names.end());
  for (const auto &name : names) {
    if (name.compare(0, 3, "hci") != 0) continue;
    fs::path dev = root / name / "device";
    fs::path resolved = dev;
    if (fs::is_symlink(dev, ec)) {
      fs::path link = fs::read_symlink(dev, ec);
      resolved = link.is_relative() ? fs::weakly_canonical(dev.parent_path() / link, ec) : link;
    }
    if (!is_awus(resolved)) return name;
  }
  return "";
}
}  // namespace cz
