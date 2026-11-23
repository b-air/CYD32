# Navigation (v0.2 batch 6)

Both targets share one screen tree (`src/ui_common.cpp`). SONAR is the home
screen (`ui::HOME`).

## Tabs and menu

The 4 tabs are **SONAR / DASH / CREA / MENU** (curved band on the StopWatch,
bottom row on the CYD). DASH opens the dashboard (COUNTERS). The GRID screen
and its TORPEDO easter egg were removed in batch 6.

MENU has the same 6 entries on both targets:

| # | Entry | Opens |
|---|---|---|
| 1 | SONAR | sonar (RSSI distance, indicative angle, 7 modes) |
| 2 | COUNTERS | the dashboard (below) |
| 3 | WARDRIVE | StopWatch: RAM-only log. CYD: microSD export (phase 3, info screen) |
| 4 | CREATURE | the creature (same as the CREA tab) |
| 5 | BEEP | toggles the proximity beep; the entry shows BEEP ON / BEEP OFF |
| 6 | OTA | update mode confirmation |

StopWatch: six 60 deg sectors around a hub, SONAR at the top, then clockwise.
CYD: 2 x 3 buttons. ABOUT (licence, version, source, Wi-Fi status, free RAM)
is reached from the dashboard (StopWatch: the hub of the COUNTERS page or KEYB
there; CYD: the ABOUT button at the top right of DASH).

## Sonar modes

The sonar shows one category at a time. The mode only changes what is drawn
(and what the beep follows); the radios always receive everything.

| Mode | Shows |
|---|---|
| ALL | every visible device |
| WIFI | Wi-Fi access points |
| BLE | BLE devices |
| RID | Remote ID drones |
| FLOCK | devices with the FLOCK badge |
| TRK | devices with the TRK badge |
| META | devices with the META badge |

- Touch, both targets: swipe left on the scope = next mode, swipe right =
  previous mode (wraps around). A tap (no swipe) on a contact opens DEVICE
  DETAIL, as before.
- StopWatch keys: on SONAR, **B long** = next mode, **A long** = previous mode.
- The current mode is shown on the HUD: `MODE` / `ALL` at the top left on the
  CYD, `< ALL >` under the title on the StopWatch. When the beep is on, `BEEP`
  (CYD, top right) or `BEEP ON` (StopWatch, lower left) is shown as well.
- The mode resets to ALL at each boot.

## Proximity beep

MENU > BEEP toggles it (off at boot). While it is on, the board beeps faster as
the followed device gets closer:

- followed device: the pinned contact (TRACK on DETAIL, A short on the
  StopWatch SONAR) if it is still visible, otherwise the strongest non-drone
  device in the current sonar mode;
- below -50 dBm (outside the CLOSE ring): silent; -50 dBm: one beep per
  second; -42 dBm (VERY CLOSE) and above: one beep every 150 ms; linear in
  between (`include/prox_beep.h`); 2.5 kHz, 30 ms;
- CYD: a small passive speaker or buzzer on the `SPEAK` connector (GPIO 26);
  the board has no speaker of its own. StopWatch: the built-in speaker.
- The speaker is powered only while BEEP is on (CYD: the LEDC channel is
  attached only then; StopWatch: `M5.Speaker` is started and stopped). Update
  mode switches the beep off.

## StopWatch keys

| Key | Action |
|---|---|
| A short | next item on the screen (next contact clockwise on SONAR, next sector on MENU, next row on lists then the next dashboard page, next device on DETAIL); on CREATURE, MENU |
| B short | select / open detail (SONAR: DEVICE DETAIL of the selected contact; MENU: open; lists: detail; DETAIL: TRACK; CREATURE: PET; COUNTERS page: ABOUT) |
| A long (0.5 s) | SONAR: previous mode. Elsewhere: back (one step of the back stack, then SONAR) |
| B long (0.5 s) | SONAR: next mode. Elsewhere: SONAR home, back stack cleared |
| A + B held | MENU (unchanged) |

The captions stay at 310 deg (A, "NEXT") and 050 deg (B, "SELECT"). Touch works
everywhere. A short press on SONAR pins the next contact (like TRACK on
DETAIL). On the unsigned-update prompt, B and both long presses mean Cancel and
A short does nothing; arming stays touch-only. During boot the keys are ignored
(the 3 s KEYA unsigned-mode gesture belongs to the splash).

## Dashboard (DASH)

The dashboard shows display state only; nothing in it changes what the radios
receive. Lists use the first hex digits of the per-boot salted device ID (the
same ID as DEVICE DETAIL), the smoothed dBm, the channel for Wi-Fi and the
badges (RND = random address). Lists are strongest first; LOG is the first
sighting (uptime mm:ss) of each drone or badged device, newest first (24
entries kept in RAM).

Views: **WIFI**, **BLE**, **RID** (drones), **TAGGED** (TRK, META or FLOCK,
drones excluded), **LOG**.

- CYD, one screen: a two-line summary (W / B / RID / TRK / META / FLK counts,
  devices in the last 3 minutes and in 24 h), the ABOUT button, a 5-button
  view bar (WIFI / BLE / RID / TAGGED / LOG) and up to 9 rows. Tap a row:
  DEVICE DETAIL (a LOG row opens only if that device is still listed).
- StopWatch, 6 pages: COUNTERS (Wi-Fi / BLE / RID gauges, TRK / META / FLOCK
  counts, 3 min and 24 h totals, ABOUT hub), then WIFI, BLE, RID, TAGGED and
  LOG (7 rows each). Swipe left / right = next / previous page; tap the title
  = next page; tap a row = DEVICE DETAIL. Keys: see above.

The old FILTERS page and the TRACKERS / DRONES screens are gone: the sonar
modes and the dashboard views replace them.

## Creature

- **PET**: the creature "forgets" the 25 oldest devices of its 3-minute window
  (`kPetForget`, `include/creature_rules.h`), so it may shrink; +1 XP. A
  forgotten device that is still around counts again at its next packet.
  It never goes back to an egg. The screen shows `FORGOT n` for 1.5 s.
- **FEED**: a small home-made drone (drawn with lines and circles, spinning
  rotors) flies into the creature's mouth, which chomps it with a few crumbs;
  +2 XP.
- The four sizes are drawn larger than before: zoom 1.125 / 1.375 / 1.625 /
  1.875 on the CYD and 1.0 / 1.17 / 1.33 / 1.5 on the StopWatch (base sprite
  64 px on the CYD, 192 px on the StopWatch).
