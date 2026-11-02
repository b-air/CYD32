// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "ota_portal.h"
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <string.h>
#include "cyd32_build_gen.h"
#include "fw_status.h"
#include "ota_sig.h"

namespace {
ota_portal::State st;
WebServer *server = nullptr;
uint32_t startedAt = 0, finishedAt = 0;
esp_ota_handle_t handle = 0;
const esp_partition_t *target = nullptr;
bool otaOpen = false, failed = false;
ota_sig::Verifier verifier;
uint8_t hold[ota_sig::kTrailer];
size_t holdLen = 0;

void randomString(char *out, size_t n) {
  static const char cs[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no ambiguous chars
  for (size_t i = 0; i < n; i++) out[i] = cs[esp_random() % (sizeof cs - 1)];
  out[n] = 0;
}
void setMsg(const char *m) { strncpy(st.msg, m, sizeof st.msg - 1); st.msg[sizeof st.msg - 1] = 0; }

void eraseTarget() {
  if (otaOpen) { esp_ota_abort(handle); otaOpen = false; }
  if (target) esp_partition_erase_range(target, 0, target->size);
}
void fail(const char *why) {
  if (failed) return;
  failed = true;
  setMsg(why);
  eraseTarget();
  st.phase = ota_portal::FAILED;
  finishedAt = millis();
}
bool emit(const uint8_t *d, size_t n) {
  if (!n) return true;
  verifier.update(d, n);
  if (esp_ota_write(handle, d, n) != ESP_OK) { fail("flash write error"); return false; }
  return true;
}
// Streams bytes to flash while holding back the last 96 bytes (possible trailer).
void feed(const uint8_t *d, size_t n) {
  if (failed) return;
  st.received += n;
  const size_t total = holdLen + n;
  if (total <= ota_sig::kTrailer) { memcpy(hold + holdLen, d, n); holdLen = total; return; }
  size_t out = total - ota_sig::kTrailer;
  const size_t fromHold = out < holdLen ? out : holdLen;
  if (!emit(hold, fromHold)) return;
  out -= fromHold;
  if (!emit(d, out)) return;
  // new hold = remaining old hold bytes + remaining new bytes (exactly 96)
  uint8_t tmp[ota_sig::kTrailer];
  const size_t keepOld = holdLen - fromHold;
  memcpy(tmp, hold + fromHold, keepOld);
  memcpy(tmp + keepOld, d + out, n - out);
  memcpy(hold, tmp, ota_sig::kTrailer);
  holdLen = ota_sig::kTrailer;
}

const char *kPage1 =
    "<!doctype html><html lang=en><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>CYD32 update</title><style>body{font-family:-apple-system,sans-serif;background:#081020;color:#f8fcf8;"
    "max-width:34em;margin:auto;padding:1em}h1{color:#f8a000}td{padding:2px 8px}.w{color:#f82040;font-weight:bold}"
    "input,button{font-size:1.1em;margin:.5em 0}a{color:#00f8f8}</style><h1>CYD32 &mdash; firmware update</h1><table>";

String esc(const char *s) {
  String o; for (; *s; s++) { if (*s == '<') o += "&lt;"; else if (*s == '>') o += "&gt;"; else if (*s == '&') o += "&amp;"; else if (*s == '"') o += "&quot;"; else o += *s; }
  return o;
}
void handleRoot() {
  String h = kPage1;
  h += "<tr><td>Licence</td><td>" CYD32_LICENSE "</td></tr>";
  h += "<tr><td>Version</td><td>" + esc(CYD32_VERSION) + "</td></tr>";
  h += "<tr><td>Commit</td><td>" + esc(CYD32_COMMIT) + "</td></tr>";
  h += "<tr><td>Source</td><td><a href='" + esc(CYD32_SOURCE_URL) + "'>" + esc(CYD32_SOURCE_URL) + "</a></td></tr>";
  h += "<tr><td>Status</td><td>" + String(fw_status::kindStr()) + "</td></tr>";
  h += "<tr><td>Keys</td><td>" CYD32_KEY0_FPR " / " CYD32_KEY1_FPR "</td></tr></table>";
  if (st.unsignedArmed) h += "<p class=w>UNSIGNED MODE ARMED: one single unsigned update will be accepted.</p>";
  h += "<form method=POST action=/update enctype=multipart/form-data><p>Signed firmware file (.bin):</p>"
       "<input type=file name=fw accept='.bin,application/octet-stream' required><br><button>Upload and install</button></form>"
       "<p>The firmware is verified (ECDSA P-256 signature) before it is activated. Source code: AGPL-3.0, see the link above.</p></html>";
  server->sendHeader("Cache-Control", "no-store");
  server->sendHeader("X-Content-Type-Options", "nosniff");
  server->sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'");
  server->send(200, "text/html; charset=utf-8", h);
}

void handleUploadChunk() {
  HTTPUpload &up = server->upload();
  if (up.status == UPLOAD_FILE_START) {
    if (st.phase == ota_portal::RECEIVING || st.phase == ota_portal::SUCCESS) return;
    failed = false; holdLen = 0; st.received = 0;
    st.expected = server->clientContentLength();
    target = esp_ota_get_next_update_partition(nullptr);
    if (!target) { fail("no OTA partition"); return; }
    if (esp_ota_begin(target, OTA_SIZE_UNKNOWN, &handle) != ESP_OK) { fail("esp_ota_begin failed"); return; }
    otaOpen = true;
    verifier.begin();
    st.phase = ota_portal::RECEIVING;
    setMsg("receiving...");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (st.received == 0 && up.currentSize > 0 && up.buf[0] != 0xE9) { fail("not an ESP32 image"); return; }
    feed(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (failed) return;
    st.phase = ota_portal::VERIFYING;
    if (holdLen < ota_sig::kTrailer) { fail("file too short"); return; }
    int slot = -1;
    bool unsignedImage = false;
    if (!ota_sig::trailerPresent(hold) && st.unsignedArmed) {
      // One-shot unsigned mode (physical gesture): the held bytes belong to the image.
      if (!emit(hold, holdLen)) return;
      unsignedImage = true;
      st.unsignedArmed = false;  // consumed, whatever the outcome
    } else {
      const ota_sig::Result r = verifier.finish(hold, slot);
      if (r != ota_sig::OK) { fail(ota_sig::resultStr(r)); return; }
    }
    // Image structure/checksum validation by ESP-IDF.
    const esp_err_t e = esp_ota_end(handle);
    otaOpen = false;
    if (e != ESP_OK) { fail("invalid image (esp_ota_end)"); return; }
    // Signature verified (or unsigned mode explicitly armed): only now switch the boot slot.
    fw_status::recordNextBoot(target->label, unsignedImage, slot);
    if (esp_ota_set_boot_partition(target) != ESP_OK) { fail("set_boot_partition failed"); return; }
    st.phase = ota_portal::SUCCESS;
    setMsg(unsignedImage ? "installed (UNSIGNED), rebooting" : "signature OK, rebooting");
    finishedAt = millis();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    fail("transfer aborted");
  }
}
void handleUploadDone() {
  const bool ok = st.phase == ota_portal::SUCCESS;
  server->sendHeader("Cache-Control", "no-store");
  server->send(ok ? 200 : 400, "text/html; charset=utf-8",
               String(kPage1) + "</table><h2>" + (ok ? "OK" : "Failed") + "</h2><p>" + esc(st.msg) +
                   "</p><p><a href=/>back</a></p></html>");
}
}  // namespace

namespace ota_portal {
void start(bool armed) {
  memset(&st, 0, sizeof st);
  st.unsignedArmed = armed;
  char sfx[5]; randomString(sfx, 4);
  snprintf(st.ssid, sizeof st.ssid, "CYD32-%s", sfx);
  randomString(st.pass, 12);
  snprintf(st.qr, sizeof st.qr, "WIFI:T:WPA;S:%s;P:%s;;", st.ssid, st.pass);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(st.ssid, st.pass, 1, 0, 1);  // WPA2-PSK, one client
  server = new WebServer(80);
  server->on("/", HTTP_GET, handleRoot);
  server->on("/update", HTTP_POST, handleUploadDone, handleUploadChunk);
  server->onNotFound([] { server->send(404, "text/plain", "404"); });
  server->begin();
  startedAt = millis();
  st.phase = WAITING;
  setMsg("waiting for file");
}
void loop() {
  if (!server) return;
  server->handleClient();
  const uint32_t now = millis();
  const uint32_t el = now - startedAt;
  st.remainingS = el >= kWindowMs ? 0 : (kWindowMs - el) / 1000;
  if (st.phase == SUCCESS && now - finishedAt > 2500) ESP.restart();
  if (st.phase == FAILED && now - finishedAt > 1000) st.phase = WAITING;  // allow another try within the window
  if (el >= kWindowMs && st.phase != RECEIVING && st.phase != VERIFYING) {
    st.phase = TIMEOUT;
    WiFi.softAPdisconnect(true);
    delay(200);
    ESP.restart();
  }
}
const State &state() { return st; }
}
