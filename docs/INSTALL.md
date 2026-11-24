<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# CYD32 installation guide — CYD and M5Stack StopWatch

**Release covered: `v0.2.0-test6`** (pre-release, built from commit `8bb5917118a2`, published 24 Nov 2026, 07:09 Berlin time). **Unsigned build: USB install only.**

- Release page: <https://github.com/b-air/CYD32/releases/tag/v0.2.0-test6>
- **Private repository:** every `github.com/b-air/CYD32/...` link in this guide (release page and downloads) works only when you are **signed in to GitHub** with an account that has access to `b-air/CYD32`. Otherwise GitHub shows **404 – Not Found**.
- There is no "Latest" release: all CYD32 builds so far are pre-releases. Open the `v0.2.0-test6` link above directly, or the full list of releases: <https://github.com/b-air/CYD32/releases>.

> ⚠️ **HARDWARE TEST STATUS.** `v0.2.0-test6` is a preview test build, **not tested on hardware yet** (neither board). Follow these steps to test it, and expect problems.

> ⚠️ **UNSIGNED IMAGES.** `v0.2.0-test6` has **no `-signed.bin` files**: its firmware files are for the **USB install only**. Signed images come back before the public phase, for the OTA tests. Section 4 (OTA) applies once a release with signed files exists.

> ℹ️ **WHAT `v0.2.0-test6` CHANGES** (full list: [`CHANGELOG.md`](../CHANGELOG.md); screens and keys: [`NAVIGATION.md`](NAVIGATION.md)).
> - **StopWatch Wi-Fi start-up fixed** (it stayed off on `v0.2.0-test5`, `W!`). **ABOUT** now shows the Wi-Fi status and free RAM on both boards.
> - Tabs **SONAR / DASH / CREA / MENU**; menu with 6 entries: SONAR, COUNTERS, WARDRIVE, CREATURE, BEEP, OTA.
> - 7 sonar modes (swipe on the scope), a dashboard with one view at a time, a proximity beep (CYD: needs a small speaker on the **SPEAK** connector), a larger creature with **PET** and **FEED**.

## Already on `v0.2.0-test5`? Update with one file

The bootloader, partition table and `boot_app0` files of `v0.2.0-test6` are **byte-identical** to those of `v0.2.0-test5` for both boards (same SHA-256, checked file by file). Only the application changes, so a board that runs `v0.2.0-test5` (installed over USB with this guide) needs **one file**:

| Board | File (direct link, sign in first) | Size | SHA-256 | Address |
|---|---|---|---|---|
| CYD | [firmware-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/firmware-cyd.bin) | 1,633,872 bytes | `9c528f006a57f094d5de708a91558fea0fe16e89b14dbd16b80c64d7c0cb3734` | `0x10000` |
| StopWatch | [firmware-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/firmware-m5stack-stopwatch.bin) | 1,740,544 bytes | `be186d7d7c4b9694d8b941ee7b02dd2e8e52dae651d81c5db26913a951f7cd27` | `0x10000` |

1. On a computer, in Chrome or Edge, sign in to GitHub and download the one file for your board (link above, or **Assets** on the release page).
2. Open <https://espressif.github.io/esptool-js/>, connect the board as in section 2.3 (CYD) or 3.3 (StopWatch: download mode first), steps 1 to 7.
3. Keep **one row only**: Flash Address **`0x10000`**, file `firmware-cyd.bin` or `firmware-m5stack-stopwatch.bin`. Remove any other row.
4. Flash settings: CYD `dio` / `40m` / `4MB`; StopWatch `dio` / `80m` / `16MB`. **Do not tick Erase all.**
5. Click **Program**. Success: one row at 100 %, `Hash of data verified.` once, then `Leaving...`.
6. Restart the board (CYD: EN/RST; StopWatch: one click on the power button). Open **MENU → COUNTERS → ABOUT**: it must show `Version  v0.2.0-test6` and `Commit   8bb5917118a2`.

If ABOUT still shows an older version, or the board does not start, do the full first install (section 2 or 3) with **Erase all** ticked. This one-file path is only for a board already on `v0.2.0-test5` installed over USB; for a first install, for any other starting point, or when troubleshooting, use the full four-file install.

This guide explains, for each device:

1. how to download the files and check them,
2. how to do the **first install over USB from a computer, using only a web browser** (no command line),
3. how to install **later updates over Wi-Fi from an iPhone** (OTA).

---

## 0. Before you start

### 0.1 Links and access

- Release page: <https://github.com/b-air/CYD32/releases/tag/v0.2.0-test6>
- The firmware repository is **private**. Every `github.com/b-air/CYD32/...` link in this guide only works when you are **signed in to GitHub with an account that has access** to the repository. Otherwise GitHub shows **404 – Not Found**, even though the file exists.
- Web flasher (official Espressif tool): <https://espressif.github.io/esptool-js/>

### 0.2 Which file is used for what

| Use | CYD32 (ESP32-2432S028) | M5Stack StopWatch (C152) |
|---|---|---|
| First install over USB (4 files, all required) | `bootloader-cyd.bin`, `partitions-cyd.bin`, `boot_app0-cyd.bin`, `firmware-cyd.bin` | `bootloader-m5stack-stopwatch.bin`, `partitions-m5stack-stopwatch.bin`, `boot_app0-m5stack-stopwatch.bin`, `firmware-m5stack-stopwatch.bin` |
| Update from `v0.2.0-test5` over USB (1 file, see the top of this guide) | `firmware-cyd.bin` at `0x10000` | `firmware-m5stack-stopwatch.bin` at `0x10000` |
| Later updates over Wi-Fi (OTA, from the iPhone) | not in `v0.2.0-test6` (no signed file) | not in `v0.2.0-test6` (no signed file) |

