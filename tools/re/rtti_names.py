#!/usr/bin/env python3
"""Write Ghidra labels ("0xADDR Class::method" per line) from macho_rtti.py + qt_moc.py output.

Virtual slots are named after the method they override (signature stripped), or
`vN` for slots no imported base declares. Each moc class adds
`Class::qt_static_metacall`. The first name for a shared address wins.

Usage: rtti_names.py VTABLES.json MOC.json OUT.txt
"""

from __future__ import annotations
import json
import re
import sys

vtables_path, moc_path, out_path = sys.argv[1:4]
names = {}


def short(cls):
    return cls.removeprefix("tibia::")


for entry in json.load(open(vtables_path)):
    cls = short(entry["class"])
    for vt in entry["vtables"]:
        for slot in vt["slots"]:
            addr = slot.get("addr")
            if not addr:
                continue
            base = slot.get("overrides")
            method = re.sub(r"\(.*$", "", base).split("::")[-1] if base else f"v{slot['slot']}"
            if method == "___cxa_pure_virtual":
                continue
            names.setdefault(int(addr, 16), f"{cls}::{method}")

for c in json.load(open(moc_path)):
    if c.get("static_metacall"):
        names.setdefault(int(c["static_metacall"], 16), f"{short(c['class'])}::qt_static_metacall")

with open(out_path, "w") as f:
    for addr in sorted(names):
        f.write(f"0x{addr:x} {names[addr]}\n")
print(f"{len(names)} names")
