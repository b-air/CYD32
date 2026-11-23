// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Host tests for the pure logic shared by both targets: indicative bearing
// (FNV-1a), RSSI smoothing / radius, the 3-minute creature window, creature
// size rules and PET (forget the 25 oldest), the display categories
// (det::inCat, TAGGED), the proximity beep cadence and the badge signatures.
// No Arduino, no ESP-IDF.
// Run: tools/hosttest/run.sh
#include <stdio.h>
#include <string.h>
#include "bearing.h"
#include "creature_rules.h"
#include "detections.h"
#include "prox_beep.h"
#include "recent_window.h"
#include "rssi_track.h"
#include "signatures.h"

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static void testBearing() {
  // Published FNV-1a 32-bit vectors.
  CHECK(bearing::fnv1a((const uint8_t *)"", 0) == 0x811C9DC5u);
  CHECK(bearing::fnv1a((const uint8_t *)"a", 1) == 0xE40C292Cu);
  CHECK(bearing::fnv1a((const uint8_t *)"foobar", 6) == 0xBF9CF968u);
  const uint8_t m1[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  const uint8_t m2[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x56};
  // Deterministic, unsalted: same input, same output, every call / device.
  CHECK(bearing::fromMac(m1) == bearing::fromMac(m1));
  CHECK(bearing::fromMac(m1) < 360);
  CHECK(bearing::fromMac(m1) != bearing::fromMac(m2));
  const uint32_t h = bearing::fnv1a(m1, 6);
  CHECK(bearing::fromMac(m1) == (uint16_t)(((uint64_t)h * 360u) >> 32));
  // Spread: 36 x 10-degree buckets over 3600 addresses, none empty or > 3x mean.
  int bucket[36] = {0};
  for (int i = 0; i < 3600; i++) {
    const uint8_t m[6] = {0x3c, 0x22, 0xfb, (uint8_t)(i >> 8), (uint8_t)i, 0x10};
    bucket[bearing::fromMac(m) / 10]++;
  }
  for (int b : bucket) CHECK(b > 0 && b < 300);
  const uint8_t la[6] = {0x02, 0, 0, 0, 0, 1}, ua[6] = {0x00, 0, 0, 0, 0, 1};
  CHECK(bearing::isRandomized(la) && !bearing::isRandomized(ua));
}

static void testRssi() {
  int16_t q = rssi::emaStep(0, true, -70);
  CHECK(rssi::fromQ4(q) == -70);
  q = rssi::emaStep(q, false, -50);  // stronger: fast attack (10/16)
  CHECK(rssi::fromQ4(q) <= -57 && rssi::fromQ4(q) >= -58);
  const int16_t q2 = rssi::emaStep(q, false, -90);  // weaker: slow decay (5/16)
  CHECK(rssi::fromQ4(q2) > -70);
  // Converges to a steady value.
  int16_t s = rssi::emaStep(0, true, -80);
  for (int i = 0; i < 40; i++) s = rssi::emaStep(s, false, -60);
  CHECK(rssi::fromQ4(s) == -60);
  // Ring mapping: -50 / -65 / -80 / -95 dBm on the 184 px StopWatch scope.
  CHECK(rssi::radius(-50, 184, 0, 999) == 46);
  CHECK(rssi::radius(-65, 184, 0, 999) == 92);
  CHECK(rssi::radius(-80, 184, 0, 999) == 138);
  CHECK(rssi::radius(-95, 184, 0, 999) == 184);
  CHECK(rssi::radius(-30, 184, 6, 999) == 6);    // clamped
  CHECK(rssi::radius(-120, 72, 0, 70) == 70);    // clamped (CYD)
  CHECK(strcmp(rssi::zone(-40), "VERY CLOSE") == 0);
  CHECK(strcmp(rssi::zone(-48), "CLOSE") == 0);
  CHECK(strcmp(rssi::zone(-90), "FAR") == 0);
}

static void testWindowAndCreature() {
  static RecentWindow<192> w;
  w.clear();
  const uint32_t W = creature_rules::kWindowMs;
  CHECK(w.count(0, W) == 0);
  CHECK(creature_rules::sizeFor(0, false) == 0);   // egg
  w.add(1, 1000);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(1000, W), false) == 1);  // hatch at 1
  for (uint32_t i = 2; i <= 30; i++) w.add(i, 1000 + i);
  CHECK(w.count(2000, W) == 30);
  CHECK(creature_rules::sizeFor(30, true) == 2);
  w.add(5, 5000);  // same id again: still 30 distinct
  CHECK(w.count(5000, W) == 30);
  for (uint32_t i = 31; i <= 60; i++) w.add(i, 6000);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(6000, W), true) == 3);
  for (uint32_t i = 61; i <= 120; i++) w.add(i, 7000);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(7000, W), true) == 4);
  // Sliding: 3 minutes later only the ids refreshed after t = 7000 - ... remain.
  CHECK(w.count(1000 + W + 100, W) == 120 - 30 + 1);  // ids 1..30 (except 5) left the window
  CHECK(w.count(7000 + W, W) == 0);
  CHECK(creature_rules::sizeFor(0, true) == 1);     // shrinks, never back to an egg
  CHECK(creature_rules::sizeFor(45, true) == 2);    // shrink 4 -> 2
  CHECK(creature_rules::nextAt(0) == 1 && creature_rules::nextAt(1) == 30 && creature_rules::nextAt(3) == 120 &&
        creature_rules::nextAt(4) == 0);
  CHECK(creature_rules::floorAt(2) == 30 && creature_rules::floorAt(4) == 120);
  // Bounded: a flood of 1000 ids keeps 192 slots, the oldest evicted.
  static RecentWindow<192> f;
  f.clear();
  for (uint32_t i = 0; i < 1000; i++) f.add(1000 + i, 10000 + i);
  CHECK(f.count(11000, W) == 192);
  CHECK(RecentWindow<192>::capacity() == 192);
}