- **No signed files in this release.** OTA updates need a `*-signed.bin` file signed by the maintainer. `v0.2.0-test6` is an unsigned test build, so it has none; signed images come back before the public phase, for the OTA tests. The SHA-256 values in this guide come from the release `SHA256SUMS` and match GitHub's own checksums.
- **There is no merged image.** The release has no single "merged" `.bin`, for either device. Earlier versions of this guide said the StopWatch's first install used "a merged image at `0x0`", but no such file is produced by the release workflow. You flash the **four separate files** at the offsets given below. Writing those four files is the same as writing a merged image.
- Never use a file made for the other device.

### 0.3 What you need

- **A computer** (Windows, macOS or Linux) with **Google Chrome or Microsoft Edge** (desktop version). The web flasher uses **Web Serial**, which Safari and Firefox do not support (the flasher page shows "This tool is not supported on Safari browser!"). An iPhone or iPad **cannot** be used for the first install, because iOS has no Web Serial.
- **A USB data cable.** Some cables only charge; with those the device never appears in the browser's port list. CYD: micro-USB (use the board's only USB port). StopWatch: USB Type-C.
- **A USB driver, only if the port does not appear:**
  - CYD: its USB-to-serial chip is a **CH340** (some batches use a **CP210x**). Windows and older macOS may need the driver: CH340 <https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all>; for a CP210x, the Silicon Labs CP210x USB to UART VCP driver (from the chip maker, Silicon Labs). Linux has both drivers built in.
  - StopWatch: none. The ESP32-S3 has a built-in USB port that every current OS recognises.
- **The 4 first-install files** for your device, downloaded to the computer (section 1).
- For later OTA updates (once a release has signed files): an iPhone with Safari, and the signed `.bin` saved in the **Files** app.

### 0.4 Flash settings at a glance (verified)

These values were read from the image headers of the release files with `esptool image-info` (esptool 5.6.0). The bootloader and the application of each device carry the same values.

| Setting in the web flasher | CYD32 (ESP32) | StopWatch (ESP32-S3) |
|---|---|---|
| Baudrate | `921600` (fallback `115200`) | `921600` (fallback `115200`) |
| Flash Mode | `dio` | `dio` |
| Flash Frequency | `40m` | `80m` |
| Flash Size | `4MB` | `16MB` |
| Erase all | ticked (see the step) | ticked (see the step) |
| Bootloader address | `0x1000` | `0x0` |
| Partition table / boot_app0 / application | `0x8000` / `0xe000` / `0x10000` | `0x8000` / `0xe000` / `0x10000` |

The flasher only writes Flash Mode, Frequency and Size into the file at the **bootloader address**. Choosing the values above writes exactly what the files already contain, so the result is the same as `keep`, but it removes any doubt.

---

## 1. Download and check the files

### 1.1 Download

Sign in to GitHub in your browser first. Then open the release page and download the files in the **Assets** section, or use the direct links in sections 2.1 and 3.1. All files from the same release go together. Do not mix them with files from another release.

### 1.2 Check the SHA-256 (no command line needed)

The release has a `SHA256SUMS` file: <https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/SHA256SUMS>. It lists every file in this guide (the eight `.bin` files, the source archive and the SBOM). Use the values in sections 2.1 and 3.1, or check against that file.

