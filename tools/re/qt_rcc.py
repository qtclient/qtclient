#!/usr/bin/env python3
"""Unpack Qt resource trees from a standalone .rcc or carve them out of a binary.

Standalone:  qt_rcc.py file.rcc OUT_DIR
Embedded:    qt_rcc.py --carve client OUT_DIR

Embedded trees (qRegisterResourceData arrays) have no header, so they are found
by scanning for self-validating name records (u16 len, u32 qt_hash, UTF-16BE)
and directory roots, then solving for the names and data base offsets.
"""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
import zlib
from pathlib import Path

FLAG_COMPRESSED = 0x01
FLAG_DIRECTORY = 0x02
FLAG_ZSTD = 0x04


def qt_hash(name: str) -> int:
    h = 0
    for ch in name.encode("utf-16-be").decode("utf-16-be"):
        h = ((h << 4) + ord(ch)) & 0xFFFFFFFF
        h ^= (h & 0xF0000000) >> 23
        h &= 0x0FFFFFFF
    return h


def read_name(buf: bytes, pos: int) -> str | None:
    if pos < 0 or pos + 6 > len(buf):
        return None
    (length,) = struct.unpack_from(">H", buf, pos)
    if length == 0 or length > 255 or pos + 6 + 2 * length > len(buf):
        return None
    (h,) = struct.unpack_from(">I", buf, pos + 2)
    try:
        name = buf[pos + 6 : pos + 6 + 2 * length].decode("utf-16-be")
    except UnicodeDecodeError:
        return None
    return name if qt_hash(name) == h else None


def decompress(flags: int, payload: bytes) -> bytes:
    if flags & FLAG_ZSTD:
        return subprocess.run(
            ["zstd", "-d", "-q", "-c"], input=payload, capture_output=True, check=True
        ).stdout
    if flags & FLAG_COMPRESSED:
        return zlib.decompress(payload[4:])
    return payload


class Tree:
    def __init__(self, buf: bytes, tree: int, names: int, data: int, version: int):
        self.buf, self.tree, self.names, self.data = buf, tree, names, data
        self.entry = 22 if version >= 2 else 14

    def node(self, idx: int):
        off = self.tree + idx * self.entry
        name_off, flags = struct.unpack_from(">IH", self.buf, off)
        if flags & FLAG_DIRECTORY:
            count, child = struct.unpack_from(">II", self.buf, off + 6)
            return name_off, flags, count, child
        _country, _lang, data_off = struct.unpack_from(">HHI", self.buf, off + 6)
        return name_off, flags, None, data_off

    def name(self, name_off: int) -> str | None:
        return read_name(self.buf, self.names + name_off)

    def walk(self, idx: int = 0, prefix: str = "", depth: int = 0):
        _name_off, flags, count, child = self.node(idx)
        if count is None or depth > 32:
            return
        for i in range(child, child + count):
            name_off, cflags, ccount, cval = self.node(i)
            nm = self.name(name_off)
            if nm is None:
                raise ValueError(f"bad name at node {i}")
            path = f"{prefix}/{nm}"
            if ccount is not None:
                yield from self.walk(i, path, depth + 1)
            else:
                yield path, cflags, cval

    def file_bytes(self, flags: int, data_off: int) -> bytes:
        pos = self.data + data_off
        (size,) = struct.unpack_from(">I", self.buf, pos)
        return decompress(flags, self.buf[pos + 4 : pos + 4 + size])


def extract(tree: Tree, out: Path) -> list[dict]:
    rows = []
    for path, flags, data_off in tree.walk():
        try:
            dest = out / path.lstrip("/")
            dest.parent.mkdir(parents=True, exist_ok=True)
            blob = tree.file_bytes(flags, data_off)
            dest.write_bytes(blob)
        except Exception as exc:  # noqa: BLE001
            rows.append({"path": path.replace("\0", "\\0"), "error": str(exc)})
            continue
        rows.append({"path": path, "size": len(blob), "flags": flags})
    return rows


