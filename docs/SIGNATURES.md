# Badge signatures (TRK / META / FLOCK)

Source: `src/signatures.cpp`. Matching is passive and uses only fields the
scanner already receives. Nothing is stored but the badge bits. A badge means
"advertises like", not proof.

| Badge | Radio | Match | Source |
|---|---|---|---|
| TRK | BLE | 16-bit service UUID 0xFD5A | Samsung SmartTag (Bluetooth SIG assigned numbers) |
| TRK | BLE | 0xFEED, 0xFEEC, 0xFD84 | Tile |
| TRK | BLE | 0xFCB2 | DULT, cross-vendor unwanted-tracker spec (IETF draft) |
| TRK | BLE | 0xFDAE | kept from v0.2, source not documented (open question) |
| TRK | BLE | Apple company 0x004C, payload type 0x12 | Find My offline finding (AirTag and Find My accessories away from their owner) |
| META | BLE | UUID 0xFD5F, 0xFEB7, 0xFEB8 | Meta Platforms (Ray-Ban Meta glasses, also Quest) |
| META | BLE | company 0x01AB, 0x0D53 | Meta Platforms, Luxottica |
| FLOCK | Wi-Fi | OUI B4:1E:52 | IEEE MA-L registered to Flock Safety |
| FLOCK | Wi-Fi | SSID starting "flock-" or "Flock Camera" (any case) | setup / camera hotspots |
| FLOCK | BLE | company 0x09C8, name starting "Flock" or "Penguin" | battery packs; lower confidence |
| RND | both | BLE random address type (not static), Wi-Fi locally administered bit | address will rotate |

Where the badges are used (batch 6): the sonar modes FLOCK, TRK and META show
only the devices with that badge; the dashboard TAGGED view lists the
non-drone devices with any of the three (RND alone does not count); the
proximity beep follows the strongest device of the current sonar mode. A
drone (Remote ID) is never TAGGED: it has its own RID mode and view.

Changes from v0.2:

- Apple payload type 0x10 (Nearby Info) is no longer a tracker: every iPhone
  sends it.
- Generic module OUIs (Espressif, Liteon and similar) are not used: they would
  tag every smart plug.

The identifiers were cross-checked against Flock-You (MIT) and SquachWatch-CYD
(GPL-3.0). No code was copied. The SSID is read only to compare it with the
two prefixes, then dropped.