// PET: forget the 25 oldest devices of the 3-minute window, size recomputed
// at once, never back to the egg.
static void testPet() {
  const uint32_t W = creature_rules::kWindowMs;
  const int K = creature_rules::kPetForget;
  CHECK(K == 25);
  static RecentWindow<192> w;
  // 130 devices, id i last seen at t = 1000 + i: size 4.
  w.clear();
  for (uint32_t i = 1; i <= 130; i++) w.add(i, 1000 + i);
  uint32_t now = 2000;
  CHECK(w.count(now, W) == 130);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(now, W), true) == 4);
  CHECK(w.evictOldest(K, now, W) == 25);
  CHECK(w.count(now, W) == 105);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(now, W), true) == 3);  // 4 -> 3 immediately
  // The oldest were removed: ids 1..25 are gone, 26 is still there (re-adding
  // 26 does not raise the count, re-adding 1 does).
  w.add(26, now);
  CHECK(w.count(now, W) == 105);
  w.add(1, now);
  CHECK(w.count(now, W) == 106);
  // Fewer than 25 devices: all of them are forgotten, but a hatched creature
  // stays at size 1 (never an egg again).
  w.clear();
  for (uint32_t i = 1; i <= 10; i++) w.add(i, 1000 + i);
  CHECK(w.evictOldest(K, now, W) == 10);
  CHECK(w.count(now, W) == 0);
  CHECK(creature_rules::sizeFor(0, true) == 1);
  // Exactly 25: all gone.
  w.clear();
  for (uint32_t i = 1; i <= 25; i++) w.add(i, 1000 + i);
  CHECK(w.evictOldest(K, now, W) == 25 && w.count(now, W) == 0);
  // Entries that already left the window are not part of the 25.
  w.clear();
  for (uint32_t i = 1; i <= 20; i++) w.add(i, 1000);           // expired at now2
  for (uint32_t i = 21; i <= 60; i++) w.add(i, 200000 + i);    // 40 in the window
  const uint32_t now2 = 200100;  // ids 1..20 are 199100 ms old: out of the window
  CHECK(w.count(now2, W) == 40);
  CHECK(w.evictOldest(K, now2, W) == 25);
  CHECK(w.count(now2, W) == 15);
  // Empty window: nothing to forget.
  w.clear();
  CHECK(w.evictOldest(K, now, W) == 0);
  // Size 2 -> 1 at once (35 devices -> 10).
  for (uint32_t i = 1; i <= 35; i++) w.add(i, 1000 + i);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(now, W), true) == 2);
  w.evictOldest(K, now, W);
  CHECK(creature_rules::sizeFor((uint16_t)w.count(now, W), true) == 1);
}

