// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Dry-run coverage for AWUS detect / arm / restore, the management filter and
// the passive BLE command bytes. No dongle and no transmit.
#include "radio.hpp"
#include "mbedtls/sha256.h"
#include "privacy.h"
#include "detections.h"
#include <errno.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
namespace fs = std::filesystem;

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " << #c << "\n"; } } while (0)

namespace {
struct Rec : cz::Exec {
  std::vector<std::vector<std::string>> ran;
  int fail_at = -1;
  int run(const std::vector<std::string> &argv, std::string &out, std::string &err) override {
    out.clear();
    ran.push_back(argv);
    if ((int)ran.size() - 1 == fail_at) { err = "boom"; return 1; }
    err.clear();
    return 0;
  }
};

void wr(const fs::path &p, const std::string &s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << s;
}
void link_to(const fs::path &link, const fs::path &target) {
  fs::create_directories(link.parent_path());
  std::error_code ec;
  fs::remove(link, ec);
  fs::create_symlink(target, link);
}

fs::path make_sys(const fs::path &root) {
  fs::remove_all(root);
  link_to(root / "class/net/wlan0/device", fs::path("../../../dev/sdio0"));
  wr(root / "dev/sdio0/driver-target", "");
  link_to(root / "dev/sdio0/driver", fs::path("../../bus/sdio/drivers/brcmfmac"));
  wr(root / "bus/sdio/drivers/brcmfmac/.keep", "");
  link_to(root / "class/net/wlan1/device", fs::path("../../../dev/usb/1-1/1-1:1.0"));
  wr(root / "dev/usb/1-1/idVendor", "0e8d\n");
  wr(root / "dev/usb/1-1/idProduct", "7961\n");
  link_to(root / "dev/usb/1-1/1-1:1.0/driver", fs::path("../../../../bus/usb/drivers/mt7921u"));
  wr(root / "bus/usb/drivers/mt7921u/.keep", "");
  wr(root / "module/mt7921u/taint", "");
  link_to(root / "class/bluetooth/hci0/device", fs::path("../../../dev/sdio0"));
  link_to(root / "class/bluetooth/hci1/device", fs::path("../../../dev/usb/1-1"));
  return root;
}

void put(std::vector<uint8_t> &b, uint8_t v) { b.push_back(v); }
void put16(std::vector<uint8_t> &b, uint16_t v) { b.push_back(v & 0xff); b.push_back(v >> 8); }
void put32(std::vector<uint8_t> &b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xff);
}

std::vector<uint8_t> radiotap(uint32_t present, uint16_t freq, int8_t rssi, bool ext) {
  std::vector<uint8_t> b;
  put(b, 0); put(b, 0);
  put16(b, 0);  // length patched later
  uint32_t word = present;
  if (ext) word |= 0x80000000u;
  put32(b, word);
  if (ext) put32(b, 0);
  auto align = [&](size_t a) { while (b.size() & (a - 1)) b.push_back(0); };
  if (present & (1u << 3)) { align(2); put16(b, freq); put16(b, 0); }
  if (present & (1u << 5)) put(b, (uint8_t)rssi);
  b[2] = (uint8_t)(b.size() & 0xff);
  b[3] = (uint8_t)(b.size() >> 8);
  return b;
}

void mac(std::vector<uint8_t> &f, const uint8_t a[6]) { f.insert(f.end(), a, a + 6); }

std::vector<uint8_t> beacon(const uint8_t addr[6], const std::vector<uint8_t> &ies, uint16_t cap, bool fcs) {
  std::vector<uint8_t> f;
  put(f, 0x80); put(f, 0x00); put16(f, 0);
  const uint8_t bc[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  mac(f, bc); mac(f, addr); mac(f, addr);
  put16(f, 0);
  for (int i = 0; i < 8; i++) put(f, 0);
  put16(f, 100);
  put16(f, cap);
  f.insert(f.end(), ies.begin(), ies.end());
  if (fcs) { put(f, 0); put(f, 0); put(f, 0); put(f, 0); }
  return f;
}
}  // namespace

