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
#include "fw_version.h"
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
char token[33];               // per-session CSRF token (16 random bytes, hex)
bool pendingUnsigned = false; // state kept while a downgrade awaits confirmation
int pendingSlot = -1;
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
bool ctEqual(const String &a, const char *b) {  // constant-time for equal lengths
  const size_t n = strlen(b);
  if (a.length() != n) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) d |= (uint8_t)a[i] ^ (uint8_t)b[i];
  return d == 0;
}
bool hostOk() {  // DNS-rebinding defence: only the literal AP address is accepted
  const String h = server->hostHeader();
  return h == "192.168.4.1" || h == "192.168.4.1:80";
}
void handleRoot() {
  if (!hostOk()) { server->send(421, "text/plain", "bad host"); return; }
  String h = kPage1;
  h += "<tr><td>Licence</td><td>" CYD32_LICENSE "</td></tr>";
  h += "<tr><td>Version</td><td>" + esc(CYD32_VERSION) + "</td></tr>";
  h += "<tr><td>Commit</td><td>" + esc(CYD32_COMMIT) + "</td></tr>";
  h += "<tr><td>Source</td><td><a href='" + esc(CYD32_SOURCE_URL) + "'>" + esc(CYD32_SOURCE_URL) + "</a></td></tr>";
  h += "<tr><td>Status</td><td>" + String(fw_status::kindStr()) + "</td></tr>";
  h += "<tr><td>Keys</td><td>" CYD32_KEY0_FPR " / " CYD32_KEY1_FPR "</td></tr></table>";
  if (st.phase == ota_portal::AWAIT_CONFIRM) h += "<p class=w>DOWNGRADE: confirm or cancel on the device screen.</p>";
  if (st.unsignedArmed) h += "<p class=w>UNSIGNED MODE ARMED: one single unsigned update will be accepted.</p>";
  h += "<form method=POST action='/update?t=" + String(token) + "' enctype=multipart/form-data><p>Signed firmware file (.bin):</p>"
       "<input type=file name=fw accept='.bin,application/octet-stream' required><br><button>Upload and install</button></form>"
       "<p>The firmware is verified (ECDSA P-256 signature) before it is activated. Source code: AGPL-3.0, see the link above.</p></html>";
  server->sendHeader("Cache-Control", "no-store");
  server->sendHeader("X-Content-Type-Options", "nosniff");
  server->sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'");
  server->send(200, "text/html; charset=utf-8", h);
}