def unpack_rcc(src: Path, out: Path) -> list[dict]:
    buf = src.read_bytes()
    if buf[:4] != b"qres":
        raise SystemExit("not a qres file")
    version, tree, data, names = struct.unpack_from(">IIII", buf, 4)
    return extract(Tree(buf, tree, names, data, version), out)


def carve(src: Path, out: Path) -> list[dict]:
    buf = src.read_bytes()
    # Index every self-validating name record.
    name_at: dict[int, str] = {}
    i = 0
    n = len(buf) - 6
    while i < n:
        if buf[i] == 0 and 0 < buf[i + 1] < 128:
            nm = read_name(buf, i)
            if nm is not None:
                name_at[i] = nm
        i += 1
    print(f"[carve] {len(name_at)} name records", file=sys.stderr)

    roots = []
    pos = buf.find(b"\x00\x00\x00\x00\x00\x02")
    while pos != -1:
        count, child = struct.unpack_from(">II", buf, pos + 6)
        if child == 1 and 0 < count < 64 and buf[pos + 14 : pos + 22] == b"\0" * 8:
            roots.append((pos, count))
        pos = buf.find(b"\x00\x00\x00\x00\x00\x02", pos + 1)
    print(f"[carve] {len(roots)} candidate roots", file=sys.stderr)

    rows: list[dict] = []
    name_positions = sorted(name_at)
    for root, count in roots:
        tree = Tree(buf, root, 0, 0, 2)
        try:
            first_name_off = tree.node(1)[0]
            child_name_offs = [tree.node(k)[0] for k in range(1, 1 + count)]
        except struct.error:
            continue
        files = []
        for p in name_positions:
            base = p - first_name_off
            if not all((base + o) in name_at for o in child_name_offs):
                continue
            tree.names = base
            try:
                files = list(tree.walk())
            except Exception:  # noqa: BLE001
                continue
            if files and not any("\0" in f[0] for f in files):
                break
            files = []
        if not files:
            continue
        offs = sorted({f[2] for f in files})
        data_base = find_data_base(buf, offs, tree, files[0])
        if data_base is None:
            print(f"[carve] root@{root:#x}: {len(files)} files, data base not found", file=sys.stderr)
            continue
        tree.data = data_base
        sub = out / f"root_{root:x}"
        got = extract(tree, sub)
        print(f"[carve] root@{root:#x}: {len(got)} files -> {sub.name}", file=sys.stderr)
        rows += [{**r, "root": f"{root:#x}"} for r in got]
    return rows


def find_data_base(buf: bytes, offs: list[int], tree: Tree, probe) -> int | None:
    """Locate the data block; `probe` (a walked file) must decode at the result."""
    if offs[0] != 0:
        return None
    if len(offs) == 1:
        return data_after_names(buf, tree, probe)
    first_size = offs[1] - 4
    needle = struct.pack(">I", first_size)
    pos = buf.find(needle)
    while pos != -1:
        ok = True
        for a, b in zip(offs[1:4], offs[2:5]):
            (sz,) = struct.unpack_from(">I", buf, pos + a)
            if sz != b - a - 4:
                ok = False
                break
        if ok:
            tree.data = pos
            try:
                tree.file_bytes(probe[1], probe[2])
                return pos
            except Exception:  # noqa: BLE001
                pass
        pos = buf.find(needle, pos + 1)
    return None


def data_after_names(buf: bytes, tree: Tree, probe) -> int | None:
    """Single-file trees: rcc emits the data block right after the (padded) names."""
    end = tree.names
    while (nm := read_name(buf, end)) is not None:
        end += 6 + 2 * len(nm)
    for pos in range(end, end + 16):
        (size,) = struct.unpack_from(">I", buf, pos)
        if not 0 < size < 1 << 24:
            continue
        tree.data = pos
        try:
            tree.file_bytes(probe[1], probe[2])
            return pos
        except Exception:  # noqa: BLE001
            pass
    return None


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("src", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--carve", action="store_true")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    rows = carve(args.src, args.out) if args.carve else unpack_rcc(args.src, args.out)
    (args.out / "_index.json").write_text(json.dumps(rows, indent=1))
    print(f"{len(rows)} entries -> {args.out}")


if __name__ == "__main__":
    main()
