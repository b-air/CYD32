#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
# Passive-only guard: fails if firmware sources contain transmit/offensive code.
set -uo pipefail
cd "$(dirname "$0")/.."
dirs=(src include lib)
fail=0
check() {  # $1 description, $2 regex, rest: paths
  local what="$1" re="$2"; shift 2
  local hits
  hits=$(grep -rniE --include='*.c' --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.ino' --include='*.S' --include='*.s' --include='*.inc' -l -- "$re" "$@" 2>/dev/null)
  if [[ -n "$hits" ]]; then echo "::error::forbidden ($what) in: $(echo $hits)"; fail=1; fi
}
check "raw 802.11 transmit" 'esp_wifi_80211_tx|ieee80211_raw_frame|wifi_send_pkt' "${dirs[@]}"
check "deauth"              'deauth|disassoc_attack' "${dirs[@]}"
check "beacon spam"         'beacon_?spam|fake_?beacon|rick_?roll' "${dirs[@]}"
check "BLE spam"            'ble_?spam|sour_?apple|swift_?pair_?spam|setAdvertisementData' "${dirs[@]}"
check "jammer"              'jammer|jamming|nrf24|cc1101' "${dirs[@]}"
check "evil portal"         'evil_?portal|captive_?portal|DNSServer' "${dirs[@]}"
check "BadUSB / HID"        'bad_?usb|USBHIDKeyboard|ducky' "${dirs[@]}"
check "IR attacks"          'tv-?b-?gone|IRsend|IRremote' "${dirs[@]}"
check "BLE advertising/GATT connect" 'NimBLEClient|createClient|ble_gap_connect|ble_gattc_|NimBLEAdvertising|startAdvertising|ble_gap_adv_start' "${dirs[@]}"
check "BLE advertising (IDF)" 'esp_ble_gap_start_advertising|esp_ble_gap_config_adv|esp_ble_gap_config_scan_rsp' "${dirs[@]}"
check "promiscuous DATA frames" 'WIFI_PROMIS_FILTER_MASK_DATA|WIFI_PROMIS_FILTER_MASK_ALL|WIFI_PKT_DATA' "${dirs[@]}"
check "active BLE scan"     'setActiveScan\(\s*true' "${dirs[@]}"
check "WiGLE upload"        'wigle' "${dirs[@]}"
wd=$(find src include lib -iname '*wardriv*' 2>/dev/null)
if [[ -n "$wd" ]]; then
  check "outbound network in wardriving" 'HTTPClient|WiFiClient|esp_http_client|AsyncClient|lwip/sockets|connect\(' $wd
fi
[[ $fail == 0 ]] && echo "guard: OK (passive-only)"
exit $fail