**Option A – on the GitHub page (simplest).** On the release page, GitHub shows a `sha256:` checksum next to each asset in **Assets**. GitHub computes it when the file is uploaded. Check that it matches the value in the tables in sections 2.1 and 3.1 (every row comes from the release `SHA256SUMS`; all were checked against GitHub's own checksums). This proves that the file on GitHub is the one the release built. It does not check the copy on your computer.

**Option B – check the copy on your computer (optional, built-in tools).**
- macOS: open Terminal in the download folder and run `shasum -a 256 firmware-cyd.bin` (do the same for each file).
- Windows: open PowerShell in the download folder and run `Get-FileHash firmware-cyd.bin` (SHA-256 is the default).
- Compare the result with the tables in sections 2.1 and 3.1.

**Option C – command line (Linux or macOS).** `gh` must be signed in with an account that can read `b-air/CYD32` (check with `gh auth status`). Run as a normal user:
```bash
mkdir -p ~/cyd32-check && cd ~/cyd32-check
gh release download v0.2.0-test6 -R b-air/CYD32 -p '*.bin' -p SHA256SUMS --clobber
grep '\.bin$' SHA256SUMS | sha256sum -c
```
Every line of the `sha256sum -c` output must end in `OK`. The `grep` keeps only the `.bin` lines, because `SHA256SUMS` also lists the source archive and the SBOM, which this command does not download. If one of the eight `.bin` files is missing, the check fails. This checks the files downloaded into `~/cyd32-check`. On macOS, use `shasum -a 256 -c` instead of `sha256sum -c`.

**If one value does not match, do not flash that file.**

---

## 2. CYD32 — ESP32-2432S028 "Cheap Yellow Display"

This firmware is built for the **single-USB** ESP32-2432S028 (ESP32, 4 MB flash, ILI9341 320×240, XPT2046 touch). Do not use it on the "2 USB" variant.

### 2.1 Files, offsets and SHA-256

Enter these exact four rows in the web flasher. The offsets were checked against the PlatformIO/Arduino-ESP32 2.0.17 build rules and against `partitions.csv`.

| Flash Address | File (direct link) | Size (bytes) | SHA-256 |
|---|---|---|---|
| `0x1000` | [bootloader-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/bootloader-cyd.bin) | 17,536 | `3d234a7471f67b013686dabd4dee7c1fa915c9928463616a94bc9297acf1abf8` |
| `0x8000` | [partitions-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/partitions-cyd.bin) | 3,072 | `82e1bb3fd187861bda39b46fd9a9e76fd90037e4cc21b8fecf6fa9db3fe286e6` |
| `0xe000` | [boot_app0-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/boot_app0-cyd.bin) | 8,192 | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |
| `0x10000` | [firmware-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/firmware-cyd.bin) | 1,633,872 | `9c528f006a57f094d5de708a91558fea0fe16e89b14dbd16b80c64d7c0cb3734` |

For later OTA updates: no signed file in `v0.2.0-test6` (unsigned test build).

Important: on the CYD (ESP32) the bootloader goes at **`0x1000`**, not `0x0`. Set the flasher's **Flash Mode `dio`**, **Flash Frequency `40m`** and **Flash Size `4MB`** (verified in the image headers, see 0.4).

### 2.2 USB driver (only if the port does not show up)

The CYD's USB-to-serial chip is a **CH340** on most boards (source: the CYD community setup guide); some batches use a **CP210x**. If the board does not appear in the browser's port list in step 5 of 2.3, install the matching driver: CH340 <https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all>; CP210x: the Silicon Labs CP210x USB to UART VCP driver (from the chip maker, Silicon Labs). Then unplug and replug the board.

### 2.3 First install, step by step (web flasher)

Every step is one screen or one field of <https://espressif.github.io/esptool-js/>.

**A. Open the flasher**

1. On the computer, open **Chrome** or **Edge** and go to <https://espressif.github.io/esptool-js/>.
2. Leave **Show Debug log** and **WebUSB (CH340)** unticked.
3. Plug the CYD into the computer with the **data** cable (micro-USB, the board's only USB port). The backlight may come on; that is normal.

**B. Program section: Baudrate and Connect**

4. **Baudrate**: leave `921600`.
5. Click **Connect**. The browser opens its own window with a list of serial ports.
   - Pick the port that appears when you plug in the CYD. Typical names: Windows `USB-SERIAL CH340 (COM3)` or `Silicon Labs CP210x … (COMx)`; macOS `wchusbserial…` / `usbserial-…`; Linux `ttyUSB0`. Unplug and replug the cable if you are not sure: the entry that disappears and comes back is the CYD.
   - Click the browser window's **Connect** button.
6. Wait a few seconds. The black log area shows `Connecting...`, then the chip details.
   - **If Connect fails** (`Failed to connect`, a timeout, or the log stops at `Connecting...`): put the ESP32 in download mode by hand. **Hold BOOT, press and release EN/RST (the reset button), then release BOOT.** Reload the page and click **Connect** again.
   - **If it still fails**, reload the page, set **Baudrate** to `115200`, and connect again.
7. Once connected, the page shows **"Connected to device:"** followed by the chip name. It must start with **ESP32** (not ESP32-S3). If it shows the other chip, you have the other device plugged in: click **Disconnect** and stop.

**C. File table: one row per file**

8. The first row already exists, with **Flash Address** `0x1000`. Keep `0x1000`. Click its file box and choose `bootloader-cyd.bin`.
9. Click **Add File**. A new row appears; **its address also starts at `0x1000`**: change it to **`0x8000`**, then choose `partitions-cyd.bin`.
10. Click **Add File**, change the address to **`0xe000`**, and choose `boot_app0-cyd.bin`.
11. Click **Add File**, change the address to **`0x10000`**, and choose `firmware-cyd.bin` (the only firmware file in this release).
12. Check the four rows against the table in 2.1:

    | Row | Flash Address | File |
    |---|---|---|
    | 1 | `0x1000` | `bootloader-cyd.bin` |
    | 2 | `0x8000` | `partitions-cyd.bin` |
    | 3 | `0xe000` | `boot_app0-cyd.bin` |
    | 4 | `0x10000` | `firmware-cyd.bin` |

    If two rows have the same address, the page shows "Offset field in row N is already in use!". Do not use **Add HEX File**.

**D. Flash settings (below the table)**

13. **Flash Mode**: choose **`dio`**.
14. **Flash Frequency**: choose **`40m`**.
15. **Flash Size**: choose **`4MB`**.
16. **Erase all**: **tick it.** It erases the whole flash before writing: older firmware, saved Wi-Fi or settings, and the OTA record are removed. It can add up to a minute. Tick it for the first install, after any other firmware, and every time you are troubleshooting. Leaving it unticked is only fine when you reinstall a newer CYD32 build over a working CYD32.

**E. Program**

17. Click **Program**. Each row shows a progress bar; the log shows `Erasing flash (this may take a while)...` (if Erase all is ticked), then, for each file, `Compressed …`, `Wrote … bytes … at 0x… in … seconds` and `Hash of data verified.` Do not unplug the device while it is writing.
18. **Success** looks like this: all four rows reached 100 %, the log shows `Hash of data verified.` **four times**, then `Leaving...` and `Hard resetting via RTS pin...`. Any line starting with `Error:` means the flash failed: see section 6.

**F. Reset and first boot**

19. The flasher resets the board by itself. If nothing changes on the screen within 5 seconds, press **EN/RST** once, or unplug and replug the cable.
20. Click **Disconnect** before opening the serial console (2.5) or closing the page.

### 2.4 How to know it worked (CYD)

- Boot screen (green phosphor radar): the **CYD32** logo and the **PASSIVE RF SONAR** title over a rotating sweep, **DEFENSIVE SONAR** under them, a progress bar, **BLE OK / WIFI OK / RID OK**, the hint **Hold BOOT 3 s: unsigned update**, **BOOT / SELF-TEST** and **% / LOAD** in the top corners and the version in the bottom-left corner. A thin scanline walks down the screen and the titles jump by 2-3 px every few seconds: this is intended.
  (`v0.2.0-test2` showed **CYD32**, then **passive RF sonar - v0.2.0-test2**.)
- After about 3 seconds, the first screen is the **SONAR**. The 4 tabs **SONAR / DASH / CREA / MENU** are at the bottom. The creature (**CREA**) starts as an egg, hatches at the first device, then grows with the number of distinct radio sources.
- The header reads **`CYD32 W0012 B0034 R0000`** (Wi-Fi, BLE and Remote ID contacts seen in the last 60 s). **`W!`** in red instead of the Wi-Fi count means that the Wi-Fi scanner failed to start; BLE keeps working. Report the `wifi:` and `scanner:` lines of the serial console (2.5) if you see it.
- Touch: tap a tab at the left and at the right edge of the screen. The tab under your finger must open (`v0.2.0-test2` mirrored the X axis: a tap on the left opened the tab on the right).
- Tap the **MENU** tab → **COUNTERS**, then the **ABOUT** button on that screen. You should see:
  - `Version  v0.2.0-test6`
  - `Commit   8bb5917118a2`
  - `Target   cyd`
  - `Status   Official` (*expected according to the code for this release build; to be confirmed on the device*)
  - `Key      b25ade7e4885efd1`
  - a **WiFi** line: `OK` (or `ERR`, the error code and the failing step) and a **RAM** line. Send a photo of this screen with any report.
- About 8 seconds after start-up the firmware runs a self-test. The first USB install is not affected by rollback; rollback applies only to OTA updates (section 4).
- A **black screen with the backlight on** was the `v0.2.0-test1` bug, fixed since `v0.2.0-test2`. If it happens, open the serial console (2.5) and note the last line.

### 2.5 Serial console (115200 baud)

The console shows what the firmware prints while it starts. Use it if the screen does not show the boot screen.

1. In the flasher, click **Disconnect** if you are still connected (one page cannot hold the port twice).
2. Scroll to the **Console** section. Set **Baudrate** to **`115200`**.
3. Click **Start**, pick the CYD's port in the browser window, and click **Connect**.
4. Click **Reset** (it pulses the reset line), or press **EN/RST** on the board.
5. Read the log. Take a photo or copy it if you need help.

Expected start of the log (from the first hardware test, ROM and bootloader part):

```text
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
```

`mode:DIO` and `entry 0x400805e4` prove that the bootloader at `0x1000` is correct.

- Since **`v0.2.0-test4`**, each start-up step prints a line before and after it (`[boot  <ms>]` is the time since start, in ms):
  ```text
  [boot  <ms>] CYD32 v0.2.0-test6 (8bb5917118a2) target=cyd
  [boot  <ms>] heap free=<bytes>
  [boot  <ms>] privacy ...
  [boot  <ms>] privacy ok
  [boot  <ms>] detections ...
  [boot  <ms>] detections ok
  [boot  <ms>] creature ...
  [boot  <ms>] creature ok
  [boot  <ms>] fw_status ...
  [boot  <ms>] fw_status ok
  [boot  <ms>] hw ...
  [boot  <ms>] tft.init (ILI9341 HSPI, XPT2046 VSPI) ...
  [boot  <ms>] tft.init (ILI9341 HSPI, XPT2046 VSPI) ok
  [boot  <ms>] backlight ...
  [boot  <ms>] backlight ok
  [boot  <ms>] fillScreen ...
  [boot  <ms>] fillScreen ok
  [boot  <ms>] hw ok
  [boot  <ms>] draw buffer <address>, 15360 bytes
  [boot  <ms>] lv_init ...
  [boot  <ms>] lv_init ok
  [boot  <ms>] lv_display_create ...
  [boot  <ms>] lv_display_create ok
  [boot  <ms>] lv_display_set_buffers ...
  [boot  <ms>] lv_display_set_buffers ok
  [boot  <ms>] ui ...
  [boot  <ms>] ui ok
  [boot  <ms>] first frame ...
  [boot  <ms>] flush #1 x=<x> y=<y> <w>x<h> ...
  [boot  <ms>] flush #1 ok
  (the same two lines for flush #2 and #3)
  [boot  <ms>] first frame ok
  [boot  <ms>] first frame: <n> flushes
  [boot  <ms>] scanner ...
  [boot  <ms>] wifi: clear STA config = ESP_OK (0)
  [boot  <ms>] wifi: mgmt-only filter = ESP_OK (0)
  [boot  <ms>] wifi: set_mode(NULL) = ESP_OK (0)
  [boot  <ms>] wifi: promiscuous probe in NULL mode = ESP_OK (0)
  [boot  <ms>] scanner: wifi=ok (ESP_OK) ble init=1 scan=1
  [boot  <ms>] scanner ok
  [boot  <ms>] ui home ...
  [boot  <ms>] ui home ok
  [boot  <ms>] setup done in <ms> ms, heap free=<bytes>
  ```
  The draw buffer address must end in `0`, `4`, `8` or `c`. If the log stops, the **last `...` line without its `ok`** is the step that hangs. If LVGL detects an error, it prints `[lvgl] ASSERT FAILED at <file>:<line>, rebooting`, and the board restarts.
6. Click **Stop** when you are done.

---

## 3. M5Stack StopWatch (SKU C152, ESP32-S3)

ESP32-S3R8, 16 MB flash, 8 MB PSRAM, 466×466 round AMOLED, two buttons: **KEYA** (yellow, GPIO2) = NEXT and **KEYB** (blue, GPIO1) = SELECT (held: see `docs/NAVIGATION.md`), plus a power button.

### 3.1 Files, offsets and SHA-256

| Flash Address | File (direct link) | Size (bytes) | SHA-256 |
|---|---|---|---|
| `0x0` | [bootloader-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/bootloader-m5stack-stopwatch.bin) | 15,104 | `2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363` |
| `0x8000` | [partitions-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/partitions-m5stack-stopwatch.bin) | 3,072 | `ba8fee9c82f9d2fde118ca4b56ebd577e6c608f0a8cc64952e4dd3603f006290` |
| `0xe000` | [boot_app0-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/boot_app0-m5stack-stopwatch.bin) | 8,192 | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |
| `0x10000` | [firmware-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test6/firmware-m5stack-stopwatch.bin) | 1,740,544 | `be186d7d7c4b9694d8b941ee7b02dd2e8e52dae651d81c5db26913a951f7cd27` |

For later OTA updates: no signed file in `v0.2.0-test6` (unsigned test build).

Important: on the StopWatch (ESP32-S3) the bootloader goes at **`0x0`**, not `0x1000`. All the other offsets are the same as on the CYD. Set **Flash Mode `dio`**, **Flash Frequency `80m`** and **Flash Size `16MB`** (verified in the image headers, see 0.4). (`boot_app0` is the same file for both boards; both copies have the same SHA-256.)

### 3.2 Put the StopWatch in download mode

The official M5Stack documentation (<https://docs.m5stack.com/en/core/StopWatch>, "Enter Download Mode") says:

> Connect the device to a computer via a USB Type-C data cable. Press and hold the power button until the green LED blinks, then release it. The device enters download mode.

On the same page: one click on the power button = power on / reset; double click = power off.

### 3.3 First install, step by step (web flasher)

Every step is one screen or one field of <https://espressif.github.io/esptool-js/>.

**A. Open the flasher**

1. On the computer, open **Chrome** or **Edge** and go to <https://espressif.github.io/esptool-js/>.
2. Leave **Show Debug log** and **WebUSB (CH340)** unticked.
3. Plug the StopWatch into the computer with a **USB Type-C data cable**, then put it in **download mode**: hold the **power button** until the **green LED blinks**, then release it (3.2).

**B. Program section: Baudrate and Connect**

4. **Baudrate**: leave `921600`.
5. Click **Connect**. The browser opens its own window with a list of serial ports.
   - Pick the StopWatch's port. Typical names: Windows `USB JTAG/serial debug unit (COMx)`; macOS `usbmodem…`; Linux `ttyACM0`. Unplug and replug if you are not sure.
   - Click the browser window's **Connect** button.
6. Wait a few seconds. The black log area shows `Connecting...`, then the chip details.
   - **If Connect fails** or no port appears: the watch is not in download mode, or the cable is charge-only. Repeat step 3 (hold the power button until the green LED blinks), reload the page and click **Connect** again. If it still fails, set **Baudrate** to `115200`.
7. Once connected, the page shows **"Connected to device:"** followed by the chip name. It must start with **ESP32-S3** (not plain ESP32). If it shows the other chip, you have the other device plugged in: click **Disconnect** and stop.

**C. File table: one row per file**

8. The first row already exists, with **Flash Address** `0x1000`. Change it to **`0x0`**. Click its file box and choose `bootloader-m5stack-stopwatch.bin`.
9. Click **Add File**. A new row appears; **its address also starts at `0x1000`**: change it to **`0x8000`**, then choose `partitions-m5stack-stopwatch.bin`.
10. Click **Add File**, change the address to **`0xe000`**, and choose `boot_app0-m5stack-stopwatch.bin`.
11. Click **Add File**, change the address to **`0x10000`**, and choose `firmware-m5stack-stopwatch.bin` (the only firmware file in this release).
12. Check the four rows against the table in 3.1:

    | Row | Flash Address | File |
    |---|---|---|
    | 1 | `0x0` | `bootloader-m5stack-stopwatch.bin` |
    | 2 | `0x8000` | `partitions-m5stack-stopwatch.bin` |
    | 3 | `0xe000` | `boot_app0-m5stack-stopwatch.bin` |
    | 4 | `0x10000` | `firmware-m5stack-stopwatch.bin` |

    If two rows have the same address, the page shows "Offset field in row N is already in use!". **Make sure no row is still at `0x1000`.** Do not use **Add HEX File**.

**D. Flash settings (below the table)**

13. **Flash Mode**: choose **`dio`**.
14. **Flash Frequency**: choose **`80m`**.
15. **Flash Size**: choose **`16MB`**.
16. **Erase all**: **tick it.** It erases the whole flash before writing: older firmware, saved Wi-Fi or settings, and the OTA record are removed. It can add up to a minute. Tick it for the first install, after any other firmware, and every time you are troubleshooting. Leaving it unticked is only fine when you reinstall a newer CYD32 build over a working CYD32.

**E. Program**

17. Click **Program**. Each row shows a progress bar; the log shows `Erasing flash (this may take a while)...` (if Erase all is ticked), then, for each file, `Compressed …`, `Wrote … bytes … at 0x… in … seconds` and `Hash of data verified.` Do not unplug the device while it is writing.
18. **Success** looks like this: all four rows reached 100 %, the log shows `Hash of data verified.` **four times**, then `Leaving...` and `Hard resetting via RTS pin...`. Any line starting with `Error:` means the flash failed: see section 6.

**F. Reset and first boot**

19. The flasher resets the watch. If it does not restart, **click the power button once** (reset, according to M5Stack).
20. Click **Disconnect** before opening the serial console (3.5) or closing the page.

### 3.4 How to know it worked (StopWatch)

- Round boot screen (green phosphor radar on a pure black background): the **M5Stack** logo and the **PASSIVE RF SONAR** title over a rotating sweep, **DEFENSIVE SONAR**, **STOPWATCH**, **BLE OK / WIFI OK / RID OK**, **LOAD xx%** with a progress arc on the rim, the version and the hint **Hold KEYA 3 s: unsigned update**. The scanline and the short title jumps are intended.
- Then the **SONAR** screen. The curved 4-tab band (**SONAR / DASH / CREA / MENU**) is on every screen. The creature (**CREA**) starts as an egg, hatches at the first device, then grows with the number of distinct radio sources. All screens are phosphor green (not blue or violet); only drones are yellow. The outer bearing ring sits close to the glass edge. Text is larger than in `v0.2.0-test3`.
- **2.4G** next to the battery means Wi-Fi is scanning. **`W!`** in red instead means that the Wi-Fi scanner failed to start (`v0.2.0-test5` had this problem; `v0.2.0-test6` contains the fix and retries every 30 s); BLE keeps working. Report the ABOUT screen and the `wifi:`, `heap` and `scanner:` lines of the serial console (3.5).
- In `v0.2.0-test2` the StopWatch screen stayed **black**; that was fixed in `v0.2.0-test3`. If the first frame is not drawn within 5 s, the console prints `first frame NOT done after 5 s` with the free memory (3.5).
- On `v0.2.0-test3` phosphor green came out blue or violet and the menu disc was pale yellow; `v0.2.0-test4` contains the byte-order fix. The **A NEXT** hint sits at about 310° and **B SELECT** at about 050°, each pointing at its physical button (not at 9 and 3 o'clock).
- To open the menu: tap **MENU** on the rim, or **hold KEYA and KEYB together for about 1 second** (800 ms in the code). A menu sector opens when you **lift your finger** on it (outlined sectors with readable labels). Tap the **COUNTERS** wedge, then tap **ABOUT** in the centre or press **KEYB** to open **ABOUT**. You should see:
  - `Version  v0.2.0-test6`
  - `Commit   8bb5917118a2`
  - `Target   m5stack-stopwatch`
  - `Status   Official` (*expected, to be confirmed*)
  - `Key      b25ade7e4885efd1`
  - `WIFI OK #1` (or `WIFI ERR ...` with the error name) and a `RAM` line.

### 3.5 Serial console (USB, 115200 in the flasher)

The StopWatch prints on its built-in USB port, so the console uses the same cable and port as the flasher (a "baud rate" is still asked; use `115200`).

1. Click **Disconnect** in the Program section. Restart the watch normally (one click on the power button), **not** in download mode.
2. In the **Console** section, set **Baudrate** to **`115200`**, click **Start**, pick the StopWatch's port and click **Connect**.
3. Click **Reset**, or click the power button once. Lines printed before the console was connected are lost, so reset after connecting. If the port disappears during the reset, click **Start** again.

Expected start (ROM and bootloader):

```text
ESP-ROM:esp32s3-20210327
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
...
mode:DIO, clock div:1
...
entry 0x403c98d0
```

`entry 0x403c98d0` is the entry point of `bootloader-m5stack-stopwatch.bin`. Since `v0.2.0-test4`, the `[boot <ms>]` lines are the same as on the CYD (2.5), except that the display step is `M5.begin (CO5300, CST820B, PMIC)`, there is a `wardriving` step, and the log prints `draw buffer in internal DMA, 40 lines` and then `draw buffer <address>, 37280 bytes` (an internal RAM address, starting with `0x3fc`). `draw buffer in PSRAM, 40 lines` (address starting with `0x3c` or `0x3d`) is only the fallback when internal DMA memory is short: it works, but report it.

**Touch log (since `v0.2.0-test4`).** At start-up one line reports the touch controller, for example `[boot <ms>] touch: init ok=1 i2c=0x15 int=13 range x 0..233 y 0..233 rot=0 panel=468x468` (or `touch: init FAILED ...`). Each tap then prints `touch: down raw=<x>,<y> xy=<x>,<y> r=<distance from centre>`, a `touch: hold ...` line every 0.5 s while the finger stays down, `touch: up xy=<x>,<y> after <ms> ms`, and `touch: hit tab <NAME>` or `touch: hit menu <NAME>` when a tab or a menu sector is recognised. If a tap does not do what you expect, send the `touch: init` line and the lines of 3 test taps: the centre of the screen, the **SONAR** tab (bottom left) and the **MENU** tab (bottom right).

---

## 4. Later updates over Wi-Fi from an iPhone (OTA) — both devices

> **Not available with `v0.2.0-test6`.** This release has no signed files. This section applies once a release ships `firmware-cyd-signed.bin` and `firmware-m5stack-stopwatch-signed.bin` (planned before the public phase, for the OTA tests); check their SHA-256 against that release's `SHA256SUMS`.

Use the **signed** file that matches the device: `firmware-cyd-signed.bin` or `firmware-m5stack-stopwatch-signed.bin`.

### 4.1 On the iPhone, before you start

1. In Safari, sign in to GitHub and open the page of the release that has the signed files (in the **Releases** list of the firmware repository).
2. Under **Assets**, tap the signed file for your device and download it. It goes to the **Files** app (usually **Downloads**). (*iOS dialog wording: to be confirmed.*)
3. Do this **before** you join the device's hotspot. The hotspot has no internet access.

### 4.2 Start update mode on the device

- **CYD:** tap the **MENU** tab → **OTA** → **Start update mode**.
- **StopWatch:** open the menu (tap **MENU**, or hold **KEYA + KEYB** together for about 1 second) → tap the **OTA** wedge → tap **Start update mode**, or press **KEYB** (SELECT).

The device stops scanning and opens a **WPA2 Wi-Fi hotspot for 10 minutes**. The screen shows:
- a Wi-Fi QR code,
- `SSID CYD32-xxxx` (4 random characters),
- `PASS` + 12 random characters (lowercase letters and digits),
- `192.168.4.1` (the update page address),
- the remaining time (`m:ss left`),
- `signed only` (or `UNSIGNED ARMED`, see section 5).

A new SSID and password are generated every time. Only **one** phone can connect at a time.

### 4.3 Connect and upload

1. Open the iPhone **Camera**, point it at the QR code and tap the Wi-Fi prompt to join the network. Or, in **Settings → Wi-Fi**, choose `CYD32-xxxx` and type the `PASS` shown on the screen.
2. Open **Safari** and type exactly **`192.168.4.1`** in the address bar. Use the address, not a name: the page refuses any other host name.
3. The page **"CYD32 — firmware update"** shows Licence, Version, Commit, Source, **Target**, Status and Keys. Check that **Target** is your device (`cyd` or `m5stack-stopwatch`).
4. Under **"Signed firmware file (.bin):"**, tap the file field and choose the signed `.bin` in **Files → Downloads**.
5. Tap **Upload and install**. Keep Safari open and stay close to the device.
6. On the device:
   - **CYD:** the amount received (KB), a progress bar and a status message.
   - **StopWatch:** a 17-segment ring with a percentage, the lines `DOWNLOAD / SHA-256 / FLASH WRITE / REBOOT`, and **DO NOT UNPLUG**.
   **Do not unplug the device and do not leave the Wi-Fi network** until it restarts.
7. If it succeeds, Safari shows **OK**, the device shows **signature OK, rebooting**, and it restarts about 2.5 seconds later.

### 4.4 What the device checks (in this order)

1. The request comes to `192.168.4.1` and carries the page's session token (if not, the upload is rejected).
2. The file is not larger than the OTA slot. The file is written to the **inactive** slot as it arrives.
3. The file starts like an ESP32 image.
4. The **ECDSA P-256 signature** at the end of the file is valid.
5. ESP-IDF accepts the image as valid.
6. The image is built for **this board** (`CYD32-TARGET:cyd` or `CYD32-TARGET:m5stack-stopwatch`). If not, it is rejected **before** the boot slot is changed.
7. Version: if the new file is **older** than the running one (or has no version), the screen shows **DOWNGRADE** with **Cancel / Install**, and Safari asks you to confirm on the device. The same version (for example `v0.2.0-test1` over `v0.2.0-test1`) is accepted without this question.
8. Only then does the device switch the boot slot and restart.

If anything fails, the device erases the inactive slot and the running firmware stays unchanged. You can try again while the 10 minutes are not over.

### 4.5 After the restart: self-test and rollback

About 8 seconds after it starts, the new firmware tests itself (display, memory, scanner). If the test fails, it rolls back by itself to the previous firmware. Check **About** (see 2.4 / 3.4) to see which version is running.

---

## 5. Unsigned update (one-time exception)

Use this only to install a file that is **not signed** (for example a test build). It works for **one update, during that boot only**.

1. Restart the device (CYD: reset button or unplug/replug; StopWatch: one click on the power button).
2. **When the boot screen appears** (it shows "Hold BOOT 3 s…" / "Hold KEYA 3 s…"), **hold** the button:
   - **CYD:** **BOOT**. Hold it only once the splash is showing. If you hold BOOT *before* the reset, the ESP32 starts the USB download mode instead.
   - **StopWatch:** **KEYA** (yellow).
   Start holding as soon as the splash appears and hold it **for 3 seconds without letting go**. From `v0.2.0-test2` on, the firmware arms at exactly 3 seconds and the splash waits while you hold the button.
3. The screen shows **"…held. Arm UNSIGNED update mode?"** with **Cancel** and **Arm unsigned**. Tap **Arm unsigned** within **30 seconds**. Otherwise nothing is armed. On the StopWatch, KEYA or KEYB mean **Cancel**.
4. A red **UNSIGNED FIRMWARE MODE** banner appears. Start update mode (4.2). The screen and the web page show **UNSIGNED ARMED**.
5. Upload the unsigned `.bin` (4.3). The arming only applies to the update session you start next (it is not kept if you leave or restart). Within that session, the first unsigned upload uses it up, whether it succeeds or not.
6. If it succeeds, the device shows **installed (UNSIGNED), rebooting**. After this install, **About** shows `Status Unsigned` and the red banner stays on. Installing a signed file again (OTA) or reflashing over USB removes this status.

Board check, file size, ESP-IDF validation and the downgrade question still apply in this mode. Only the signature check is skipped.

---

## 6. Troubleshooting

| What you see | Meaning / what to do |
|---|---|
| A GitHub link shows **404** | You are not signed in, or your account has no access to the private repo. |
| The flasher says Safari is not supported / nothing happens on the iPhone | Use Chrome or Edge on a computer (Web Serial). |
| No port in the browser list | Charge-only cable: try another one. **CYD:** install the CH340 (or CP210x) driver (0.3). **StopWatch:** not in download mode, so hold the power button until the green LED blinks (3.2). Close any other program that uses the serial port (Arduino IDE, PlatformIO monitor, another browser tab). |
| **Connect** fails or times out | **CYD:** manual download mode: hold **BOOT**, press and release **EN/RST**, release **BOOT**, reload the page, connect again; then retry at `115200` baud. **StopWatch:** repeat the download-mode step (3.2). |
| "Connected to device:" shows the wrong chip | The other device is plugged in. Disconnect and use the right files. |
| "Offset field in row N is already in use!" | Two rows have the same address. A new row always starts at `0x1000`: change it. |
| **Wrong offsets** (for example the CYD bootloader at `0x0`, a StopWatch row left at `0x1000`, or the application at `0x1000`) | The device does not start, or the ROM prints `invalid header` / `flash read err` in a loop. Flash all four files again at the addresses in 2.1 / 3.1, with **Erase all** ticked. |
| **Boot loop**: the console repeats `rst:` lines, `Guru Meditation Error` or `abort() was called` | Note the last lines before the restart. Since `v0.2.0-test2`, a `[lvgl] ASSERT FAILED at …` line names the cause; a **CYD32 FATAL ERROR** screen means start-up stopped on purpose (send the console log). Reflash with **Erase all** ticked; if it continues, send the log. |
| **Black screen with the backlight on** (CYD) | The firmware started but drew nothing. This was the `v0.2.0-test1` bug, fixed since `v0.2.0-test2`. If it still happens, open the console (2.5): the last `...` line without `ok` shows the step that hangs. |
| **Black screen** (StopWatch) | This was the `v0.2.0-test2` bug, fixed in `v0.2.0-test3`. If it still happens, open the console (3.5): note the `flush #` lines and any `first frame NOT done after 5 s` line. |
| Wrong colours on the StopWatch (green looks blue or violet, menu disc pale yellow) | This was the `v0.2.0-test3` bug, fixed in `v0.2.0-test4`. If it still happens, send photos and the serial log (3.5). |
| Screen completely dark, backlight off | The firmware did not start or the files are wrong: check the console (2.5 / 3.5). No output at all after `entry 0x…`: reflash with **Erase all**. No ROM output: the board is not powered, or the cable is charge-only. |
| Console line `getBytesLength(): nvs_get_blob len fail: sha NOT_FOUND` | **Harmless.** On a freshly erased device there is no OTA record yet. Builds since `v0.2.0-test2` no longer print it. |
| Safari: **bad host** | Type exactly `192.168.4.1`. |
| `rejected: bad session token` | Reload `192.168.4.1` and upload again (the page carries a token for each session). |
| `file is not signed` | You uploaded the unsigned `firmware-*.bin`. Use `*-signed.bin` (not available in `v0.2.0-test6`; use unsigned mode, section 5), or arm the unsigned mode (section 5). |
| `invalid signature` / `invalid signature block` | The file is damaged or signed with another key. Download it again and check its SHA-256. |
| `wrong board (image is for …)` / `wrong board (no target marker)` | The file is for the other device (or not a CYD32 image). Nothing was changed. Use the right file. |
| `file larger than the OTA slot` / `not an ESP32 image` / `file too short` / `invalid image (esp_ota_end)` | This is not a valid firmware file for this device. Check the file name and SHA-256. |
| `transfer aborted` | The Wi-Fi connection dropped during the upload. Rejoin the hotspot and try again within the 10 minutes. |
| **DOWNGRADE** on screen | The file is older than the running version. Tap **Install** to accept it or **Cancel**. Without an answer within 2 minutes: `downgrade not confirmed`. |
| The device restarts on its own during update mode | The 10 minutes are over. Start update mode again. |
| After an OTA update, the previous version is still running | The new firmware failed its self-test and the device rolled back (4.5). |

---

## 7. Notes

- **USB always works.** CYD32 burns no eFuse and uses no Secure Boot and no flash encryption, so you can always reinstall over USB (section 2 or 3).
- The web flasher does **not** check which board a file is for. Only the OTA path checks the board. Check the file names carefully before clicking **Program**.
- **Things not confirmed on this build:** the whole `v0.2.0-test6` firmware on both boards (not hardware-tested yet), including the StopWatch Wi-Fi fix and the proximity beep. Also still to confirm: iOS dialog wording, and the "Official" status after a USB install.

## Sources

- Firmware repository `b-air/CYD32` at `8bb5917` (`v0.2.0-test6`): `platformio.ini`, `partitions.csv`, `partitions_stopwatch.csv`, `.github/workflows/release.yml`, `docs/RELEASING.md`, `README.md`, `src/main.cpp`, `src/ota_portal.cpp`, `src/ota_sig.cpp`, `src/fw_status.cpp`, `src/ui.cpp`, `src/ui_round.cpp`, `src/hw.cpp`, `src/hw_stopwatch.cpp`, `include/ui_layout_cyd.h`, `include/ui_layout_round.h`, `scripts/pio_pre_build.py`.
- Release `v0.2.0-test6`: assets and `SHA256SUMS` (checked with `sha256sum -c SHA256SUMS`: 10 OK). Its bootloader, partitions and boot_app0 files were compared byte by byte with the `v0.2.0-test5` ones (identical, both boards). Image headers of the `firmware-*.bin` files: CYD DIO / 40m / 4MB, StopWatch DIO / 80m / 16MB.
- First hardware test of `v0.2.0-test1` (CYD), 18 Nov 2026: serial log and screen state.
- Flash layout: arduino-esp32 2.0.17 `tools/platformio-build.py` (bootloader `0x1000` on ESP32 / `0x0` on ESP32-S3, partitions `0x8000`, boot_app0 `0xe000`); platform-espressif32 `builder/main.py` (application `0x10000`).
- Web flasher: <https://espressif.github.io/esptool-js/> (page and script read on 18 Nov 2026: field names, Flash Mode/Frequency/Size options per chip, "Erase all", success messages; Flash settings are applied only to the file at the bootloader address).
- Flash settings: `esptool image-info` (esptool 5.6.0) on the `v0.2.0-test1`, `v0.2.0-test2`, `v0.2.0-test3` and `v0.2.0-test4` `bootloader-*.bin` and `firmware-*.bin`: CYD DIO / 40m / 4MB, StopWatch DIO / 80m / 16MB. The `v0.2.0-test4` bootloader, partitions and boot_app0 files are byte-identical to `v0.2.0-test3`, the `v0.2.0-test5` ones to `v0.2.0-test4`, and the `v0.2.0-test6` ones to `v0.2.0-test5` (same SHA-256 in `SHA256SUMS`).
- Hardware tests, 18 Nov 2026: `v0.2.0-test1` / `v0.2.0-test3` CYD serial log and screen state; `v0.2.0-test3` StopWatch photos (wrong colours, layout, menu and touch, fixed in `v0.2.0-test4`). 23 Nov 2026: `v0.2.0-test5` USB install on both boards; Wi-Fi did not start on the StopWatch (fixed in `v0.2.0-test6`, not yet re-tested).
- Manual download mode (ESP32): Espressif esptool documentation, "Boot Mode Selection" (`docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html`)
- StopWatch download mode: <https://docs.m5stack.com/en/core/StopWatch>
- CYD CH340 / cable / 115200: CYD community guide `github.com/witnessmenow/ESP32-Cheap-Yellow-Display` (`SETUP.md`)
- GitHub asset checksums: GitHub changelog, 3 Jun 2025, "Releases now expose digests for release assets" (`github.blog/changelog/2025-06-03-releases-now-expose-digests-for-release-assets/`)
