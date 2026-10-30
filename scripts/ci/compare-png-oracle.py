#!/usr/bin/env python3
"""Compare a PNG screenshot to a play-baseline.oracle (IHDR + average hash)."""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png_rgb(path: Path) -> tuple[int, int, list[bytearray], int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"not a png: {path}")
    length, typ = struct.unpack(">I4s", data[8:16])
    if typ != b"IHDR" or length < 13:
        raise SystemExit("missing IHDR")
    w, h, bit, color, _comp, _filt, inter = struct.unpack(">IIBBBBB", data[16:29])
    if inter != 0:
        raise SystemExit("interlaced PNG unsupported")
    idat = b""
    off = 8
    while off < len(data):
        length, typ = struct.unpack(">I4s", data[off : off + 8])
        off += 8
        chunk = data[off : off + length]
        off += length + 4
        if typ == b"IEND":
            break
        if typ == b"IDAT":
            idat += chunk
    raw = zlib.decompress(idat)
    bpp = {2: 3, 6: 4, 3: 1, 0: 1, 4: 2}[color]
    stride = w * bpp
    rows: list[bytearray] = []
    i = 0
    prev = bytearray(stride)
    for _y in range(h):
        f = raw[i]
        i += 1
        row = bytearray(raw[i : i + stride])
        i += stride
        if f == 1:
            for x in range(bpp, stride):
                row[x] = (row[x] + row[x - bpp]) & 255
        elif f == 2:
            for x in range(stride):
                row[x] = (row[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                left = row[x - bpp] if x >= bpp else 0
                row[x] = (row[x] + ((left + prev[x]) // 2)) & 255
        elif f == 4:
            for x in range(stride):
                left = row[x - bpp] if x >= bpp else 0
                up = prev[x]
                ul = prev[x - bpp] if x >= bpp else 0
                row[x] = (row[x] + paeth(left, up, ul)) & 255
        rows.append(row)
        prev = row
    return w, h, rows, color


def average_hash(w: int, h: int, rows: list[bytearray], color: int) -> int:
    bpp = {2: 3, 6: 4, 3: 1, 0: 1, 4: 2}[color]

    def sample(gx: int, gy: int) -> float:
        x0, x1 = int(gx * w / 8), int((gx + 1) * w / 8)
        y0, y1 = int(gy * h / 8), int((gy + 1) * h / 8)
        s = n = 0
        for y in range(y0, max(y1, y0 + 1)):
            r = rows[min(y, h - 1)]
            for x in range(x0, max(x1, x0 + 1)):
                px = x * bpp
                if color in (2, 6):
                    s += (r[px] + r[px + 1] + r[px + 2]) // 3
                else:
                    s += r[px]
                n += 1
        return s / max(n, 1)

    cells = [[sample(x, y) for x in range(8)] for y in range(8)]
    avg = sum(sum(r) for r in cells) / 64
    bits = 0
    for y in range(8):
        for x in range(8):
            bits = (bits << 1) | (1 if cells[y][x] >= avg else 0)
    return bits


def load_oracle(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if "=" in line and not line.startswith("#"):
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def hamming(a: int, b: int) -> int:
    return (a ^ b).bit_count()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("png")
    ap.add_argument("oracle")
    args = ap.parse_args()
    oracle = load_oracle(Path(args.oracle))
    w, h, rows, color = decode_png_rgb(Path(args.png))
    want_w, want_h = int(oracle["width"]), int(oracle["height"])
    if (w, h) != (want_w, want_h):
        print(f"IHDR mismatch: got {w}x{h} want {want_w}x{want_h}", file=sys.stderr)
        return 1
    got = average_hash(w, h, rows, color)
    want = int(oracle["ahash"], 16)
    max_h = int(oracle.get("max_hamming", "16"))
    dist = hamming(got, want)
    print(f"ahash got={got:016x} want={want:016x} hamming={dist} max={max_h}")
    if dist > max_h:
        print(f"pixel-oracle ahash too far: {dist} > {max_h}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
