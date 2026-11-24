// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "scanner.h"
#include "bearing.h"
#include "detections.h"
#include "signatures.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <string.h>
#include "boot_log.h"

namespace {
// Shorter slices than v0.2-test4 (12 s Wi-Fi / 6 s BLE, 350 ms per channel):
// every BLE device is heard at least every 4.5 s instead of every 18 s, and
// the busy channels 1 / 6 / 11 come round every ~0.6 s instead of every
// 4.5 s, so sonar positions follow a walk much faster. The hop table visits
// 1 / 6 / 11 four times per 24 hops (the classic "favour the non-overlapping
// channels" approach of passive scanners) and every other channel once.
constexpr uint32_t kWifiSliceMs = 3000, kBleSliceMs = 1500, kHopMs = 125;
const uint8_t kHop[24] = {1, 6, 11, 2, 1, 6, 11, 3, 4, 1, 6, 11, 5, 7, 1, 6, 11, 8, 9, 10, 12, 13, 6, 11};
uint8_t hopIdx = 0;
enum Phase { OFF, WIFI_PH, BLE_PH } phase = OFF;
uint32_t phaseStart = 0, lastHop = 0;
uint8_t channel = 1;
bool stopped = false;
bool wifiOk = false, wifiTried = false, wifiReady = false, bleInitOk = false, bleStartOk = false;
esp_err_t wifiErr = ESP_OK;  // last Wi-Fi bring-up / promiscuous error (the real code, not ESP_FAIL)
const char *wifiStep = "";   // failing step: "init", "start", "promisc"
// Retry the Wi-Fi bring-up every 30 s while it is not ready (BLE keeps
// scanning in between). v0.2.0-test5 tried once at boot and never again.
constexpr uint32_t kWifiRetryMs = 30000;
uint32_t wifiLastTry = 0;
uint16_t wifiAttempts = 0;
uint32_t heapFreeInt = 0, heapLargestInt = 0, heapLargestDma = 0, heapFreePsram = 0;

void sampleHeap(const char *when) {
  heapFreeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heapLargestInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heapLargestDma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  heapFreePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  BOOT_LOG("heap %s: internal free=%u largest=%u, dma largest=%u, psram free=%u", when, (unsigned)heapFreeInt,
           (unsigned)heapLargestInt, (unsigned)heapLargestDma, (unsigned)heapFreePsram);
}

// WiFi.mode() only returns false: Arduino logs the esp_wifi_init /
// esp_wifi_start code at error level and drops it. Find the step that failed
// and its real esp_err_t, without leaving a half-initialised driver behind
// (a successful probe is undone, so Arduino's own state stays consistent and
// the next WiFi.mode() starts from scratch).
void diagnoseWifiFailure() {
  wifi_mode_t m;
  if (esp_wifi_get_mode(&m) == ESP_ERR_WIFI_NOT_INIT) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Same buffer settings as Arduino's wifiLowLevelInit (dynamic buffers).
    cfg.static_tx_buf_num = 0; cfg.dynamic_tx_buf_num = 32; cfg.tx_buf_type = 1;
    cfg.cache_tx_buf_num = 4; cfg.static_rx_buf_num = 4; cfg.dynamic_rx_buf_num = 32;
    const esp_err_t e = esp_wifi_init(&cfg);
    wifiStep = "init";
    wifiErr = e == ESP_OK ? ESP_FAIL : e;  // ESP_OK here = transient (or the netif step failed)
    if (e == ESP_OK) esp_wifi_deinit();
  } else {
    const esp_err_t e = esp_wifi_start();
    wifiStep = "start";
    wifiErr = e == ESP_OK ? ESP_FAIL : e;
    if (e == ESP_OK) esp_wifi_stop();
  }
  BOOT_LOG("wifi: driver %s failed = %s (%d)", wifiStep, esp_err_to_name(wifiErr), (int)wifiErr);
}

// ASTM F3411 Remote ID over Wi-Fi beacon: vendor IE with OUI FA:0B:BC, type 0x0D.
bool hasRemoteIdIe(const uint8_t *ies, int len) {
  int p = 0;
  while (p + 2 <= len) {
    const uint8_t id = ies[p], l = ies[p + 1];
    if (p + 2 + l > len) break;
    if (id == 221 && l >= 4 && ies[p + 2] == 0xFA && ies[p + 3] == 0x0B && ies[p + 4] == 0xBC && ies[p + 5] == 0x0D) return true;
    p += 2 + l;
  }
  return false;
}


