// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Fixed-size sliding window of DISTINCT device ids (RAM only, no heap).
// add() refreshes an id's last-seen time; count() returns how many distinct
// ids were seen in the last windowMs. When the table is full, the entry with
// the oldest last-seen time is overwritten (it is the first to expire anyway),
// so the count saturates at N. Not thread-safe: the caller holds its lock.
#pragma once
#include <stdint.h>
template <int N>
class RecentWindow {
 public:
  void clear() { used_ = 0; }
  void add(uint32_t id, uint32_t now) {
    int oldest = 0;
    for (int i = 0; i < used_; i++) {
      if (e_[i].id == id) { e_[i].seen = now; return; }
      if ((uint32_t)(now - e_[i].seen) > (uint32_t)(now - e_[oldest].seen)) oldest = i;
    }
    const int slot = used_ < N ? used_++ : oldest;
    e_[slot].id = id;
    e_[slot].seen = now;
  }
  int count(uint32_t now, uint32_t windowMs) const {
    int n = 0;
    for (int i = 0; i < used_; i++) if ((uint32_t)(now - e_[i].seen) < windowMs) n++;
    return n;
  }
  static constexpr int capacity() { return N; }
 private:
  struct E { uint32_t id, seen; };
  E e_[N];
  int used_ = 0;
};
