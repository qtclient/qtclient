#!/usr/bin/env python3
"""Compute tip header closure needed by the public SDK (include/qtclient/).

Public packs #include <qtclient/…> only. pack_sdk ships the full public tree
plus the tip headers those façades pull in. There is no committed ALLOWLIST —
extend include/qtclient/ instead.

Usage:
  python3 sdk_tip_closure.py --list
  python3 sdk_tip_closure.py --copy /path/to/client-sdk
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]  # apps/qtclient
PUBLIC = ROOT / "include" / "qtclient"
TIBIA = ROOT / "client" / "tibia"

INC_RE = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')

SEED_PUBLIC = (
    "include/qtclient/mod_api_v1.h",
    "include/qtclient/session.h",
    "include/qtclient/mod_entry.h",
)

TIP_ROOTS = (
    TIBIA,
    ROOT / "include",
)


def resolve_tip(h: str) -> Path | None:
    if h.startswith("qtclient/"):
        p = ROOT / "include" / h
        return p if p.is_file() else None
    if h.startswith("tibia/"):
        rel = h[len("tibia/") :]
        p = TIBIA / rel
        return p if p.is_file() else None
    for root in TIP_ROOTS:
        p = root / h
        if p.is_file():
            return p
    if h.startswith("client/"):
        for cand in (TIBIA / "engine" / h, TIBIA / h):
            if cand.is_file():
                return cand
    return None


def to_repo_path(path: Path) -> str | None:
    try:
        rel = path.relative_to(ROOT).as_posix()
    except ValueError:
        return None
    if (
        rel.startswith("include/qtclient/")
        or rel.startswith("client/tibia/")
        or rel.startswith("include/")
    ):
        return rel
    return None


def walk(start: Path, out: set[str]) -> None:
    stack = [start]
    seen: set[Path] = set()
    while stack:
        path = stack.pop().resolve()
        if path in seen or not path.is_file():
            continue
        seen.add(path)
        ap = to_repo_path(path)
        if ap:
            out.add(ap)
        text = path.read_text(encoding="utf-8", errors="ignore")
        for h in INC_RE.findall(text):
            if h.startswith("Q") or h.startswith("rhi/") or h in ("cip_gen.h",):
                continue
            if h.endswith(".pb.h"):
                continue
            resolved = resolve_tip(h)
            if resolved is None:
                continue
            try:
                resolved.relative_to(ROOT)
            except ValueError:
                continue
            stack.append(resolved)


def collect() -> tuple[list[str], list[str]]:
    if not PUBLIC.is_dir():
        raise SystemExit(f"missing public SDK dir {PUBLIC}")
    paths: set[str] = set()
    for seed in SEED_PUBLIC:
        p = ROOT / seed
        if p.is_file():
            walk(p, paths)
    for hdr in sorted(PUBLIC.rglob("*.h")):
        walk(hdr, paths)
    public = sorted(p for p in paths if p.startswith("include/qtclient/"))
    tip = sorted(p for p in paths if not p.startswith("include/qtclient/"))
    return public, tip


def copy_sdk(dest: Path) -> None:
    """Ship full public tree + tip closure into client-sdk DEST."""
    dest = dest.resolve()
    public_dst = dest / "include" / "qtclient"
    public_dst.parent.mkdir(parents=True, exist_ok=True)
    if public_dst.exists():
        shutil.rmtree(public_dst)
    shutil.copytree(PUBLIC, public_dst)

    _, tip = collect()
    for rel in tip:
        src = ROOT / rel
        if not src.is_file():
            raise SystemExit(f"tip closure path missing: {rel}")
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, out)
    print(f"sdk_tip_closure: copied public tree + {len(tip)} tip headers → {dest}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true", help="print tip closure paths")
    g.add_argument("--copy", type=Path, metavar="DEST", help="copy public+tip into DEST")
    args = ap.parse_args()

    if args.copy:
        copy_sdk(args.copy)
        return 0

    public, tip = collect()
    print(f"# public={len(public)} tip={len(tip)}")
    for p in tip:
        print(p)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
