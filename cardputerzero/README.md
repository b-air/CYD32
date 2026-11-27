# CYD32 on CardputerZero

Linux userspace port of the receive-only sonar. This is not a PlatformIO
environment. The ESP32 firmware in the parent tree is unchanged.

## Architecture

The UI is an unprivileged process. LVGL 9.2.2 is the same release the firmware
builds, so the phosphor radar, the I4 creature sprites and the shared detection
table compile as they are. A host build draws into an SDL2 window (320×170).
On the device the same UI uses the Linux framebuffer and the keyboard event
device. SDL is built here with the dummy video driver so a machine without a
display can still run `--self-test`.

Privileged work is a separate binary, `cyd32-awus-helper`, which is the only
process that should hold `CAP_NET_ADMIN` and `CAP_NET_RAW`. It never gets a
shell. The capture interface is whichever netdev is bound to the in-kernel
`mt7921u` driver, preferring USB id `0e8d:7961`. The internal CYW43439
(`brcmfmac` / `brcmfmac_wcc`) is never selected and never switched to monitor.
Arm is `nmcli dev set IF managed no`, link down, `iw dev IF set type monitor`,
link up. Restore runs on failure, on exit, and when the helper's stdin closes,
putting the type back and returning the interface to NetworkManager when it
had been managed. Channel changes are `iw dev IF set freq MHZ` across 2.4 GHz,
5 GHz and a 6 GHz subset. A 6 GHz hop error is recorded and that band is then
skipped.

Frames are filtered in userspace the same way as the ESP32 scanner: beacon,
probe response and probe request only. Everything else is dropped. BLE is a
passive scan on the adapter that is not the dongle. The header says `SIM` or
`AWUS`. A missing dongle does not pretend to be live.

A later ESP32-C5 coprocessor would be another backend behind the same scanner
edge (kind, rssi, channel, traits, badges, salted id). `C5Backend::present()`
is false in this tree. There is no C5 firmware and no PlatformIO environment
for it.

Shared sources (`detections`, `privacy`, `bearing`, `signatures`, `creature`,
`ui_radar`, `ui_common`, `ui_feed`, the four sprites) are compiled with small
host shims. `hw.cpp`, `scanner.cpp`, the OTA files and `ui.cpp` are not linked.

## Keyboard

| Key | Action |
| --- | --- |
| Left / Up | previous contact, menu row or creature button |
| Right / Down | next |
| Enter / Space | open the row, PET or FEED |
| 1 2 3 4 / Tab | SONAR, DASH, CREA, MENU |
| Esc | back |
| H | SONAR |
| [ ] or M | previous / next mode |
| B | toggle the proximity beep |
| A | ABOUT (from DASH, A is also the last row) |
| Q | quit (SDL). On the device, leave the service instead |

Modes are ALL, WIFI, BLE, RID, FLOCK, TRK, META. DASH views are WIFI, BLE,
RID, TAGGED, LOG. Wardrive is a RAM log. microSD is the system disk and is
not written. MENU → OTA says "Linux update, later." There is no GRID screen
and no TORPEDO screen.

BEEP follows `prox_beep.h` (silent below -50 dBm, 1000 ms at -50, 150 ms at
or above -42). The panel has no documented speaker, so the cadence is a flash
on the header. Bearing is drawn as an indicative angle and is not logged.

## Build

Host UI (SDL). Point `zig` (0.14.1) and CMake at the wrappers, and pass the
LVGL 9.2.2 and SDL 2.30.10 trees if the machine has no packages. Verify each
archive against the SHA-256 sums in "Source archives" below before unpacking.

```sh
cmake -S cardputerzero -B build-cz -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cardputerzero/tools/host-zig.cmake" \
  -DCYD32_LVGL_SOURCE=/path/to/lvgl-9.2.2 \
  -DCYD32_SDL_SOURCE=/path/to/SDL2-2.30.10 \
  -DCYD32_DISPLAY=sdl
cmake --build build-cz
./build-cz/cyd32-radio-test --helper ./build-cz/cyd32-awus-helper
SDL_VIDEODRIVER=dummy ./build-cz/cyd32-ui --self-test
./build-cz/cyd32-ui --dry-run
```