// Classify IEs without storing them. bits match det::Trait.
uint8_t classifyWifiIes(const uint8_t *ies, int len, uint16_t cap, bool ap) {
  uint8_t bits = 0;
  bool hidden = false, hasRSN = false, hasWPA = false, hot = false;
  int p = 0;
  while (p + 2 <= len) {
    const uint8_t id = ies[p], l = ies[p + 1];
    if (p + 2 + l > len) break;
    const uint8_t *d = ies + p + 2;
    if (id == 0 && l == 0) hidden = true;
    else if (id == 48) hasRSN = true;
    else if (id == 107) hot = true;  // Interworking (802.11u / Passpoint)
    else if (id == 221 && l >= 3) {
      if (d[0] == 0x00 && d[1] == 0x50 && d[2] == 0xF2 && l >= 4 && d[3] == 0x01) hasWPA = true;
      if (d[0] == 0x50 && d[1] == 0x6F && d[2] == 0x9A) hot = true;  // Wi-Fi Alliance HS
      if (d[0] == 0x00 && d[1] == 0x17 && d[2] == 0xF2) hot = true;  // Apple vendor AP
    } else if (id == 255 && l >= 1) {
      const uint8_t ext = d[0];
      if (ext == 35 || ext == 36) bits |= det::traitBit(det::WIFI6);
      if (ext == 106 || ext == 107 || ext == 108) bits |= det::traitBit(det::WIFI7);
    }
    p += 2 + l;
  }
  if (ap) {
    bits |= det::traitBit(det::AP);
    if (hot || hidden) bits |= det::traitBit(det::HOT);
    if (!(cap & 0x0010) && !hasRSN && !hasWPA) bits |= det::traitBit(det::OPEN);
  }
  return bits;
}

uint8_t bandBit(uint8_t ch) {
  if (ch >= 1 && ch <= 14) return det::traitBit(det::G24);
  if (ch >= 32) return det::traitBit(det::G5);
  return 0;
}

void promiscCb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;  // DATA / CTRL / MISC frames are dropped
  const auto *pkt = static_cast<const wifi_promiscuous_pkt_t *>(buf);
  const int len = pkt->rx_ctrl.sig_len;
  if (len < 24) return;
  const uint8_t *f = pkt->payload;
  const uint8_t subtype = (f[0] >> 4) & 0x0F;
  const int8_t rssi = pkt->rx_ctrl.rssi;
  const uint8_t ch = pkt->rx_ctrl.channel;
  const uint8_t band = bandBit(ch);
  if (subtype == 8 || subtype == 5) {          // beacon / probe response: transmitter = addr2
    const int ieLen = len > 40 ? len - 36 - 4 : 0;
    const bool rid = subtype == 8 && ieLen > 0 && hasRemoteIdIe(f + 36, ieLen);
    uint16_t cap = 0;
    if (len >= 36) cap = (uint16_t)(f[34] | (f[35] << 8));
    uint8_t traits = band;
    if (!rid && ieLen > 0) traits |= classifyWifiIes(f + 36, ieLen, cap, true);
    // SSID (IE 0) is only matched against the badge patterns, never stored.
    const uint8_t *ssid = nullptr;
    int ssidLen = 0;
    if (ieLen >= 2 && f[36] == 0 && f[37] <= 32 && 2 + f[37] <= ieLen) { ssid = f + 38; ssidLen = f[37]; }
    uint8_t badges = sig::wifi(f + 10, ssid, ssidLen);
    if (bearing::isRandomized(f + 10)) badges |= sig::RANDOM_ADDR;
    det::report(rid ? det::DRONE : det::WIFI, f + 10, rssi, ch, traits, badges);
  } else if (subtype == 4) {                   // probe request (client device)
    const int ieLen = len > 28 ? len - 24 - 4 : 0;
    uint8_t traits = band;
    if (ieLen > 0) traits |= classifyWifiIes(f + 24, ieLen, 0, false);
    uint8_t badges = sig::wifi(f + 10, nullptr, 0);
    if (bearing::isRandomized(f + 10)) badges |= sig::RANDOM_ADDR;  // probe MAC randomization
    det::report(det::WIFI, f + 10, rssi, ch, traits, badges);
  }
  // Frame contents (SSID, payload) are not kept: only classification bits are stored.
}

