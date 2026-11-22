# Navigation (v0.2 batch 5)

Both targets share one screen tree (`src/ui_common.cpp`). SONAR is the home
screen (`ui::HOME`).

## Tabs and menu

The 4 tabs are unchanged: **SONAR / GRID / CREA / MENU** (curved band on the
StopWatch, bottom row on the CYD).

MENU has the same 8 entries on both targets:

| # | Entry | Opens |
|---|---|---|
| 1 | SONAR | sonar (RSSI distance, indicative angle) |
| 2 | GRID | A-J x 1-8 grid of indicative cells |
| 3 | TRACKERS | visible devices with a TRK, META or FLOCK badge |
| 4 | DRONES | visible Remote ID devices |
| 5 | WARDRIVE | StopWatch: RAM-only log. CYD: microSD export (phase 3, info screen) |
| 6 | CREATURE | the creature (same as the CREA tab) |
| 7 | COUNTERS | the v0.1 dashboard (below) |
| 8 | OTA | update mode confirmation (was "Update" on the CYD) |

StopWatch: eight 45 deg sectors around a hub, SONAR at the top, then clockwise.
CYD: 2 x 4 buttons. ABOUT (licence, version, source) is reached from the
dashboard (StopWatch: the hub of the COUNTERS page or KEYB there; CYD: the ABOUT
button).

## StopWatch keys

| Key | Action |
|---|---|
| A short | next item on the screen (next contact clockwise on SONAR, next occupied cell on GRID, next sector on MENU, next row on lists, next device on DETAIL); on CREATURE, the next tab |
| B short | select / open detail (SONAR and GRID: DEVICE DETAIL of the selected contact; MENU: open; lists: detail; DETAIL: TRACK; CREATURE: PET; dashboard: detail / toggle a filter / ABOUT) |
| A long (0.5 s) | back (one step of the back stack, then SONAR) |
| B long (0.5 s) | SONAR home, back stack cleared |
| A + B held | MENU (unchanged) |

The captions stay at 310 deg (A, "NEXT") and 050 deg (B, "SELECT"). Touch works
everywhere as before. A short press on SONAR pins the next contact (like TRACK
on DETAIL). On the unsigned-update prompt, B and both long presses mean Cancel and A short
does nothing; arming stays touch-only. During boot the keys are ignored (the 3 s KEYA unsigned-mode
gesture belongs to the splash).

## Dashboard (COUNTERS)

COUNTERS is the v0.1 dashboard. It shows display state only: the filters never
change what the radio receives.

StopWatch, 5 pages (A steps rows, then the next page; tap the title for the
next page):

1. COUNTERS: Wi-Fi / BLE / RID gauges, TRK / META / FLOCK counts, devices in
   the last 3 minutes and in 24 h, ABOUT hub.
2. WIFI: 7 strongest access points (cell, smoothed dBm, channel, badges).
3. BLE: 7 strongest BLE devices (cell, dBm, badges, RND = random address).
4. LOG: first sighting (uptime mm:ss) of each drone or badged device, newest
   first (24 entries kept in RAM).
5. FILTERS: WIFI / BLE / RID / TAGGED ONLY toggles (A = next, B = toggle).

CYD, one page: counters line, Wi-Fi and BLE lists (5 each, tap: detail),
3 log lines, 4 filter toggles and ABOUT.

Filters apply to SONAR, GRID and the dashboard lists. TRACKERS and DRONES
ignore them (they are filters themselves).
