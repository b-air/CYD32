/* SPDX-License-Identifier: AGPL-3.0-only */
/* SPDX-FileCopyrightText: 2026 CYD32 contributors */
/* Scanner backends for the CardputerZero port.
   AWUS036AXM (mt7921u) is the capture radio. ESP32-C5 is a later coprocessor.
   The internal CYW43439 stays a normal NetworkManager client. */
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "detections.h"

namespace cz {

struct Cmd {
  std::vector<std::string> argv;
};

struct SavedLink {
  std::string ifname;
  std::string type;  // "managed" unless the iface was already something else
  bool nm_managed = true;
};

struct Iface {
  std::string name;
  std::string driver;
  std::string usb_id;  // "vvvv:pppp" or empty
  bool internal = false;
  bool in_kernel = false;
  bool out_of_tree = false;
};

struct DetectResult {
  std::vector<Iface> ifaces;
  std::string chosen;
  std::string note;
};

struct Exec {
  virtual ~Exec() = default;
  virtual int run(const std::vector<std::string> &argv, std::string &out, std::string &err) = 0;
};

struct RxFrame {
  bool ok = false;
  bool accepted = false;  // beacon, probe response or probe request only
  det::Kind kind = det::WIFI;
  uint8_t mac[6] = {};
  int8_t rssi = 0;
  uint8_t channel = 0;
  uint16_t mhz = 0;
  uint8_t traits = 0;
  uint8_t badges = 0;
  bool fcs = false;
};

struct AdvReport {
  uint8_t mac[6] = {};
  int8_t rssi = 0;
  bool remote_id = false;
  uint8_t traits = 0;
  uint8_t badges = 0;
};

struct HciCmd {
  uint8_t bytes[16] = {};
  size_t len = 0;
};

struct Chan {
  int mhz;
  int band;  // 24, 50 or 60
  int number;
};

bool ifname_ok(const std::string &name);
DetectResult detect_ifaces(const std::string &sys_root);
const Iface *find_iface(const DetectResult &d, const std::string &name);

std::vector<Cmd> plan_arm(const SavedLink &s);
std::vector<Cmd> plan_restore(const SavedLink &s);
Cmd plan_hop(const std::string &ifname, int mhz);
bool arm_allowed(const DetectResult &d, const std::string &ifname, std::string &why);

class Session {
 public:
  explicit Session(Exec &ex) : ex_(ex) {}
  bool arm(const DetectResult &d, const SavedLink &saved, std::string &err);
  bool restore(std::string &err);
  bool hop(int mhz, std::string &err);
  bool armed() const { return armed_; }
  const SavedLink &saved() const { return saved_; }
  bool band6_failed() const { return band6_failed_; }

 private:
  Exec &ex_;
  SavedLink saved_;
  bool armed_ = false;
  bool band6_failed_ = false;
};

const std::vector<Chan> &hop_table();

bool parse_monitor_frame(const uint8_t *p, size_t n, RxFrame &out);
void ingest_frame(const RxFrame &f);  // hashes the address, then det::report
uint16_t lookup_mhz(uint32_t id);

bool parse_le_adv_report(const uint8_t *evt, size_t n, std::vector<AdvReport> &out);
HciCmd le_set_scan_params_passive();
HciCmd le_set_scan_enable(bool on);
void ingest_adv(const AdvReport &a);

std::string find_internal_hci(const std::string &sys_root);

// Later coprocessor. No firmware in this tree.
struct C5Backend {
  static const char *name() { return "esp32-c5"; }
  static bool present() { return false; }
  static const char *reason() {
    return "ESP32-C5 firmware is not in this build. A later image can speak USB-CDC or the "
           "HY2.0-4P UART and send salted ids only (no raw MAC). 802.15.4 is out of scope.";
  }
};

std::string join_cmd(const Cmd &c);

}  // namespace cz
