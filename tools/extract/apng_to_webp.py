#!/usr/bin/env python3
"""Convert APNG (acTL) PNGs to animated WebP for Qt QImageReader (qtclient tip).

Qt's bundled PNG reader does not decode APNG; animated UI assets ship as WebP instead.
Run from repo root or apps/qtclient after extract updates PNGs:

  python3 tools/extract/apng_to_webp.py [--check] [paths...]
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Pillow required: pip install pillow") from exc


def has_actl(data: bytes) -> bool:
    if len(data) < 8 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return False
    offset = 8
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        ctype = data[offset + 4 : offset + 8]
        if ctype == b"acTL":
            return True
        if ctype in (b"IDAT", b"IEND"):
            return False
        offset += 12 + length
    return False


def convert_file(png_path: Path, *, dry_run: bool = False) -> bool:
    data = png_path.read_bytes()
    if not has_actl(data):
        return False

    webp_path = png_path.with_suffix(".webp")
    im = Image.open(png_path)
    frames: list[Image.Image] = []
    durations: list[int] = []
    for i in range(im.n_frames):
        im.seek(i)
        frame = im.convert("RGBA")
        durations.append(max(1, int(im.info.get("duration", 100))))
        frames.append(frame)

    loop = int(im.info.get("loop", 0))
    if dry_run:
        print(f"would convert {png_path} -> {webp_path} ({len(frames)} frames)")
        return True

    frames[0].save(
        webp_path,
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=loop,
        lossless=True,
        method=6,
    )
    png_path.unlink()
    print(f"converted {webp_path.relative_to(png_path.parent.parent.parent)} ({len(frames)} frames)")
    return True


def default_roots() -> list[Path]:
    here = Path(__file__).resolve()
    qtclient = here.parents[2]
    return [qtclient / "data"]


def iter_apngs(roots: list[Path]) -> list[Path]:
    out: list[Path] = []
    for root in roots:
        if not root.is_dir():
            continue
        for path in root.rglob("*.png"):
            try:
                if has_actl(path.read_bytes()):
                    out.append(path)
            except OSError:
                continue
    return sorted(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path, help="PNG files or directories to scan")
    parser.add_argument("--check", action="store_true", help="Exit 1 if any APNG .png remains")
    args = parser.parse_args(argv)

    if args.paths:
        candidates: list[Path] = []
        for p in args.paths:
            if p.is_dir():
                candidates.extend(iter_apngs([p]))
            elif p.is_file():
                candidates.append(p)
    else:
        candidates = iter_apngs(default_roots())

    converted = 0
    for png in candidates:
        if convert_file(png, dry_run=args.check):
            converted += 1

    if args.check:
        if converted:
            print(f"{converted} APNG .png file(s) still present (run without --check to convert)", file=sys.stderr)
            return 1
        print("no APNG .png files found")
        return 0

    print(f"done ({converted} converted)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
