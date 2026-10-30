#!/usr/bin/env python3
"""Dump a Qt .qm translation file to JSON: [{context, source, comment, translation}].

Usage: qt_qm.py client.en.qm OUT.json
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

MAGIC = bytes.fromhex("3cb86418caef9c95cd211cbf60a1bddd")
TAG_HASHES, TAG_MESSAGES, TAG_CONTEXTS = 0x42, 0x69, 0x2F

T_END, T_SOURCE16, T_TRANSLATION, T_CONTEXT16 = 1, 2, 3, 4
T_OBSOLETE1, T_SOURCE, T_CONTEXT, T_COMMENT, T_OBSOLETE2 = 5, 6, 7, 8, 9


def parse_messages(blob: bytes) -> list[dict]:
    out: list[dict] = []
    pos = 0
    cur: dict = {}
    while pos < len(blob):
        tag = blob[pos]
        pos += 1
        if tag == T_END:
            if cur:
                out.append(cur)
            cur = {}
        elif tag == T_TRANSLATION:
            (n,) = struct.unpack_from(">I", blob, pos)
            pos += 4
            if n == 0xFFFFFFFF:
                continue
            cur.setdefault("translation", []).append(
                blob[pos : pos + n].decode("utf-16-be", "replace")
            )
            pos += n
        elif tag in (T_SOURCE, T_CONTEXT, T_COMMENT, T_SOURCE16, T_CONTEXT16):
            (n,) = struct.unpack_from(">I", blob, pos)
            pos += 4
            raw = blob[pos : pos + n]
            pos += n
            key = {T_SOURCE: "source", T_SOURCE16: "source", T_CONTEXT: "context",
                   T_CONTEXT16: "context", T_COMMENT: "comment"}[tag]
            enc = "utf-16-be" if tag in (T_SOURCE16, T_CONTEXT16) else "utf-8"
            cur[key] = raw.decode(enc, "replace")
        elif tag == T_OBSOLETE1:
            pos += 4
        elif tag == T_OBSOLETE2:
            pass
        else:
            raise ValueError(f"unknown message tag {tag:#x} at {pos - 1}")
    if cur:
        out.append(cur)
    return out


def main() -> None:
    src, dest = Path(sys.argv[1]), Path(sys.argv[2])
    buf = src.read_bytes()
    if buf[:16] != MAGIC:
        raise SystemExit("not a .qm file")
    pos = 16
    sections: dict[int, bytes] = {}
    while pos < len(buf):
        tag = buf[pos]
        (n,) = struct.unpack_from(">I", buf, pos + 1)
        sections[tag] = buf[pos + 5 : pos + 5 + n]
        pos += 5 + n
    msgs = parse_messages(sections.get(TAG_MESSAGES, b""))
    for m in msgs:
        tr = m.get("translation") or []
        m["translation"] = tr[0] if len(tr) == 1 else tr
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(msgs, ensure_ascii=False, indent=1))
    ctx = {m.get("context", "") for m in msgs}
    print(f"{len(msgs)} messages, {len(ctx)} contexts -> {dest}")


if __name__ == "__main__":
    main()
