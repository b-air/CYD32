// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "scanner.h"
#include "detections.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <string.h>
#include "boot_log.h"

namespace {
constexpr uint32_t kWifiSliceMs = 12000, kBleSliceMs = 6000, kHopMs = 350;
enum Phase { OFF, WIFI_PH, BLE_PH } phase = OFF;
uint32_t phaseStart = 0, lastHop = 0;
uint8_t channel = 1;
bool stopped = false;
bool wifiOk = false, wifiTried = false, wifiReady = false, bleInitOk = false, bleStartOk = false;
esp_err_t wifiErr = ESP_OK;  // last Wi-Fi bring-up / promiscuous error

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
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  if (!WiFi.mode(WIFI_STA)) { wifiErr = ESP_FAIL; BOOT_LOG("wifi: driver start failed"); return; }
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
}
void startWifi() {
  wifiTried = true;
  // Later slices only toggle promiscuous mode and the channel.
  const esp_err_t e = wifiReady ? esp_wifi_set_promiscuous(true) : wifiErr;
  if (e != wifiErr || !wifiOk) BOOT_LOG("wifi: promiscuous on = %s (%d)", esp_err_to_name(e), (int)e);
  wifiErr = e;
  wifiOk = e == ESP_OK;
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
  initWifi();
  startWifi();
  BOOT_LOG("scanner: wifi=%s (%s) ble init=%d scan=%d", wifiOk ? "ok" : "FAILED", esp_err_to_name(wifiErr),
           (int)bleInitOk, (int)bleStartOk);
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
bool wifiFailed() { return wifiTried && !wifiOk; }
}