// One display category at a time (sonar modes, dashboard TAGGED list).
static void testCategories() {
  det::Entry e{};
  e.kind = det::WIFI;
  CHECK(det::inCat(e, det::CAT_ALL) && det::inCat(e, det::CAT_WIFI));
  CHECK(!det::inCat(e, det::CAT_BLE) && !det::inCat(e, det::CAT_RID) && !det::inCat(e, det::CAT_FLOCK));
  CHECK(!det::isTagged(e));
  e.badges = sig::FLOCK;
  CHECK(det::inCat(e, det::CAT_FLOCK) && !det::inCat(e, det::CAT_TRK) && !det::inCat(e, det::CAT_META));
  CHECK(det::isTagged(e));
  e.kind = det::BLE;
  e.badges = sig::TRK | sig::RANDOM_ADDR;
  CHECK(det::inCat(e, det::CAT_BLE) && det::inCat(e, det::CAT_TRK) && !det::inCat(e, det::CAT_WIFI));
  CHECK(det::isTagged(e));
  e.badges = sig::META;
  CHECK(det::inCat(e, det::CAT_META) && det::isTagged(e));
  e.badges = sig::RANDOM_ADDR;  // RND alone is not a tag
  CHECK(!det::isTagged(e) && !det::inCat(e, det::CAT_TRK));
  e.kind = det::DRONE;
  e.badges = 0;
  CHECK(det::inCat(e, det::CAT_RID) && det::inCat(e, det::CAT_ALL) && !det::inCat(e, det::CAT_WIFI));
  CHECK(!det::isTagged(e));
  CHECK(!det::inCat(e, det::CAT_COUNT));
  // Mode cycling wraps both ways.
  CHECK(det::nextCat(det::CAT_ALL, 1) == det::CAT_WIFI);
  CHECK(det::nextCat(det::CAT_META, 1) == det::CAT_ALL);
  CHECK(det::nextCat(det::CAT_ALL, -1) == det::CAT_META);
  det::Cat c = det::CAT_ALL;
  for (int i = 0; i < det::CAT_COUNT; i++) c = det::nextCat(c, 1);
  CHECK(c == det::CAT_ALL);
}

// Proximity beep cadence: silent below -50 dBm, 1000 -> 150 ms linear to -42.
static void testBeep() {
  CHECK(beep_rules::intervalMs(-90) == 0);
  CHECK(beep_rules::intervalMs(-51) == 0);
  CHECK(beep_rules::intervalMs(-50) == 1000);
  CHECK(beep_rules::intervalMs(-46) == 575);
  CHECK(beep_rules::intervalMs(-42) == 150);
  CHECK(beep_rules::intervalMs(-30) == 150);
  for (int d = -50; d < -42; d++) CHECK(beep_rules::intervalMs(d) > beep_rules::intervalMs(d + 1));
  CHECK(beep_rules::kToneHz == 2500 && beep_rules::kToneMs == 30);
}

static void testSignatures() {
  const uint16_t tile = 0xFEED, samsung = 0xFD5A, none = 0x180F, meta = 0xFD5F;
  CHECK(sig::ble(nullptr, 0, &tile, 1, nullptr) & sig::TRK);
  CHECK(sig::ble(nullptr, 0, &samsung, 1, nullptr) & sig::TRK);
  CHECK(sig::ble(nullptr, 0, &none, 1, nullptr) == 0);
  CHECK(sig::ble(nullptr, 0, &meta, 1, nullptr) & sig::META);
  const uint8_t findMy[] = {0x4C, 0x00, 0x12, 0x19};       // Apple, Offline Finding
  const uint8_t nearby[] = {0x4C, 0x00, 0x10, 0x05};       // Apple, Nearby Info (phones)
  CHECK(sig::ble(findMy, sizeof findMy, nullptr, 0, nullptr) & sig::TRK);
  CHECK((sig::ble(nearby, sizeof nearby, nullptr, 0, nullptr) & sig::TRK) == 0);
  const uint8_t metaMfg[] = {0xAB, 0x01, 0x00};
  CHECK(sig::ble(metaMfg, sizeof metaMfg, nullptr, 0, nullptr) & sig::META);
  const uint8_t flockMfg[] = {0xC8, 0x09, 0x00};
  CHECK(sig::ble(flockMfg, sizeof flockMfg, nullptr, 0, nullptr) & sig::FLOCK);
  CHECK(sig::ble(nullptr, 0, nullptr, 0, "Penguin-1234") & sig::FLOCK);
  CHECK(sig::ble(nullptr, 0, nullptr, 0, "Pixel 8") == 0);
  const uint8_t flockOui[6] = {0xB4, 0x1E, 0x52, 1, 2, 3}, other[6] = {0x3C, 0x22, 0xFB, 1, 2, 3};
  CHECK(sig::wifi(flockOui, nullptr, 0) & sig::FLOCK);
  CHECK(sig::wifi(other, nullptr, 0) == 0);
  const char *ss = "Flock-A1B2C3";
  CHECK(sig::wifi(other, (const uint8_t *)ss, (int)strlen(ss)) & sig::FLOCK);
  const char *home = "HomeNet";
  CHECK(sig::wifi(other, (const uint8_t *)home, (int)strlen(home)) == 0);
}

int main() {
  testBearing();
  testRssi();
  testWindowAndCreature();
  testPet();
  testCategories();
  testBeep();
  testSignatures();
  printf("%d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
