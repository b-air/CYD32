#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
"""Print the path of boot_app0.bin from one PlatformIO environment's own
framework-arduinoespressif32 package (as recorded in that env's .d files).

Usage: find-boot-app0.py <pio-env>
"""
from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: find-boot-app0.py <pio-env>", file=sys.stderr)
        return 2
    env = sys.argv[1]
    build = Path(".pio/build") / env
    if not build.is_dir():
        print(f"build directory missing: {build}", file=sys.stderr)
        return 1
    found = None
    for dfile in build.rglob("*.d"):
        try:
            text = dfile.read_text(errors="replace")
        except OSError:
            continue
        for tok in text.replace("\\", " ").split():
            i = tok.find("framework-arduinoespressif32")
            if i < 0:
                continue
            pkg = tok[:i] + tok[i:].split("/")[0]
            cand = Path(pkg) / "tools" / "partitions" / "boot_app0.bin"
            if cand.is_file():
                found = cand
                break
        if found:
            break
    if not found:
        print(f"boot_app0.bin not found for env {env}", file=sys.stderr)
        return 1
    print(found)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