// Badge bits from the advertisement (signatures.h). Nothing is stored.
uint8_t classifyBle(NimBLEAdvertisedDevice *d) {
  uint16_t uu[8];
  int nu = 0;
  auto add = [&](const NimBLEUUID &u) {
    if (nu < 8 && u.bitSize() == 16) uu[nu++] = u.getNative()->u16.value;
  };
  for (uint8_t i = 0; i < d->getServiceUUIDCount() && nu < 8; i++) add(d->getServiceUUID(i));
  for (uint8_t i = 0; i < d->getServiceDataCount() && nu < 8; i++) add(d->getServiceDataUUID(i));
  std::string md, name;
  if (d->haveManufacturerData()) md = d->getManufacturerData();
  if (d->haveName()) name = d->getName();
  return sig::ble(reinterpret_cast<const uint8_t *>(md.data()), md.size(), uu, nu, name.empty() ? nullptr : name.c_str());
}

class BleCb : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *d) override {
    // NimBLE keeps the address little-endian: reverse it to display order so
    // the indicative bearing matches the printed address (bearing.h).
    const uint8_t *n = d->getAddress().getNative();
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) mac[i] = n[5 - i];
    // ASTM F3411 Remote ID over BLE: service data UUID 0xFFFA.
    const bool rid = d->haveServiceData() && d->getServiceDataUUID().equals(NimBLEUUID((uint16_t)0xFFFA));
    uint8_t badges = rid ? 0 : classifyBle(d);
    // Random address that is not "static random" (top bits 11): a resolvable
    // or non-resolvable private address, which rotates.
    if (d->getAddressType() == BLE_ADDR_RANDOM && (mac[0] >> 6) != 3) badges |= sig::RANDOM_ADDR;
    const uint8_t traits = (badges & sig::TRK) ? det::traitBit(det::TRK) : 0;
    det::report(rid ? det::DRONE : det::BLE, mac, d->getRSSI(), 0, traits, badges);
  }
};
BleCb bleCb;

// Bring the Wi-Fi driver up ONCE, receive-only. WiFi.mode(WIFI_MODE_NULL) on a
// driver that was never started is a no-op in Arduino-ESP32 (getMode() is
// already NULL), so promiscuous mode used to fail and Wi-Fi never ran.
// Nothing here transmits:
//  - persistent(false): driver config kept in RAM, never written to NVS;
//  - setAutoReconnect(false): Arduino's STA_DISCONNECTED handler never calls
//    WiFi.begin() (and we never call begin/connect/scanNetworks);
//  - WiFi.mode(WIFI_STA) only runs esp_wifi_init/set_mode/start: an idle,
//    unassociated station does not scan, probe or associate on its own;
//  - the RAM STA config is cleared directly (no esp_wifi_disconnect(), which
//    could raise a disconnect event);
//  - esp_wifi_set_mode(WIFI_MODE_NULL) then drops the station interface while
//    the driver stays started, which is all promiscuous mode needs. If the
//    driver refuses promiscuous mode in NULL mode, we fall back to the idle
//    station, which is just as silent.
void initWifi() {
  wifiLastTry = millis();
  wifiAttempts++;
  wifiReady = false;
  sampleHeap(wifiAttempts == 1 ? "before wifi init" : "before wifi retry");
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  if (!WiFi.mode(WIFI_STA)) {
    diagnoseWifiFailure();
    sampleHeap("after wifi failure");
    return;
  }
  wifiStep = "";
  wifi_config_t blank = {};
  esp_err_t e = esp_wifi_set_config(WIFI_IF_STA, &blank);
  BOOT_LOG("wifi: clear STA config = %s (%d)", esp_err_to_name(e), (int)e);
  wifi_promiscuous_filter_t filt = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  e = esp_wifi_set_promiscuous_filter(&filt);
  BOOT_LOG("wifi: mgmt-only filter = %s (%d)", esp_err_to_name(e), (int)e);
  esp_wifi_set_promiscuous_rx_cb(&promiscCb);
  e = esp_wifi_set_mode(WIFI_MODE_NULL);
  BOOT_LOG("wifi: set_mode(NULL) = %s (%d)", esp_err_to_name(e), (int)e);
  e = esp_wifi_set_promiscuous(true);
  BOOT_LOG("wifi: promiscuous probe in NULL mode = %s (%d)", esp_err_to_name(e), (int)e);
  if (e != ESP_OK) {
    const esp_err_t m = esp_wifi_set_mode(WIFI_MODE_STA);  // idle station fallback
    e = esp_wifi_set_promiscuous(true);
    BOOT_LOG("wifi: fallback idle STA: set_mode = %s, promiscuous = %s (%d)", esp_err_to_name(m), esp_err_to_name(e), (int)e);
  }
  esp_wifi_set_promiscuous(false);
  wifiReady = e == ESP_OK;
  wifiErr = e;
  if (!wifiReady) wifiStep = "promisc";
  BOOT_LOG("wifi: bring-up #%u %s", (unsigned)wifiAttempts, wifiReady ? "ok" : "FAILED");
}
void startWifi() {
  wifiTried = true;
  // Later slices only toggle promiscuous mode and the channel.
  const esp_err_t e = wifiReady ? esp_wifi_set_promiscuous(true) : wifiErr;
  if (e != wifiErr || !wifiOk) BOOT_LOG("wifi: promiscuous on = %s (%d)", esp_err_to_name(e), (int)e);
  wifiErr = e;
  wifiOk = e == ESP_OK;
  if (wifiReady && !wifiOk) wifiStep = "promisc";
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  phase = WIFI_PH; phaseStart = lastHop = millis();
}
void stopWifi() { esp_wifi_set_promiscuous(false); }
void startBle() {
  NimBLEScan *s = NimBLEDevice::getScan();
  s->setAdvertisedDeviceCallbacks(&bleCb, true);
  s->setActiveScan(false);  // passive: no SCAN_REQ is ever sent
  s->setInterval(100); s->setWindow(100);  // 100 % duty cycle during the short BLE slice
  bleStartOk = s->start(0, nullptr, false) || bleStartOk;
  phase = BLE_PH; phaseStart = millis();
}
void stopBle() { NimBLEDevice::getScan()->stop(); NimBLEDevice::getScan()->clearResults(); }
}  // namespace