uint32_t confirmStart = 0;
// True only for the upload whose START passed the Host/token checks while WAITING.
// Unauthorized uploads are ignored silently so they cannot fail() a pending image.
bool uploadAuthorized = false;
// Authorization result of the current request, read by handleUploadDone.
bool lastUploadAuthorized = false;
// Searches the freshly written slot for the fw_version marker.
bool findMarker(fw_version::Marker &out) {
  static uint8_t buf[1024 + sizeof(fw_version::Marker)];
  const size_t ml = sizeof(fw_version::Marker);
  const size_t len = st.received < target->size ? st.received : target->size;
  for (size_t off = 0; off < len; off += 1024) {
    const size_t n = (off + sizeof buf <= target->size) ? sizeof buf : target->size - off;
    if (esp_partition_read(target, off, buf, n) != ESP_OK) return false;
    for (size_t i = 0; i + ml <= n; i++)
      if (buf[i] == 'C' && memcmp(buf + i, fw_version::kMagic, 16) == 0) { memcpy(&out, buf + i, ml); return true; }
  }
  return false;
}
void activate() {
  // Signature verified (or unsigned mode explicitly armed), version accepted: only now switch the boot slot.
  fw_status::recordNextBoot(target, pendingUnsigned, pendingSlot);
  if (esp_ota_set_boot_partition(target) != ESP_OK) { fail("set_boot_partition failed"); return; }
  st.phase = ota_portal::SUCCESS;
  setMsg(pendingUnsigned ? "installed (UNSIGNED), rebooting" : "signature OK, rebooting");
  finishedAt = millis();
}
void handleUploadChunk() {
  HTTPUpload &up = server->upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadAuthorized = false;
    lastUploadAuthorized = false;
    if (st.phase != ota_portal::WAITING) return;
    failed = false; holdLen = 0; st.received = 0; target = nullptr;
    if (!hostOk()) { fail("rejected: bad Host header"); return; }
    if (!ctEqual(server->arg("t"), token)) { fail("rejected: bad session token"); return; }
    uploadAuthorized = true;
    lastUploadAuthorized = true;
    st.expected = server->clientContentLength();
    target = esp_ota_get_next_update_partition(nullptr);
    if (!target) { fail("no OTA partition"); return; }
    // multipart overhead allowance: 2 KB
    if (st.expected > target->size + ota_sig::kTrailer + 2048) { target = nullptr; fail("file larger than the OTA slot"); return; }
    if (esp_ota_begin(target, OTA_SIZE_UNKNOWN, &handle) != ESP_OK) { fail("esp_ota_begin failed"); return; }
    otaOpen = true;
    verifier.begin();
    st.phase = ota_portal::RECEIVING;
    setMsg("receiving...");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!uploadAuthorized) return;
    if (st.received == 0 && up.currentSize > 0 && up.buf[0] != 0xE9) { fail("not an ESP32 image"); return; }
    if (failed) return;
    if (st.received + up.currentSize > target->size + ota_sig::kTrailer) { fail("file larger than the OTA slot"); return; }
    feed(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (!uploadAuthorized) return;
    uploadAuthorized = false;
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
    pendingUnsigned = unsignedImage; pendingSlot = slot;
    // Anti-downgrade: compare the version marker of the new image with ours.
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(target, &desc) != ESP_OK) { fail("no app descriptor"); return; }
    fw_version::Marker nv;
    const bool found = findMarker(nv);
    if (!found || fw_version::compare(nv, fw_version::running) < 0) {
      snprintf(st.msg, sizeof st.msg, found ? "DOWNGRADE to %u.%u.%u: confirm on screen" : "DOWNGRADE (unknown version): confirm on screen",
               nv.major, nv.minor, nv.patch);
      st.phase = ota_portal::AWAIT_CONFIRM;
      confirmStart = millis();
      return;
    }
    activate();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (!uploadAuthorized) return;
    uploadAuthorized = false;
    fail("transfer aborted");
  }
}
void handleUploadDone() {
  const bool authorized = lastUploadAuthorized;
  lastUploadAuthorized = false;  // never carried over to a later request
  if (!authorized) {
    server->sendHeader("Cache-Control", "no-store");
    server->send(403, "text/plain", "");
    return;
  }
  const bool ok = st.phase == ota_portal::SUCCESS || st.phase == ota_portal::AWAIT_CONFIRM;
  server->sendHeader("Cache-Control", "no-store");
  server->send(ok ? 200 : 400, "text/html; charset=utf-8",
               String(kPage1) + "</table><h2>" + (ok ? "OK" : "Failed") + "</h2><p>" + esc(st.msg) +
                   "</p><p><a href=/>back</a></p></html>");
}
}  // namespace

namespace ota_portal {
void confirmDowngrade(bool yes) {
  if (st.phase != AWAIT_CONFIRM) return;
  if (yes) activate();
  else { failed = false; fail("downgrade cancelled"); }
}
void start(bool armed) {
  memset(&st, 0, sizeof st);
  st.unsignedArmed = armed;
  char sfx[5]; randomString(sfx, 4);
  snprintf(st.ssid, sizeof st.ssid, "CYD32-%s", sfx);
  randomString(st.pass, 12);
  uint8_t tb[16]; esp_fill_random(tb, sizeof tb);
  for (int i = 0; i < 16; i++) snprintf(token + 2 * i, 3, "%02x", tb[i]);
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
  if (st.phase == FAILED && now - finishedAt > 1000) { st.phase = WAITING; failed = false; }
  if (st.phase == AWAIT_CONFIRM && now - confirmStart > 120000) { failed = false; fail("downgrade not confirmed"); }  // allow another try within the window
  if (el >= kWindowMs && st.phase != RECEIVING && st.phase != VERIFYING && st.phase != AWAIT_CONFIRM) {
    st.phase = TIMEOUT;
    WiFi.softAPdisconnect(true);
    delay(200);
    ESP.restart();
  }
}
const State &state() { return st; }
}
