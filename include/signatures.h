// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
// Passive badge signatures: TRK (item trackers), META (camera glasses) and
// FLOCK (Flock Safety ALPR cameras). Pure functions over fields the scanner
// already receives; nothing is stored except the resulting badge bits.
//
// All identifiers are public facts: Bluetooth SIG assigned numbers (company
// identifiers, 16-bit service UUIDs), the IEEE MA-L registry and documented
// SSID / advertised-name patterns. The selection was cross-checked against the
// detection notes of Flock-You (github.com/colonelpanichacks/flock-you, MIT)
// and SquachWatch-CYD (github.com/skizzophrenic/SquachWatch-CYD, GPL-3.0); no
// code was copied from either project. Kept minimal and high-signal on
// purpose: generic module-maker OUIs (Espressif, Liteon, ...) are NOT used,
// because they would tag every smart plug as a camera. See docs/SIGNATURES.md.
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace sig {
enum Badge : uint8_t { TRK = 0x01, META = 0x02, FLOCK = 0x04, RANDOM_ADDR = 0x80 };
// mfg: manufacturer data (company id little-endian first), uuids16: every
// 16-bit service UUID advertised or carried as service data, name: advertised
// local name or nullptr.
uint8_t ble(const uint8_t *mfg, size_t mfgLen, const uint16_t *uuids16, int nUuids, const char *name);
// mac: transmitter address (display order); ssid: SSID bytes of an access
// point frame (beacon / probe response), nullptr for client frames.
uint8_t wifi(const uint8_t mac[6], const uint8_t *ssid, int ssidLen);
}
