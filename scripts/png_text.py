#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
"""Print the text metadata of a PNG (tEXt, zTXt, iTXt, eXIf chunks), decompressed."""
import struct, sys, zlib

def chunks(data):
    p = 8
    while p + 8 <= len(data):
        n, t = struct.unpack(">I4s", data[p:p + 8])
        yield t, data[p + 8:p + 8 + n]
        p += 12 + n

def text(t, b):
    try:
        if t == b"tEXt":
            return b.replace(b"\0", b"\n")
        if t == b"zTXt":
            k, rest = b.split(b"\0", 1)
            return k + b"\n" + zlib.decompress(rest[1:])
        if t == b"iTXt":
            k, rest = b.split(b"\0", 1)
            flag = rest[0]
            lang, rest = rest[2:].split(b"\0", 1)
            tk, val = rest.split(b"\0", 1)
            return b"\n".join([k, lang, tk, zlib.decompress(val) if flag else val])
        if t == b"eXIf":
            return b
    except Exception:
        return b
    return None

for path in sys.argv[1:]:
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        continue
    for t, b in chunks(data):
        out = text(t, b)
        if out is not None:
            sys.stdout.buffer.write(out + b"\n")