With distro packages, omit the two `*_SOURCE` options. `find_package(SDL2)`
is used and LVGL 9.2.2 is fetched (the SHA-256 is pinned in CMakeLists.txt).

aarch64 helper (static musl), from a machine with `zig` 0.14.1 on `PATH`:

```sh
cmake -S cardputerzero -B build-cz-arm -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cardputerzero/tools/aarch64-musl.cmake" \
  -DCYD32_BUILD_UI=OFF
cmake --build build-cz-arm --target cyd32-awus-helper cyd32-radio-test
```

Device UI (framebuffer, no SDL), cross-compiled with the same musl toolchain:

```sh
cmake -S cardputerzero -B build-cz-fb -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cardputerzero/tools/aarch64-musl.cmake" \
  -DCYD32_DISPLAY=fbdev \
  -DCYD32_LVGL_SOURCE=/path/to/lvgl-9.2.2
cmake --build build-cz-fb --target cyd32-ui
```

On the CardputerZero itself, compile with the distro toolchain (gcc). Do not
pass a Zig CMake toolchain file. The device has 512 MiB of RAM, so keep the
build job count low (`-j1` or `-j2`). Binaries land in the CMake build
directory (`build-cz-fb/` below).

```sh
cmake -S cardputerzero -B build-cz-fb -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCYD32_DISPLAY=fbdev
cmake --build build-cz-fb -j1
```

Radio tests without the UI (no LVGL download). This is also the command CI
runs on a ready pull request:

```sh
cmake -S cardputerzero -B build-cz -DCYD32_BUILD_UI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-cz -j4
ctest --test-dir build-cz --output-on-failure --no-tests=error
```

On the device, prefer `-j1` for that build too.

## Source archives

SHA-256 sums were computed from the official downloads (not invented). Check
`sha256sum -c` before unpacking, or let CMake check LVGL via `URL_HASH`.

| Artifact | Official URL | SHA-256 |
| --- | --- | --- |
| LVGL 9.2.2 | https://github.com/lvgl/lvgl/archive/refs/tags/v9.2.2.tar.gz | `129b4e00e06639fa79d7e8a6cab3c1ecce2445b1a246652ccd34f22e7b17ad6f` |
| SDL 2.30.10 | https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-2.30.10.tar.gz and https://libsdl.org/release/SDL2-2.30.10.tar.gz | `f59adf36a0fcf4c94198e7d3d776c1b3824211ab7aeebeb31fe19836661196aa` |
| Zig 0.14.1 (x86_64 Linux) | https://ziglang.org/download/0.14.1/zig-x86_64-linux-0.14.1.tar.xz | `24aeeec8af16c381934a6cd7d95c807a8cb2cf7df9fa40d359aa884195c4716c` |
| Zig 0.14.1 (aarch64 Linux) | https://ziglang.org/download/0.14.1/zig-aarch64-linux-0.14.1.tar.xz | `f7a654acc967864f7a050ddacfaa778c7504a0eca8d2b678839c21eea47c992b` |

```sh
echo "129b4e00e06639fa79d7e8a6cab3c1ecce2445b1a246652ccd34f22e7b17ad6f  v9.2.2.tar.gz" | sha256sum -c
```

## Install on the device

Create a system user `cyd32` with no login shell. Install the helper as
`root:cyd32` mode `0750`, then apply file capabilities. `chown` and `chmod`
must run before `setcap`. Do not set the setuid bit. Do not run the UI as
root. `packaging/cyd32-cardputer.service` starts the UI as `cyd32` with
`NoNewPrivileges=false` so those file capabilities still apply.