namespace scanner {
void begin() {
  // Wi-Fi first: its driver needs large contiguous internal RAM blocks, so it
  // is brought up before NimBLE takes its share (v0.2.0-test5 initialised
  // NimBLE first). Then exercise BLE once so the self-test covers both radios.
  initWifi();
  NimBLEDevice::init("");
  bleInitOk = NimBLEDevice::getInitialized();
  startBle();
  stopBle();
  if (wifiReady) startWifi();
  else { wifiTried = true; startBle(); }  // BLE only until a retry succeeds
  BOOT_LOG("scanner: wifi=%s (%s, step %s) ble init=%d scan=%d", wifiOk ? "ok" : "FAILED", esp_err_to_name(wifiErr),
           wifiStep[0] ? wifiStep : "-", (int)bleInitOk, (int)bleStartOk);
}
void loop() {
  if (stopped) return;
  const uint32_t now = millis();
  if (!wifiReady && now - wifiLastTry >= kWifiRetryMs) {
    if (phase == BLE_PH) stopBle();
    initWifi();
    if (wifiReady) { startWifi(); return; }
    startBle();
  }
  if (phase == WIFI_PH) {
    if (now - lastHop >= kHopMs) {
      hopIdx = (uint8_t)((hopIdx + 1) % sizeof kHop);
      channel = kHop[hopIdx];
      esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
      lastHop = now;
    }
    if (now - phaseStart >= kWifiSliceMs) { stopWifi(); startBle(); }
  } else if (phase == BLE_PH && now - phaseStart >= kBleSliceMs) {
    if (wifiReady) { stopBle(); startWifi(); }
    else phaseStart = now;  // no Wi-Fi: the continuous BLE scan simply goes on
  }
}
void stop() {
  stopped = true;
  if (phase == WIFI_PH) stopWifi();
  if (phase == BLE_PH) stopBle();
  NimBLEDevice::deinit(true);
  phase = OFF;
}
bool wifiActive() { return phase == WIFI_PH; }
bool healthy() { return wifiOk && bleInitOk && bleStartOk; }
bool wifiFailed() { return wifiTried && (!wifiReady || !wifiOk); }
WifiDiag wifiDiag() {
  WifiDiag d;
  d.ready = wifiReady && wifiOk;
  d.err = (int)wifiErr;
  d.errName = wifiErr == ESP_OK ? "ESP_OK" : esp_err_to_name(wifiErr);
  d.step = wifiStep;
  d.attempts = wifiAttempts;
  const uint32_t since = millis() - wifiLastTry;
  d.retryInMs = (wifiReady || stopped) ? 0 : (since >= kWifiRetryMs ? 0 : kWifiRetryMs - since);
  d.freeInt = heapFreeInt; d.largestInt = heapLargestInt; d.largestDma = heapLargestDma; d.freePsram = heapFreePsram;
  return d;
}
}
