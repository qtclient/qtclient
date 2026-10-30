#!/usr/bin/env python3
"""Map C++ RTTI to vtables in a stripped x86_64 Mach-O (Itanium ABI, classic rebase info).

For every typeinfo object the binary defines, lists its vtables (primary and
secondary, with offset-to-top) and each slot's target: an internal function
address or an imported symbol. Internal slots are labelled with the virtual
they override when other classes of the same Qt family leave that slot as an
import (e.g. slot 43 of QQuickItem subclasses is QQuickItem::updatePaintNode).

Usage: macho_rtti.py BINARY OUT.json [--fixups FIXUPS.txt] [--filter REGEX]
"""
from __future__ import annotations

import json
import re
import struct
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

BASE = 0x100000000
# Most-derived first: a vtable belongs to the first family whose prefix appears in its imports.
FAMILIES = [
    "QQuickPaintedItem::", "QQuickItem::", "QAbstractTableModel::", "QAbstractListModel::",
    "QSortFilterProxyModel::", "QAbstractProxyModel::", "QAbstractItemModel::",
    "QQuickImageResponse::", "QQuickAsyncImageProvider::", "QQuickImageProvider::",
    "QSGMaterialShader::", "QSGMaterial::", "QSGGeometryNode::", "QSGNode::",
    "QRunnable::", "QObject::",
]


def fixups_of(path: Path, cached: Path | None = None) -> tuple[dict[int, int], dict[int, str]]:
    text = cached.read_text() if cached else subprocess.run(
        ["dyld_info", "-fixups", str(path)], capture_output=True, text=True, check=True
    ).stdout
    rebase: dict[int, int] = {}
    bind: dict[int, str] = {}
    for line in text.splitlines():
        p = line.split()
        if len(p) < 5 or not p[2].startswith("0x"):
            continue
        addr = int(p[2], 16)
        if p[3] == "rebase":
            rebase[addr] = int(p[4], 16)
        elif p[3] == "bind":
            bind[addr] = p[4].split("/", 1)[-1]
    return rebase, bind


def text_range(path: Path) -> tuple[int, int]:
    out = subprocess.run(["otool", "-l", str(path)], capture_output=True, text=True, check=True).stdout
    m = re.search(r"sectname __text\s+segname __TEXT\s+addr (0x[0-9a-f]+)\s+size (0x[0-9a-f]+)", out)
    start, size = int(m.group(1), 16), int(m.group(2), 16)
    return start, start + size


def demangle(names: list[str]) -> dict[str, str]:
    if not names:
        return {}
    res = subprocess.run(["c++filt", "-_"], input="\n".join(names), capture_output=True, text=True)
    return dict(zip(names, res.stdout.splitlines()))


def read_vtable(rebase, bind, start, text, limit=400):
    slots = []
    va = start
    while len(slots) < limit:
        if va in rebase and text[0] <= rebase[va] < text[1]:
            slots.append(hex(rebase[va]))
        elif va in bind and not bind[va].startswith("__ZTI"):
            slots.append(bind[va])
        else:
            break
        va += 8
    return slots


def main() -> int:
    args = sys.argv[1:]
    opts: dict[str, str] = {}
    rest = []
    i = 0
    while i < len(args):
        if args[i] in ("--fixups", "--filter"):
            opts[args[i]] = args[i + 1]
            i += 2
        else:
            rest.append(args[i])
            i += 1
    if len(rest) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    binary, out = Path(rest[0]), Path(rest[1])
    data = binary.read_bytes()
    rebase, bind = fixups_of(binary, Path(opts["--fixups"]) if "--fixups" in opts else None)
    text = text_range(binary)
    flt = re.compile(opts["--filter"]) if "--filter" in opts else None

    def cstr(va: int) -> str:
        o = va - BASE
        return data[o : data.index(b"\0", o)].decode("latin-1")

    # typeinfo object: [vptr -> __cxxabiv1::*_type_info vtable][name ptr][bases...]
    typeinfos: dict[int, str] = {}
    for loc, sym in bind.items():
        if "cxxabiv1" in sym and "class_type_info" in sym and (loc + 8) in rebase:
            typeinfos[loc] = cstr(rebase[loc + 8])
    pretty = demangle(["__ZTS" + n for n in typeinfos.values()])

    by_ti: dict[int, list[int]] = defaultdict(list)
    for loc, target in rebase.items():
        if target in typeinfos:
            by_ti[target].append(loc)

    classes = []
    for ti, mangled in sorted(typeinfos.items()):
        name = pretty.get("__ZTS" + mangled, mangled).replace("typeinfo name for ", "")
        vtables = []
        for ref in sorted(by_ti.get(ti, [])):
            # vtable group: [offset_to_top][typeinfo][slots...]
            if (ref - 8) in rebase or (ref - 8) in bind:
                continue
            off = struct.unpack_from("<q", data, ref - 8 - BASE)[0]
            if not -0x10000 < off <= 0:
                continue
            slots = read_vtable(rebase, bind, ref + 8, text)
            if slots:
                vtables.append({"address_point": hex(ref + 8), "offset_to_top": off, "slots": slots})
        if vtables:
            classes.append({"class": name, "typeinfo": hex(ti), "vtables": vtables})

    imports = sorted({s for c in classes for v in c["vtables"] for s in v["slots"] if not s.startswith("0x")})
    dm = demangle(imports)

    def family(slots: list[str]) -> str | None:
        names = [dm.get(s, s) for s in slots if not s.startswith("0x")]
        for fam in FAMILIES:
            if any(n.startswith(fam) for n in names):
                return fam
        return None

    consensus: dict[str, dict[int, Counter]] = defaultdict(lambda: defaultdict(Counter))
    for c in classes:
        prim = c["vtables"][0]
        prim["family"] = family(prim["slots"])
        if prim["family"]:
            for idx, s in enumerate(prim["slots"]):
                if not s.startswith("0x"):
                    consensus[prim["family"]][idx][dm.get(s, s)] += 1

    out_classes = []
    for c in classes:
        if flt and not flt.search(c["class"]):
            continue
        for v in c["vtables"]:
            fam = v.get("family")
            named = []
            for idx, s in enumerate(v["slots"]):
                if s.startswith("0x"):
                    guess = consensus[fam][idx].most_common(1)[0][0] if fam and consensus[fam][idx] else None
                    named.append({"slot": idx, "addr": s, "overrides": guess})
                else:
                    named.append({"slot": idx, "import": dm.get(s, s)})
            v["slots"] = named
        out_classes.append(c)

    out.write_text(json.dumps(out_classes, indent=1))
    print(f"{len(out_classes)} classes with vtables -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
