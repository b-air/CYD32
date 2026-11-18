<!-- SPDX-License-Identifier: AGPL-3.0-only -->
<!-- SPDX-FileCopyrightText: 2026 CYD32 contributors -->
# CYD32 installation guide — CYD and M5Stack StopWatch

**Release covered: `v0.2.0-test1`** (built from commit `d79be3f4d7e4`, published 13 Nov 2026, 17:00 UTC).

> ⚠️ **HARDWARE TEST STATUS.** `v0.2.0-test1` is a test build. **CYD:** the hardware test on 18 Nov 2026 found a boot hang (black screen with the backlight on, see the known issue below); the fix awaits a hardware test. **StopWatch:** not tested on hardware yet. Follow these steps to test it, and expect problems.

> ⚠️ **KNOWN ISSUE IN `v0.2.0-test1` (CYD only).** On the first hardware test (18 Nov 2026), the CYD's backlight came on but the screen stayed **black**, and the serial console stopped after the line `[E][Preferences.cpp:503] getBytesLength(): nvs_get_blob len fail: sha NOT_FOUND`. The flash itself worked (the bootloader and the application start). The firmware stops while it sets up the display: LVGL requires a 4-byte-aligned draw buffer, and in this build the buffer sits at an odd address, so LVGL halts silently. The fix (and a serial line for each start-up step) is ready for the next test build. The StopWatch build is not affected by this bug. The flashing steps below stay the same for the next build; only the file hashes will change.

This guide explains, for each device:

1. how to download the files and check them,
2. how to do the **first install over USB from a computer, using only a web browser** (no command line),
3. how to install **later updates over Wi-Fi from an iPhone** (OTA).

---

## 0. Before you start

### 0.1 Links and access

- Release page: <https://github.com/b-air/CYD32/releases/tag/v0.2.0-test1>
- The firmware repository is **private**. Every `github.com/b-air/CYD32/...` link in this guide only works when you are **signed in to GitHub with an account that has access** to the repository. Otherwise GitHub shows **404 – Not Found**, even though the file exists.
- Web flasher (official Espressif tool): <https://espressif.github.io/esptool-js/>

### 0.2 Which file is used for what

| Use | CYD32 (ESP32-2432S028) | M5Stack StopWatch (C152) |
|---|---|---|
| First install over USB (4 files, all required) | `bootloader-cyd.bin`, `partitions-cyd.bin`, `boot_app0-cyd.bin`, `firmware-cyd.bin` | `bootloader-m5stack-stopwatch.bin`, `partitions-m5stack-stopwatch.bin`, `boot_app0-m5stack-stopwatch.bin`, `firmware-m5stack-stopwatch.bin` |
| Later updates over Wi-Fi (OTA, from the iPhone) | `firmware-cyd-signed.bin` | `firmware-m5stack-stopwatch-signed.bin` |

- The `*-signed.bin` files are signed by the maintainer after the release is built, and are added to the release separately. **Both are on the release** (uploaded 13 Nov 2026, 18:11 UTC). The release `SHA256SUMS` lists them; the SHA-256 values in this guide come from that file and match GitHub's own checksums.
- **There is no merged image.** The release has no single "merged" `.bin`, for either device. Earlier versions of this guide said the StopWatch's first install used "a merged image at `0x0`", but no such file is produced by the release workflow. You flash the **four separate files** at the offsets given below. Writing those four files is the same as writing a merged image.
- Never use a file made for the other device.

### 0.3 What you need

- **A computer** (Windows, macOS or Linux) with **Google Chrome or Microsoft Edge** (desktop version). The web flasher uses **Web Serial**, which Safari and Firefox do not support (the flasher page shows "This tool is not supported on Safari browser!"). An iPhone or iPad **cannot** be used for the first install, because iOS has no Web Serial.
- **A USB data cable.** Some cables only charge; with those the device never appears in the browser's port list. CYD: micro-USB (use the board's only USB port). StopWatch: USB Type-C.
- **A USB driver, only if the port does not appear:**
  - CYD: its USB-to-serial chip is a **CH340** (some batches use a **CP210x**). Windows and older macOS may need the driver: CH340 <https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all>; for a CP210x, the Silicon Labs CP210x USB to UART VCP driver (from the chip maker, Silicon Labs). Linux has both drivers built in.
  - StopWatch: none. The ESP32-S3 has a built-in USB port that every current OS recognises.
