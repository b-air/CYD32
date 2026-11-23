// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// RAM-only wardriving log for the StopWatch. See include/wardriving.h.
#ifdef CYD32_HW_M5STACK_STOPWATCH
#include "wardriving.h"
#include "big_alloc.h"
#include "boot_log.h"
#include <Arduino.h>
#include <string.h>

namespace {
// Heap, allocated once in begin() in PSRAM (internal heap as the fallback):
// ~20 KB that v0.2.0-test5 kept in internal .bss. RAM only, gone at reboot.
wardriving::Rec *table = nullptr;
det::Entry *snap = nullptr;  // det::snapshot() scratch, ~2.7 KB
int cap = 0, used = 0;
bool running = false;
uint32_t startedAt = 0;

int findId(uint32_t id) {
  for (int i = 0; i < used; i++) if (table[i].id == id) return i;
  return -1;
}
int oldestSlot() {
  int s = 0;
  for (int i = 1; i < used; i++) if (table[i].firstMs < table[s].firstMs) s = i;
  return s;
}
void upsert(const det::Entry &e) {
  // Only fields already stored in det::Entry (salted id, rssi, channel, kind).
  // Raw BSSIDs and SSIDs are never present in that table.
  int slot = findId(e.id);
  if (slot < 0) {
    slot = used < cap ? used++ : oldestSlot();
    table[slot].id = e.id;
    table[slot].firstMs = e.lastSeen;
  }
  table[slot].lastMs = e.lastSeen;
  table[slot].rssi = e.rssi;
  table[slot].channel = e.channel;
  table[slot].kind = (uint8_t)e.kind;
  table[slot].bearing = e.bearing;
}
}  // namespace

namespace wardriving {
void begin() {
  // Do not touch NVS or files. Zero-filled RAM, never persisted.
  if (!table) table = (Rec *)cyd32_big_alloc(sizeof(Rec) * kCap);
  if (!snap) snap = (det::Entry *)cyd32_big_alloc(sizeof(det::Entry) * det::kMax);
  cap = (table && snap) ? kCap : 0;  // no buffer: the log stays empty instead of crashing
  if (!cap) BOOT_LOG("wardriving: buffer allocation failed, log disabled");
  used = 0;
  running = false;
  startedAt = 0;
}
void start() {
  if (running) return;
  running = true;
  startedAt = millis();
}
void loop() {
  if (!running || !cap) return;
  const int n = det::snapshot(snap, det::kMax);
  for (int i = 0; i < n; i++) upsert(snap[i]);
}
bool active() { return running; }
uint32_t sessionMs() { return running ? (millis() - startedAt) : 0; }
int count() { return used; }
int countKind(det::Kind k) {
  int n = 0;
  for (int i = 0; i < used; i++) if (table[i].kind == (uint8_t)k) n++;
  return n;
}
const Rec *records() { return table; }
}
#endif
