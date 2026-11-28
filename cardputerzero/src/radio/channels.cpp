// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "radio.hpp"
namespace cz {
namespace {
int ch24(int mhz) { return mhz == 2484 ? 14 : (mhz - 2407) / 5; }
int ch5(int mhz) { return (mhz - 5000) / 5; }
int ch6(int mhz) { return (mhz - 5950) / 5; }
std::vector<Chan> build() {
  std::vector<Chan> t;
  // 2.4 GHz, busy channels first (same idea as the ESP32 hop table), then the rest.
  const int g24[] = {2412, 2437, 2462, 2417, 2422, 2427, 2432, 2442, 2447, 2452, 2457, 2467, 2472, 2484};
  for (int m : g24) t.push_back(Chan{m, 24, ch24(m)});
  const int g5[] = {5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320, 5500, 5520, 5540, 5560, 5580,
                    5600, 5620, 5640, 5660, 5680, 5700, 5720, 5745, 5765, 5785, 5805, 5825};
  for (int m : g5) t.push_back(Chan{m, 50, ch5(m)});
  // 6 GHz centres (20 MHz channels). Alfa does not document monitor mode here;
  // a rejected `iw set freq` is recorded and that band is skipped afterwards.
  const int g6[] = {5955, 5975, 5995, 6015, 6055, 6135, 6215, 6295, 6375, 6455,
                    6535, 6615, 6695, 6775, 6855, 6935, 7015, 7095};
  for (int m : g6) t.push_back(Chan{m, 60, ch6(m)});
  return t;
}
}  // namespace
const std::vector<Chan> &hop_table() {
  static const std::vector<Chan> t = build();
  return t;
}
}  // namespace cz