The helper execs only `/usr/sbin/ip`, `/usr/sbin/iw` and `/usr/bin/nmcli`
(confirm these paths on the official M5 image). It raises `CAP_NET_ADMIN` in
the ambient set for `ip` and `iw` only, just before `execve`. With `+ep` and
no ambient capability, arm would fail.

```sh
sudo groupadd --system cyd32
sudo useradd --system --gid cyd32 --home-dir /nonexistent --shell /usr/sbin/nologin cyd32
# Confirm the shell path if /usr/sbin/nologin is absent.

sudo install -o root -g root -m 0755 build-cz-fb/cyd32-ui /usr/local/bin/cyd32-ui
sudo install -o root -g cyd32 -m 0750 build-cz-fb/cyd32-awus-helper /usr/local/bin/cyd32-awus-helper
sudo chown root:cyd32 /usr/local/bin/cyd32-awus-helper
sudo chmod 0750 /usr/local/bin/cyd32-awus-helper
sudo setcap cap_net_admin,cap_net_raw+ep /usr/local/bin/cyd32-awus-helper
getcap /usr/local/bin/cyd32-awus-helper
```

`getcap` should print `cap_net_admin,cap_net_raw=ep` on the helper only.
The UI binary must have no file capabilities. For an interactive test, add
your login user to group `cyd32` (then start a new session) and pass
`--helper /usr/local/bin/cyd32-awus-helper`, or run the systemd unit.

## Radio and USB

Monitor mode on the AWUS036AXM (`mt7921u`) is not documented by the vendor,
and 6 GHz monitor has no official source. This tree plans the commands and
tests them with a fake executor. It does not claim the dongle accepts them.
Confirm the USB id with `lsusb` (`0e8d:7961` is the kernel table, not a
vendor page). Prefer the in-kernel module. If `/sys/module/mt7921u/taint`
contains `O`, the module is out of tree and the helper says so.

The dongle is rated 2.7 W on USB 3.2 and is plugged into a USB 2.0 host port.
The only published current ceiling on this board is the Grove port, under
500 mA. Whether the USB-A port can feed the dongle on battery is unmeasured.
Check it on mains and on battery before leaving a hop running.

The M5 image's copy of `linux-firmware` is not verified here. If `mt7921u`
does not bind, the firmware blobs are the first thing to check. The internal
Wi-Fi stays a NetworkManager client the whole time.

## What to try on the hardware

This repository build does not flash a board.

- 320×170 framebuffer comes up and the four tabs are readable.
- The 46-key map matches the table above (`CYD32_EVDEV` if event0 is wrong).
- The dongle enumerates as `mt7921u` in USB host mode.
- Monitor turns on, a few management frames arrive, and restore returns the
  interface to managed mode, including after an error.
- Hops land on 2.4 GHz and 5 GHz. Treat 6 GHz as best-effort.
- No data frames are shown. Nothing is transmitted.
- The internal Wi-Fi still associates.
- Passive BLE sees advertisements on the internal adapter.
- USB current stays stable on mains and on battery.
- SONAR, DASH, CREA (hatch, PET, FEED), BEEP and ABOUT.
- ESP32-C5 only after a firmware image exists. Not this build.

## Tests without a dongle

`cyd32-radio-test` builds a fake sysfs (internal `brcmfmac` plus `mt7921u` on
`0e8d:7961`), checks the choice, checks that arm is refused on the internal
NIC, records the exact `nmcli` / `ip` / `iw` argv, and checks that a failure
on the monitor command still runs the full restore plan. It also checks the
radiotap walker (including an extended present word), the management-frame
filter, the passive HCI scan bytes, and a simulated advertisement. Pass
`--helper` to run the helper's `detect --sys` and `dry-arm` the same way.
`cyd32-ui --self-test` pumps the SIM source through SONAR, CREA and DASH and
exits 0 only when Wi-Fi, BLE and a drone are visible and the creature has
hatched.