- **The 4 first-install files** for your device, downloaded to the computer (section 1).
- For later updates: an iPhone with Safari, and the signed `.bin` saved in the **Files** app.

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

The release has a `SHA256SUMS` file: <https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/SHA256SUMS>. It lists every file in this guide, including the two `*-signed.bin` files. Use the values in sections 2.1 and 3.1, or check against that file.

**Option A – on the GitHub page (simplest).** On the release page, GitHub shows a `sha256:` checksum next to each asset in **Assets**. GitHub computes it when the file is uploaded. Check that it matches the value in the tables in sections 2.1 and 3.1 (every row, signed and unsigned, comes from the release `SHA256SUMS`; all were checked against GitHub's own checksums). This proves that the file on GitHub is the one the release built. It does not check the copy on your computer.

**Option B – check the copy on your computer (optional, built-in tools).**
- macOS: open Terminal in the download folder and run `shasum -a 256 firmware-cyd.bin` (do the same for each file).
- Windows: open PowerShell in the download folder and run `Get-FileHash firmware-cyd.bin` (SHA-256 is the default).
- Compare the result with the tables in sections 2.1 and 3.1.

**Option C – command line (Linux or macOS).** `gh` must be signed in with an account that can read `b-air/CYD32` (check with `gh auth status`). Run as a normal user:
```bash
mkdir -p ~/cyd32-check && cd ~/cyd32-check
gh release download v0.2.0-test1 -R b-air/CYD32 -p '*.bin' -p SHA256SUMS --clobber
grep '\.bin$' SHA256SUMS | sha256sum -c
```
Every line of the `sha256sum -c` output must end in `OK`. The `grep` keeps only the `.bin` lines, because `SHA256SUMS` also lists the source archive and the SBOM, which this command does not download. If a signed or unsigned `.bin` is missing, the check fails. This checks the files downloaded into `~/cyd32-check`. On macOS, use `shasum -a 256 -c` instead of `sha256sum -c`.

**If one value does not match, do not flash that file.**

---

## 2. CYD32 — ESP32-2432S028 "Cheap Yellow Display"

This firmware is built for the **single-USB** ESP32-2432S028 (ESP32, 4 MB flash, ILI9341 320×240, XPT2046 touch). Do not use it on the "2 USB" variant.

### 2.1 Files, offsets and SHA-256

Enter these exact four rows in the web flasher. The offsets were checked against the PlatformIO/Arduino-ESP32 2.0.17 build rules and against `partitions.csv`.

| Flash Address | File (direct link) | SHA-256 |
|---|---|---|
| `0x1000` | [bootloader-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/bootloader-cyd.bin) | `3d234a7471f67b013686dabd4dee7c1fa915c9928463616a94bc9297acf1abf8` |
| `0x8000` | [partitions-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/partitions-cyd.bin) | `82e1bb3fd187861bda39b46fd9a9e76fd90037e4cc21b8fecf6fa9db3fe286e6` |
| `0xe000` | [boot_app0-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/boot_app0-cyd.bin) | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |
| `0x10000` | [firmware-cyd.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/firmware-cyd.bin) | `a5f007d8ef769584c39b37202606a7e30c2b1b045865839094bfec8ae55eb480` |

For later OTA updates: [firmware-cyd-signed.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/firmware-cyd-signed.bin), SHA-256 `eee80df599e5c3175b810ad4904f6ede4898822b2d3214b90ede0175f68d354f` (from the release `SHA256SUMS`).

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
11. Click **Add File**, change the address to **`0x10000`**, and choose `firmware-cyd.bin` (the **unsigned** file; `-signed.bin` is only for OTA).
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

- Boot screen: **CYD32**, then **passive RF sonar - v0.2.0-test1**, a progress bar with **BLE OK / WIFI OK / RID OK**, and the hint **Hold BOOT 3 s: unsigned update**.
- After about 3 seconds, the **SONAR** screen appears with the **SONAR / GRID / CREA / MENU** tabs.
- Tap **MENU → About**. You should see:
  - `Version  v0.2.0-test1`
  - `Commit   d79be3f4d7e4`
  - `Target   cyd`
  - `Status   Official` (*expected according to the code for this release build; to be confirmed on the device*)
  - `Key      b25ade7e4885efd1`
- About 8 seconds after start-up the firmware runs a self-test. The first USB install is not affected by rollback; rollback applies only to OTA updates (section 4).
- **`v0.2.0-test1` on the CYD**: see the known issue at the top. A **black screen with the backlight on** is the expected result of that bug, not a flashing mistake.

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

- With **`v0.2.0-test1`**, the application then prints only one line, and nothing after it:
  ```text
  [    16][E][Preferences.cpp:503] getBytesLength(): nvs_get_blob len fail: sha NOT_FOUND
  ```
  This line is **harmless**: on a freshly erased device there is no OTA record yet. The silence after it is the known issue.
- With the **fixed build** (next test build), the `sha NOT_FOUND` line is gone and each start-up step prints a line before and after it (`[boot  <ms>]` is the time since start, in ms):
  ```text
  [boot  <ms>] CYD32 <version> (<commit>) target=cyd
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
  [boot  <ms>] first frame ok
  [boot  <ms>] scanner ...
  [boot  <ms>] scanner ok
  [boot  <ms>] ui sonar ...
  [boot  <ms>] ui sonar ok
  [boot  <ms>] setup done in <ms> ms, heap free=<bytes>
  ```
  The draw buffer address must end in `0`, `4`, `8` or `c`. If the log stops, the **last `...` line without its `ok`** is the step that hangs. If LVGL detects an error, it prints `[lvgl] ASSERT FAILED at <file>:<line>, rebooting`, and the board restarts.
6. Click **Stop** when you are done.

---

## 3. M5Stack StopWatch (SKU C152, ESP32-S3)

ESP32-S3R8, 16 MB flash, 8 MB PSRAM, 466×466 round AMOLED, two buttons: **KEYA** (yellow, GPIO2) = BACK and **KEYB** (blue, GPIO1) = SELECT, plus a power button.

### 3.1 Files, offsets and SHA-256

| Flash Address | File (direct link) | SHA-256 |
|---|---|---|
| `0x0` | [bootloader-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/bootloader-m5stack-stopwatch.bin) | `2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363` |
| `0x8000` | [partitions-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/partitions-m5stack-stopwatch.bin) | `ba8fee9c82f9d2fde118ca4b56ebd577e6c608f0a8cc64952e4dd3603f006290` |
| `0xe000` | [boot_app0-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/boot_app0-m5stack-stopwatch.bin) | `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0` |
| `0x10000` | [firmware-m5stack-stopwatch.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/firmware-m5stack-stopwatch.bin) | `8204ea3974360454e66a9beedfc3126302657718bfdcad2a06cbe1a2d8765f45` |

For later OTA updates: [firmware-m5stack-stopwatch-signed.bin](https://github.com/b-air/CYD32/releases/download/v0.2.0-test1/firmware-m5stack-stopwatch-signed.bin), SHA-256 `efabfc52ef73ce46c4ecfd3beb64aac86f81495d2d9e54c355ece4ce243b2a2e` (from the release `SHA256SUMS`).

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
11. Click **Add File**, change the address to **`0x10000`**, and choose `firmware-m5stack-stopwatch.bin` (the **unsigned** file; `-signed.bin` is only for OTA).
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

- Round boot screen: **CYD32**, **DEFENSIVE SONAR**, **M5 STOPWATCH**, the markers **BLE / WIFI / RID**, the version **v0.2.0-test1**, and the hint **Hold KEYA 3 s: unsigned update**.
- Then **SONAR ACTIVE**.
- To open the menu: tap **MENU** on the rim, or **hold KEYA and KEYB together for about 1 second** (800 ms in the code). Tap the **COUNTERS** wedge, then tap **SIG** in the centre or press **KEYB** to open **ABOUT**. You should see:
  - `Version  v0.2.0-test1`
  - `Commit   d79be3f4d7e4`
  - `Target   m5stack-stopwatch`
  - `Status   Official` (*expected, to be confirmed*)
  - `Key      b25ade7e4885efd1`

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

`entry 0x403c98d0` is the entry point of `bootloader-m5stack-stopwatch.bin`. On a fresh device the same harmless `sha NOT_FOUND` line can appear with `v0.2.0-test1`. With the fixed build, the `[boot <ms>]` lines are the same as on the CYD (2.5), except that the display step is `M5.begin (CO5300, CST820B, PMIC)`, the draw buffer is in PSRAM (address starting with `0x3c` or `0x3d`), and there is a `wardriving` step.

---

## 4. Later updates over Wi-Fi from an iPhone (OTA) — both devices

Use the **signed** file that matches the device: `firmware-cyd-signed.bin` or `firmware-m5stack-stopwatch-signed.bin`. Both signed files are on the release; check their SHA-256 against sections 2.1 and 3.1 (see 0.2).

### 4.1 On the iPhone, before you start

1. In Safari, sign in to GitHub and open the release page: <https://github.com/b-air/CYD32/releases/tag/v0.2.0-test1>.
2. Under **Assets**, tap the signed file for your device and download it. It goes to the **Files** app (usually **Downloads**). (*iOS dialog wording: to be confirmed.*)
3. Do this **before** you join the device's hotspot. The hotspot has no internet access.

### 4.2 Start update mode on the device

- **CYD:** tap the **MENU** tab → **Update** → **Start update mode**.
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
   Start holding as soon as the splash appears and hold it **for 3 seconds without letting go**. From the next test build on, the firmware arms at exactly 3 seconds and the splash waits while you hold the button.
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
| **Boot loop**: the console repeats `rst:` lines, `Guru Meditation Error` or `abort() was called` | Note the last lines before the restart. With the fixed build, a `[lvgl] ASSERT FAILED at …` line names the cause. Reflash with **Erase all** ticked; if it continues, send the log. |
| **Black screen with the backlight on** (CYD) | The firmware started but drew nothing. With `v0.2.0-test1` this is the known issue (top of this guide): wait for the next test build. With the fixed build, open the console (2.5): the last `...` line without `ok` shows the step that hangs. |
| Screen completely dark, backlight off | The firmware did not start or the files are wrong: check the console (2.5 / 3.5). No output at all after `entry 0x…`: reflash with **Erase all**. No ROM output: the board is not powered, or the cable is charge-only. |
| Console line `getBytesLength(): nvs_get_blob len fail: sha NOT_FOUND` | **Harmless.** On a freshly erased device there is no OTA record yet. The fixed build no longer prints it. |
| Safari: **bad host** | Type exactly `192.168.4.1`. |
| `rejected: bad session token` | Reload `192.168.4.1` and upload again (the page carries a token for each session). |
| `file is not signed` | You uploaded the unsigned `firmware-*.bin`. Use `*-signed.bin`, or arm the unsigned mode (section 5). |
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
- **Things not confirmed on real hardware yet**: the CYD start-up with the fix, the whole StopWatch procedure, the CYD buttons' names and positions, whether the flasher resets the boards by itself, the exact port names, the StopWatch console output, iOS dialog wording, and the "Official" status after a USB install.

## Sources

- Firmware repository `b-air/CYD32` at `d79be3f`: `platformio.ini`, `partitions.csv`, `partitions_stopwatch.csv`, `.github/workflows/release.yml`, `docs/RELEASING.md`, `README.md`, `src/main.cpp`, `src/ota_portal.cpp`, `src/ota_sig.cpp`, `src/fw_status.cpp`, `src/ui.cpp`, `src/ui_round.cpp`, `src/hw.cpp`, `src/hw_stopwatch.cpp`, `include/ui_layout_cyd.h`, `include/ui_layout_round.h`, `scripts/pio_pre_build.py`.
- Release `v0.2.0-test1`: assets, `SHA256SUMS` (including the two `*-signed.bin` files), and the image headers of the `.bin` files.
- Flash layout: arduino-esp32 2.0.17 `tools/platformio-build.py` (bootloader `0x1000` on ESP32 / `0x0` on ESP32-S3, partitions `0x8000`, boot_app0 `0xe000`); platform-espressif32 `builder/main.py` (application `0x10000`).
- Web flasher: <https://espressif.github.io/esptool-js/> (page and script read on 18 Nov 2026: field names, Flash Mode/Frequency/Size options per chip, "Erase all", success messages; Flash settings are applied only to the file at the bootloader address).
- Flash settings: `esptool image-info` (esptool 5.6.0) on the release `bootloader-*.bin` and `firmware-*.bin`: CYD DIO / 40m / 4MB, StopWatch DIO / 80m / 16MB.
- First hardware test, 18 Nov 2026: serial log of the CYD (known issue).
- Manual download mode (ESP32): Espressif esptool documentation, "Boot Mode Selection" (`docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html`)
- StopWatch download mode: <https://docs.m5stack.com/en/core/StopWatch>
- CYD CH340 / cable / 115200: CYD community guide `github.com/witnessmenow/ESP32-Cheap-Yellow-Display` (`SETUP.md`)
- GitHub asset checksums: GitHub changelog, 3 Jun 2025, "Releases now expose digests for release assets" (`github.blog/changelog/2025-06-03-releases-now-expose-digests-for-release-assets/`)
