# CardputerZero

The Linux port lives in [`cardputerzero/README.md`](../cardputerzero/README.md):
LVGL 9.2.2, an APPLaunch `.deb`, an unprivileged UI, and `cyd32-awus-helper`.
The ESP32 install, signing and release steps are unchanged. Follow
[INSTALL.md](INSTALL.md), [RELEASING.md](RELEASING.md) and
[SIGNATURES.md](SIGNATURES.md) for the CYD and the StopWatch.

## Install

Install from a pre-release that contains `cyd32_0.3.1_arm64.deb` and
`SHA256SUMS`. Replace `TAG` with that pre-release name. Run one command at
a time. `SHA256SUMS` lists only this `.deb`.

```sh
curl --proto '=https' --proto-redir '=https' --tlsv1.2 -fL -O https://github.com/b-air/CYD32/releases/download/TAG/cyd32_0.3.1_arm64.deb -O https://github.com/b-air/CYD32/releases/download/TAG/SHA256SUMS
sha256sum -c SHA256SUMS
sudo apt install ./cyd32_0.3.1_arm64.deb
```

The icon appears in the carousel. `Q`, or Esc on the SONAR screen, returns
there. The first install adds the installing account (or `pi`) to group
`cyd32` when that uid is at least 1000. A new login session is required
before that account can run the radio helper.

## SSH test

Do not stop LightDM and do not kill the launcher.

```sh
systemctl --user stop APPLaunch
/usr/share/APPLaunch/bin/cyd32-ui --self-test; echo "code=$?"
systemctl --user start APPLaunch
```

## Update

`cyd32-update` downloads the newest `.deb`, checks `SHA256SUMS`, checks the
package name, installs it, and prints the version.

```sh
cyd32-update
```

## Remove

```sh
sudo apt remove cyd32
sudo apt purge cyd32
```

`purge` also deletes the `cyd32` system user. `remove` leaves that user.
