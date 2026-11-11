// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "scanner.h"
#include "detections.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <string.h>

namespace {
constexpr uint32_t kWifiSliceMs = 12000, kBleSliceMs = 6000, kHopMs = 350;
enum Phase { OFF, WIFI_PH, BLE_PH } phase = OFF;
uint32_t phaseStart = 0, lastHop = 0;
uint8_t channel = 1;
bool stopped = false;
bool wifiOk = false, bleInitOk = false, bleStartOk = false;

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
    det::report(rid ? det::DRONE : det::WIFI, f + 10, rssi, ch, traits);
  } else if (subtype == 4) {                   // probe request (client device)
    const int ieLen = len > 28 ? len - 24 - 4 : 0;
    uint8_t traits = band;
    if (ieLen > 0) traits |= classifyWifiIes(f + 24, ieLen, 0, false);
    det::report(det::WIFI, f + 10, rssi, ch, traits);
  }
  // Frame contents (SSID, payload) are not kept: only classification bits are stored.
}

uint8_t classifyBle(NimBLEAdvertisedDevice *d) {
  uint8_t bits = 0;
  if (d->haveManufacturerData()) {
    const std::string md = d->getManufacturerData();
    const size_t n = md.size();
    const auto *p = reinterpret_cast<const uint8_t *>(md.data());
    if (n >= 3 && p[0] == 0x4C && p[1] == 0x00) {
      const uint8_t typ = p[2];
      if (typ == 0x12 || typ == 0x10) bits |= det::traitBit(det::TRK);
    }
  }
  if (d->haveServiceData() && d->getServiceDataUUID().equals(NimBLEUUID((uint16_t)0xFD5A)))
    bits |= det::traitBit(det::TRK);
  if (d->isAdvertisingService(NimBLEUUID((uint16_t)0xFEED)) ||
      d->isAdvertisingService(NimBLEUUID((uint16_t)0xFDAE)))
    bits |= det::traitBit(det::TRK);
  return bits;
}

class BleCb : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *d) override {
    uint8_t mac[6];
    memcpy(mac, d->getAddress().getNative(), 6);
    // ASTM F3411 Remote ID over BLE: service data UUID 0xFFFA.
    const bool rid = d->haveServiceData() && d->getServiceDataUUID().equals(NimBLEUUID((uint16_t)0xFFFA));
    det::report(rid ? det::DRONE : det::BLE, mac, d->getRSSI(), 0, rid ? 0 : classifyBle(d));
  }
};
BleCb bleCb;

void startWifi() {
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  wifi_promiscuous_filter_t filt = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&promiscCb);
  wifiOk = esp_wifi_set_promiscuous(true) == ESP_OK;
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  phase = WIFI_PH; phaseStart = lastHop = millis();
}
void stopWifi() { esp_wifi_set_promiscuous(false); }
void startBle() {
  NimBLEScan *s = NimBLEDevice::getScan();
  s->setAdvertisedDeviceCallbacks(&bleCb, true);
  s->setActiveScan(false);  // passive: no SCAN_REQ is ever sent
  s->setInterval(100); s->setWindow(90);
  bleStartOk = s->start(0, nullptr, false) || bleStartOk;
  phase = BLE_PH; phaseStart = millis();
}
void stopBle() { NimBLEDevice::getScan()->stop(); NimBLEDevice::getScan()->clearResults(); }
}  // namespace

namespace scanner {
void begin() {
  NimBLEDevice::init("");
  bleInitOk = NimBLEDevice::getInitialized();
  // Exercise both radios once at boot so the self-test covers them.
  startBle();
  stopBle();
  startWifi();
}
void loop() {
  if (stopped) return;
  const uint32_t now = millis();
  if (phase == WIFI_PH) {
    if (now - lastHop >= kHopMs) {
      channel = channel % 13 + 1;
      esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
      lastHop = now;
    }
    if (now - phaseStart >= kWifiSliceMs) { stopWifi(); startBle(); }
  } else if (phase == BLE_PH && now - phaseStart >= kBleSliceMs) {
    stopBle(); startWifi();
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
}
