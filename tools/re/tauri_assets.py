#!/usr/bin/env python3
"""Carve the embedded frontend assets out of a Tauri (wry) Windows executable.

Usage: tauri_assets.py App.exe OUT_DIR   (needs: pip install pefile brotli)

Tauri codegen embeds each asset as (path: &str, bytes: &[u8]) pairs, usually
brotli-compressed. On PE32 a pair is four u32s: path ptr, path len, data ptr,
data len (PE32+ uses u64). Asset paths are found as plain strings, then the
pair that points at each path yields the data blob.
"""
from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

import brotli
import pefile


def main() -> None:
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    pe = pefile.PE(str(src), fast_load=True)
    buf = src.read_bytes()
    base = pe.OPTIONAL_HEADER.ImageBase
    is64 = pe.FILE_HEADER.Machine == 0x8664
    word, fmt = (8, "<Q") if is64 else (4, "<I")

    def off_to_va(off: int) -> int | None:
        for s in pe.sections:
            if s.PointerToRawData <= off < s.PointerToRawData + s.SizeOfRawData:
                return base + s.VirtualAddress + (off - s.PointerToRawData)
        return None

    def va_to_off(va: int) -> int | None:
        rva = va - base
        for s in pe.sections:
            if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
                return s.PointerToRawData + (rva - s.VirtualAddress)
        return None

    path_re = re.compile(rb"/[A-Za-z0-9_./@-]+\.(?:html|js|mjs|css|svg|png|webp|woff2?|ttf|json|ico|mp3|ogg|jpg|jpeg|gif|txt)")
    found = []
    seen: set[str] = set()
    for m in path_re.finditer(buf):
        # Paths are packed back to back without separators; try every prefix end.
        text = m.group(0)
        for end in [m.end()] + [m.start() + k.end() for k in path_re.finditer(text)]:
            for start in range(m.start(), end):
                if buf[start : start + 1] != b"/":
                    continue
                path = buf[start:end].decode()
                va = off_to_va(start)
                if va is None or path in seen:
                    continue
                needle = struct.pack(fmt, va) + struct.pack(fmt, end - start)
                ref = buf.find(needle)
                if ref == -1:
                    continue
                (dptr,) = struct.unpack_from(fmt, buf, ref + 2 * word)
                (dlen,) = struct.unpack_from(fmt, buf, ref + 3 * word)
                doff = va_to_off(dptr)
                if doff is None or dlen <= 0 or dlen > len(buf):
                    continue
                blob = buf[doff : doff + dlen]
                try:
                    data, enc = brotli.decompress(blob), "brotli"
                except brotli.error:
                    data, enc = blob, "raw"
                dest = out / path.lstrip("/")
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(data)
                seen.add(path)
                found.append({"path": path, "encoding": enc, "size": len(data)})
    (out / "_index.json").write_text(json.dumps(found, indent=1))
    print(f"{len(found)} assets -> {out}")


if __name__ == "__main__":
    main()
