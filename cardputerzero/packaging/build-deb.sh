#!/bin/sh
# Build cyd32_<version>_arm64.deb from an already cross-compiled UI and helper.
# Does not install the package and does not talk to GitHub.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
ver=0.3.1
ui=${1:-"$root/build-cz-fb/cyd32-ui"}
helper=${2:-"$root/build-cz-fb/cyd32-awus-helper"}
out=${3:-"$root/cardputerzero/dist/cyd32_${ver}_arm64.deb"}
[ -f "$ui" ] && [ -f "$helper" ]
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p \
  "$stage/DEBIAN" \
  "$stage/usr/share/APPLaunch/bin" \
  "$stage/usr/share/APPLaunch/applications" \
  "$stage/usr/share/APPLaunch/share/images" \
  "$stage/usr/bin" \
  "$stage/usr/share/doc/cyd32"
cp "$ui" "$stage/usr/share/APPLaunch/bin/cyd32-ui"
cp "$helper" "$stage/usr/share/APPLaunch/bin/cyd32-awus-helper"
cp "$root/cardputerzero/packaging/cyd32-update" "$stage/usr/bin/cyd32-update"
chmod 0755 \
  "$stage/usr/share/APPLaunch/bin/cyd32-ui" \
  "$stage/usr/share/APPLaunch/bin/cyd32-awus-helper" \
  "$stage/usr/bin/cyd32-update"
cp "$root/cardputerzero/packaging/debian/cyd32.desktop" "$stage/usr/share/APPLaunch/applications/cyd32.desktop"
cp "$root/cardputerzero/packaging/cyd32.png" "$stage/usr/share/APPLaunch/share/images/cyd32.png"
cp "$root/cardputerzero/packaging/debian/copyright" "$stage/usr/share/doc/cyd32/copyright"
cp "$root/cardputerzero/packaging/debian/control" "$stage/DEBIAN/control"
cp "$root/cardputerzero/packaging/debian/postinst" "$stage/DEBIAN/postinst"
cp "$root/cardputerzero/packaging/debian/prerm" "$stage/DEBIAN/prerm"
cp "$root/cardputerzero/packaging/debian/postrm" "$stage/DEBIAN/postrm"
chmod 0755 "$stage/DEBIAN" "$stage/DEBIAN/postinst" "$stage/DEBIAN/prerm" "$stage/DEBIAN/postrm"
chmod 0644 "$stage/DEBIAN/control" "$stage/usr/share/doc/cyd32/copyright" \
  "$stage/usr/share/APPLaunch/applications/cyd32.desktop" \
  "$stage/usr/share/APPLaunch/share/images/cyd32.png"
find "$stage" -type d -exec chmod 0755 {} +
mkdir -p "$(dirname "$out")"
dpkg-deb --root-owner-group -Zxz --build "$stage" "$out"
dpkg-deb -I "$out"
dpkg-deb -c "$out"
