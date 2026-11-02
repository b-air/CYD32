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


void promiscCb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;  // DATA / CTRL / MISC frames are dropped
  const auto *pkt = static_cast<const wifi_promiscuous_pkt_t *>(buf);
  const int len = pkt->rx_ctrl.sig_len;
  if (len < 24) return;
  const uint8_t *f = pkt->payload;
  const uint8_t subtype = (f[0] >> 4) & 0x0F;
  const int8_t rssi = pkt->rx_ctrl.rssi;
  const uint8_t ch = pkt->rx_ctrl.channel;
  if (subtype == 8 || subtype == 5) {          // beacon / probe response: transmitter = addr2
    const bool rid = subtype == 8 && len > 36 && hasRemoteIdIe(f + 36, len - 36 - 4);
    det::report(rid ? det::DRONE : det::WIFI, f + 10, rssi, ch);
  } else if (subtype == 4) {                   // probe request (client device)
    det::report(det::WIFI, f + 10, rssi, ch);
  }
  // Frame contents (SSID, payload) are not kept.
}

class BleCb : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *d) override {
    uint8_t mac[6];
    memcpy(mac, d->getAddress().getNative(), 6);
    // ASTM F3411 Remote ID over BLE: service data UUID 0xFFFA.
    const bool rid = d->haveServiceData() && d->getServiceDataUUID().equals(NimBLEUUID((uint16_t)0xFFFA));
    det::report(rid ? det::DRONE : det::BLE, mac, d->getRSSI(), 0);
  }
};
BleCb bleCb;

void startWifi() {
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  wifi_promiscuous_filter_t filt = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&promiscCb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  phase = WIFI_PH; phaseStart = lastHop = millis();
}
void stopWifi() { esp_wifi_set_promiscuous(false); }
void startBle() {
  NimBLEScan *s = NimBLEDevice::getScan();
  s->setAdvertisedDeviceCallbacks(&bleCb, true);
  s->setActiveScan(false);  // passive: no SCAN_REQ is ever sent
  s->setInterval(100); s->setWindow(90);
  s->start(0, nullptr, false);
  phase = BLE_PH; phaseStart = millis();
}
void stopBle() { NimBLEDevice::getScan()->stop(); NimBLEDevice::getScan()->clearResults(); }
}  // namespace

namespace scanner {
void begin() {
  NimBLEDevice::init("");
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
}