static void test_sha() {
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  CHECK(mbedtls_sha256_starts_ret(&c, 0) == 0);
  const char *abc = "abc";
  CHECK(mbedtls_sha256_update_ret(&c, (const uint8_t *)abc, 3) == 0);
  uint8_t out[32];
  CHECK(mbedtls_sha256_finish_ret(&c, out) == 0);
  const uint8_t exp[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                           0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  CHECK(memcmp(out, exp, 32) == 0);
  mbedtls_sha256_free(&c);
}

static void test_detect(const fs::path &sys) {
  const auto d = cz::detect_ifaces(sys.string());
  CHECK(d.chosen == "wlan1");
  bool saw_internal = false;
  for (const auto &iface : d.ifaces) {
    if (iface.name == "wlan0") { saw_internal = iface.internal && iface.driver == "brcmfmac"; }
    if (iface.name == "wlan1") {
      CHECK(iface.driver == "mt7921u");
      CHECK(iface.usb_id == "0e8d:7961");
      CHECK(iface.in_kernel);
      CHECK(!iface.out_of_tree);
    }
  }
  CHECK(saw_internal);
  std::string why;
  CHECK(!cz::arm_allowed(d, "wlan0", why));
  CHECK(cz::arm_allowed(d, "wlan1", why));
  CHECK(cz::find_internal_hci(sys.string()) == "hci0");
}

static bool only_tools(const std::vector<std::string> &argv) {
  if (argv.empty()) return false;
  if (argv[0] != "/usr/bin/nmcli" && argv[0] != "/usr/sbin/ip" && argv[0] != "/usr/sbin/iw") return false;
  for (const auto &a : argv)
    if (a.find("kill") != std::string::npos) return false;
  return true;
}

static void test_arm() {
  const uint8_t mac[6] = {0};
  (void)mac;
  cz::SavedLink s{"wlan1", "managed", true};
  const auto arm = cz::plan_arm(s);
  CHECK(arm.size() == 4);
  CHECK(arm[0].argv[0] == "/usr/bin/nmcli" && arm[0].argv[5] == "no");
  CHECK(arm[2].argv[0] == "/usr/sbin/iw" && arm[2].argv[5] == "monitor");
  const auto back = cz::plan_restore(s);
  CHECK(back.size() == 4);
  CHECK(back[1].argv[5] == "managed");
  CHECK(back[3].argv[0] == "/usr/bin/nmcli" && back[3].argv[5] == "yes");
  for (const auto &c : arm) CHECK(only_tools(c.argv));
  for (const auto &c : back) CHECK(only_tools(c.argv));

  fs::path sys = fs::temp_directory_path() / "cyd32-awus-sys";
  make_sys(sys);
  const auto d = cz::detect_ifaces(sys.string());
  Rec ok;
  cz::Session session(ok);
  std::string err;
  CHECK(session.arm(d, s, err));
  CHECK(session.armed());
  CHECK(ok.ran.size() == 4);
  CHECK(session.hop(2412, err));
  CHECK(ok.ran.back()[0] == "/usr/sbin/iw");
  CHECK(ok.ran.back()[5] == "2412");
  CHECK(session.restore(err));
  CHECK(!session.armed());

  Rec bad;
  bad.fail_at = 2;  // iw set type monitor fails
  cz::Session boom(bad);
  CHECK(!boom.arm(d, s, err));
  CHECK(!boom.armed());
  // arm commands 0,1 then the failed 2, then the full restore plan.
  CHECK(bad.ran.size() == 3 + 4);
  CHECK(bad.ran[2][5] == "monitor");
  CHECK(bad.ran[4][5] == "managed");  // restore sets the original type
  bool nm_yes = false;
  for (const auto &c : bad.ran)
    if (!c.empty() && c[0] == "/usr/bin/nmcli" && c.back() == "yes") nm_yes = true;
  CHECK(nm_yes);
}

static void test_frames() {
  const uint8_t addr[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  const uint8_t flock[6] = {0xB4, 0x1E, 0x52, 0x00, 0x00, 0x10};
  std::vector<uint8_t> ies = {0, 4, 't', 'e', 's', 't'};
  auto rt = radiotap((1u << 3) | (1u << 5), 2412, (int8_t)-40, false);
  auto fr = beacon(addr, ies, 0x0001, false);
  rt.insert(rt.end(), fr.begin(), fr.end());
  cz::RxFrame rx;
  CHECK(cz::parse_monitor_frame(rt.data(), rt.size(), rx));
  CHECK(rx.accepted);
  CHECK(rx.kind == det::WIFI);
  CHECK(rx.rssi == -40);
  CHECK(rx.mhz == 2412);
  CHECK(rx.channel == 1);
  CHECK(rx.traits & det::traitBit(det::G24));
  CHECK(rx.traits & det::traitBit(det::AP));
  CHECK(rx.traits & det::traitBit(det::OPEN));
  CHECK(rx.badges & sig::RANDOM_ADDR);

  // Extended present word still yields the same RSSI and frequency.
  auto rt2 = radiotap((1u << 3) | (1u << 5), 2412, (int8_t)-40, true);
  rt2.insert(rt2.end(), fr.begin(), fr.end());
  cz::RxFrame rx2;
  CHECK(cz::parse_monitor_frame(rt2.data(), rt2.size(), rx2) && rx2.accepted && rx2.rssi == -40 && rx2.mhz == 2412);

  // Data frame is not accepted.
  auto data = rt;
  data[rt.size() - fr.size()] = 0x08;
  cz::RxFrame dropped;
  CHECK(cz::parse_monitor_frame(data.data(), data.size(), dropped));
  CHECK(!dropped.accepted);

  // Remote ID vendor IE -> drone. 6 GHz does not set the 2.4 trait.
  std::vector<uint8_t> rid = ies;
  const uint8_t vie[] = {221, 4, 0xFA, 0x0B, 0xBC, 0x0D};
  rid.insert(rid.end(), vie, vie + sizeof vie);
  auto rt6 = radiotap((1u << 3) | (1u << 5), 5955, (int8_t)-55, false);
  auto fr6 = beacon(flock, rid, 0x0011, true);
  // FCS flag in radiotap so the trailing 4 bytes are stripped.
  // Rebuild with FLAGS (bit 1) set and the FCS bit 0x10.
  std::vector<uint8_t> hdr;
  put(hdr, 0); put(hdr, 0); put16(hdr, 0);
  put32(hdr, (1u << 1) | (1u << 3) | (1u << 5));
  put(hdr, 0x10);  // FLAGS, FCS present
  put(hdr, 0);     // channel field is aligned to 2
  put16(hdr, 5955); put16(hdr, 0);
  put(hdr, (uint8_t)-55);
  hdr[2] = (uint8_t)hdr.size();
  hdr.insert(hdr.end(), fr6.begin(), fr6.end());
  cz::RxFrame drone;
  CHECK(cz::parse_monitor_frame(hdr.data(), hdr.size(), drone));
  CHECK(drone.accepted && drone.kind == det::DRONE);
  CHECK(drone.mhz == 5955);
  CHECK(!(drone.traits & det::traitBit(det::G24)));
  CHECK(drone.badges & sig::FLOCK);

  privacy::begin();
  det::begin();
  cz::ingest_frame(rx);
  const auto counts = det::counts();
  CHECK(counts.wifi >= 1);
  CHECK(cz::lookup_mhz(privacy::macId(addr)) == 2412);
}

static void test_ble() {
  const auto p = cz::le_set_scan_params_passive();
  const uint8_t exp[] = {0x01, 0x0b, 0x20, 0x07, 0x00, 0x60, 0x00, 0x60, 0x00, 0x00, 0x00};
  CHECK(p.len == sizeof exp);
  CHECK(memcmp(p.bytes, exp, sizeof exp) == 0);
  const auto en = cz::le_set_scan_enable(true);
  CHECK(en.bytes[4] == 0x01);
  // Not a connection command (opcode 0x200D would be 0x0d 0x20).
  CHECK(!(p.bytes[1] == 0x0d && p.bytes[2] == 0x20));
  CHECK(!(en.bytes[1] == 0x0d && en.bytes[2] == 0x20));

  // Apple Find My manufacturer payload -> TRK. Address on air is little-endian.
  std::vector<uint8_t> evt = {0x04, 0x3E, 0, 0x02, 0x01, 0x00, 0x01};
  const uint8_t le[6] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
  evt.insert(evt.end(), le, le + 6);
  const uint8_t ad[] = {0x02, 0x01, 0x06, 0x04, 0xFF, 0x4C, 0x00, 0x12};
  evt.push_back(sizeof ad);
  evt.insert(evt.end(), ad, ad + sizeof ad);
  evt.push_back((uint8_t)-60);
  evt[2] = (uint8_t)(evt.size() - 3);
  std::vector<cz::AdvReport> adv;
  CHECK(cz::parse_le_adv_report(evt.data(), evt.size(), adv));
  CHECK(adv.size() == 1);
  CHECK(adv[0].mac[0] == 0x11 && adv[0].mac[5] == 0x66);
  CHECK(adv[0].rssi == -60);
  CHECK(adv[0].badges & sig::TRK);
  CHECK(adv[0].traits & det::traitBit(det::TRK));
  cz::ingest_adv(adv[0]);
  CHECK(det::counts().ble >= 1);
}

static void test_hops() {
  bool a = false, b = false, c = false;
  for (const auto &ch : cz::hop_table()) {
    if (ch.mhz == 2412 && ch.band == 24) a = true;
    if (ch.mhz == 5180 && ch.band == 50) b = true;
    if (ch.mhz == 5955 && ch.band == 60) c = true;
  }
  CHECK(a && b && c);
  CHECK(cz::C5Backend::present() == false);
  CHECK(cz::C5Backend::name() != nullptr);
}

static int spawn(const std::string &bin, const std::vector<std::string> &args, std::string &out) {
  int p[2];
  if (pipe(p) != 0) return 127;
  const pid_t pid = fork();
  if (pid == 0) {
    dup2(p[1], 1);
    dup2(p[1], 2);
    close(p[0]); close(p[1]);
    std::vector<char *> av;
    av.push_back(const_cast<char *>(bin.c_str()));
    for (const auto &s : args) av.push_back(const_cast<char *>(s.c_str()));
    av.push_back(nullptr);
    execv(bin.c_str(), av.data());
    _exit(127);
  }
  close(p[1]);
  char buf[256];
  out.clear();
  while (true) {
    const ssize_t n = read(p[0], buf, sizeof buf);
    if (n <= 0) break;
    out.append(buf, buf + n);
  }
  close(p[0]);
  int st = 0;
  waitpid(pid, &st, 0);
  return WIFEXITED(st) ? WEXITSTATUS(st) : 127;
}

static void test_helper(const std::string &bin) {
  std::string out;
  CHECK(spawn(bin, {"dry-arm", "--if", "wlan1", "--type", "managed", "--nm", "yes"}, out) == 0);
  CHECK(out.find("/usr/bin/nmcli dev set wlan1 managed no") != std::string::npos);
  CHECK(out.find("/usr/sbin/iw dev wlan1 set type monitor") != std::string::npos);
  CHECK(out.find("/usr/sbin/iw dev wlan1 set type managed") != std::string::npos);
  CHECK(out.find("/usr/bin/nmcli dev set wlan1 managed yes") != std::string::npos);
  CHECK(out.find("kill") == std::string::npos);
}

static void test_sys_refused(const std::string &bin) {
  const fs::path fake = fs::temp_directory_path() / "cyd32-sys-must-not-be-read";
  make_sys(fake);
  const std::string path = fake.string();
  std::string out;
  const char *cmds[][2] = {
      {"detect", nullptr},
      {"arm", "wlan1"},
      {"restore", "wlan1"},
      {"hop", "wlan1"},
      {"serve", nullptr},
  };
  for (const auto &c : cmds) {
    std::vector<std::string> args = {c[0], "--sys", path};
    if (c[1]) {
      args = {c[0], "--if", c[1], "--sys", path};
      if (std::string(c[0]) == "hop") {
        args = {c[0], "--if", c[1], "--mhz", "2412", "--sys", path};
      }
    }
    const int rc = spawn(bin, args, out);
    CHECK(rc != 0);
    CHECK(out.find("--sys is not accepted in privileged mode") != std::string::npos);
    CHECK(out.find("chosen wlan1") == std::string::npos);
  }
}

int main(int argc, char **argv) {
  const fs::path sys = fs::temp_directory_path() / "cyd32-awus-sys";
  make_sys(sys);
  test_sha();
  test_detect(sys);
  test_arm();
  test_frames();
  test_ble();
  test_hops();
  std::string helper, sys_refused;
  for (int i = 1; i + 1 < argc; i++) {
    if (std::string(argv[i]) == "--helper") helper = argv[i + 1];
    if (std::string(argv[i]) == "--sys-refused") sys_refused = argv[i + 1];
  }
  if (!helper.empty()) test_helper(helper);
  if (!sys_refused.empty()) test_sys_refused(sys_refused);
  std::cout << (fails ? "FAIL" : "OK") << " " << checks << " checks, " << fails << " failed\n";
  return fails ? 1 : 0;
}
