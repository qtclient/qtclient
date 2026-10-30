#!/usr/bin/env python3
"""Dump every Qt 6 QMetaObject (moc output) of an x86_64 Mach-O to JSON.

Each class lists its superclass, classinfo, enums, properties, signals, slots,
invokables and constructors with types, plus the qt_static_metacall address.
Works on stripped binaries: metaobjects are found through their pointer layout.

Usage: qt_moc.py BINARY OUT.json [--fixups FIXUPS.txt]
FIXUPS.txt is `dyld_info -fixups BINARY` output (generated when omitted).
"""
from __future__ import annotations

import json
import struct
import subprocess
import sys
from pathlib import Path

BASE = 0x100000000
OUTPUT_REVISIONS = (12, 13)

# QMetaType ids (qmetatype.h)
BUILTIN = {
    0: "UnknownType", 1: "bool", 2: "int", 3: "uint", 4: "qlonglong", 5: "qulonglong",
    6: "double", 7: "QChar", 8: "QVariantMap", 9: "QVariantList", 10: "QString",
    11: "QStringList", 12: "QByteArray", 13: "QBitArray", 14: "QDate", 15: "QTime",
    16: "QDateTime", 17: "QUrl", 18: "QLocale", 19: "QRect", 20: "QRectF", 21: "QSize",
    22: "QSizeF", 23: "QLine", 24: "QLineF", 25: "QPoint", 26: "QPointF", 28: "QVariantHash",
    29: "QEasingCurve", 30: "QUuid", 31: "void*", 32: "long", 33: "short", 34: "char",
    35: "ulong", 36: "ushort", 37: "uchar", 38: "float", 39: "QObject*", 40: "signed char",
    41: "QVariant", 42: "QModelIndex", 43: "void", 44: "QRegularExpression", 45: "QJsonValue",
    46: "QJsonObject", 47: "QJsonArray", 48: "QJsonDocument", 49: "QByteArrayList",
    50: "QPersistentModelIndex", 51: "std::nullptr_t", 52: "QCborSimpleType",
    53: "QCborValue", 54: "QCborArray", 55: "QCborMap", 56: "char16_t", 57: "char32_t",
    0x1000: "QFont", 0x1001: "QPixmap", 0x1002: "QBrush", 0x1003: "QColor",
    0x1004: "QPalette", 0x1005: "QIcon", 0x1006: "QImage", 0x1007: "QPolygon",
    0x1008: "QRegion", 0x1009: "QBitmap", 0x100A: "QCursor", 0x100B: "QKeySequence",
    0x100C: "QPen", 0x100D: "QTextLength", 0x100E: "QTextFormat", 0x100F: "QTransform",
    0x1010: "QMatrix4x4", 0x1011: "QVector2D", 0x1012: "QVector3D", 0x1013: "QVector4D",
    0x1014: "QQuaternion", 0x1015: "QPolygonF", 0x1016: "QColorSpace", 0x2000: "QSizePolicy",
}
UNRESOLVED = 0x80000000

PROP_FLAGS = [
    (0x1, "READ"), (0x2, "WRITE"), (0x4, "RESET"), (0x8, "ENUM"), (0x400, "CONSTANT"),
    (0x800, "FINAL"), (0x01000000, "REQUIRED"), (0x02000000, "BINDABLE"),
]
METHOD_KIND = {0x00: "method", 0x04: "signal", 0x08: "slot", 0x0C: "constructor"}
ACCESS = {0: "private", 1: "protected", 2: "public"}


class Image:
    def __init__(self, path: Path, fixups: Path | None):
        self.b = path.read_bytes()
        self.rebase: dict[int, int] = {}
        self.bind: dict[int, str] = {}
        text = fixups.read_text() if fixups else subprocess.run(
            ["dyld_info", "-fixups", str(path)], capture_output=True, text=True, check=True
        ).stdout
        for line in text.splitlines():
            parts = line.split()
            if len(parts) < 5 or not parts[2].startswith("0x"):
                continue
            addr = int(parts[2], 16)
            if parts[3] == "rebase":
                self.rebase[addr] = int(parts[4], 16)
            elif parts[3] == "bind":
                self.bind[addr] = parts[4].split("/", 1)[-1]

    def off(self, va: int) -> int:
        return va - BASE

    def u32(self, va: int) -> int:
        return struct.unpack_from("<I", self.b, self.off(va))[0]

    def ptr(self, va: int) -> int | None:
        if va in self.rebase:
            return self.rebase[va]
        return None

    def cstr(self, va: int, n: int) -> str:
        o = self.off(va)
        return self.b[o : o + n].decode("utf-8", "replace")


class Meta:
    def __init__(self, img: Image, addr: int):
        self.img, self.addr = img, addr
        self.strings_va = img.ptr(addr + 8)
        self.data_va = img.ptr(addr + 16)
        self.metacall = img.ptr(addr + 24)
        self.super_va = img.ptr(addr)
        self.super_sym = img.bind.get(addr)
        self._strings: list[str] | None = None

    def valid(self) -> bool:
        s, d = self.strings_va, self.data_va
        if not s or not d or not (BASE <= s < BASE + len(self.img.b)) or not (BASE <= d < BASE + len(self.img.b)):
            return False
        try:
            if self.img.u32(d) not in OUTPUT_REVISIONS or self.img.u32(d + 4) != 0:
                return False
            first_off, first_len = self.img.u32(s), self.img.u32(s + 4)
        except struct.error:
            return False
        return first_off % 8 == 0 and 0 < first_off < 0x100000 and 0 < first_len < 512

    def strings(self) -> list[str]:
        if self._strings is None:
            s = self.strings_va
            n = self.img.u32(s) // 8
            self._strings = [
                self.img.cstr(s + self.img.u32(s + 8 * i), self.img.u32(s + 8 * i + 4)) for i in range(n)
            ]
        return self._strings

    def d(self, i: int) -> int:
        return self.img.u32(self.data_va + 4 * i)

    def type_name(self, t: int) -> str:
        if t & UNRESOLVED:
            return self.strings()[t & ~UNRESOLVED]
        return BUILTIN.get(t, f"metatype#{t}")

    def parse(self) -> dict:
        st = self.strings()
        (rev, name, ci_n, ci_i, m_n, m_i, p_n, p_i, e_n, e_i, c_n, c_i, flags, sig_n) = (
            self.d(i) for i in range(14)
        )
        out: dict = {
            "class": st[name],
            "metaobject": hex(self.addr),
            "static_metacall": hex(self.metacall) if self.metacall else None,
            "revision": rev,
            "superclass": None,
            "classinfo": {st[self.d(ci_i + 2 * i)]: st[self.d(ci_i + 2 * i + 1)] for i in range(ci_n)},
        }
        out["_super_va"] = self.super_va
        out["_super_sym"] = self.super_sym
        methods = [self.method(m_i + 6 * i, i, sig_n) for i in range(m_n)]
        ctors = [self.method(c_i + 6 * i, i, 0, ctor=True) for i in range(c_n)]
        out["signals"] = [m for m in methods if m["kind"] == "signal"]
        out["slots"] = [m for m in methods if m["kind"] == "slot"]
        out["invokables"] = [m for m in methods if m["kind"] == "method"]
        out["constructors"] = ctors
        out["properties"] = [self.prop(p_i + 5 * i, methods) for i in range(p_n)]
        out["enums"] = [self.enum(e_i + 5 * i) for i in range(e_n)]
        return out

    def method(self, at: int, index: int, sig_n: int, ctor: bool = False) -> dict:
        st = self.strings()
        name, argc, params, _tag, flags = (self.d(at + k) for k in range(5))
        ret = self.type_name(self.d(params))
        types = [self.type_name(self.d(params + 1 + k)) for k in range(argc)]
        names = [st[self.d(params + 1 + argc + k)] for k in range(argc)]
        kind = "constructor" if ctor else METHOD_KIND.get(flags & 0x0C, "method")
        m = {
            "index": index,
            "kind": kind,
            "name": st[name],
            "access": ACCESS.get(flags & 3, "?"),
            "return": ret,
            "args": [{"type": t, "name": n} for t, n in zip(types, names)],
        }
        if flags & 0x20:
            m["cloned"] = True
        if flags & 0x100:
            m["const"] = True
        return m

    def prop(self, at: int, methods: list[dict]) -> dict:
        st = self.strings()
        name, typ, flags, notify, _rev = (self.d(at + k) for k in range(5))
        p = {
            "name": st[name],
            "type": self.type_name(typ),
            "flags": [label for bit, label in PROP_FLAGS if flags & bit],
        }
        if notify != 0xFFFFFFFF and notify < len(methods) and not (notify & UNRESOLVED):
            p["notify"] = methods[notify]["name"]
        elif notify & UNRESOLVED and notify != 0xFFFFFFFF:
            p["notify_external"] = st[notify & ~UNRESOLVED]
        return p

    def enum(self, at: int) -> dict:
        st = self.strings()
        name, alias, flags, count, data = (self.d(at + k) for k in range(5))
        e = {
            "name": st[name],
            "flag": bool(flags & 1),
            "scoped": bool(flags & 2),
            "values": {st[self.d(data + 2 * i)]: struct.unpack("<i", struct.pack("<I", self.d(data + 2 * i + 1)))[0] for i in range(count)},
        }
        if alias != name:
            e["alias"] = st[alias]
        return e


def demangle(symbols: list[str]) -> dict[str, str]:
    if not symbols:
        return {}
    res = subprocess.run(["c++filt", "-_"], input="\n".join(symbols), capture_output=True, text=True)
    return dict(zip(symbols, res.stdout.splitlines()))


def main() -> int:
    args = sys.argv[1:]
    fixups = None
    if "--fixups" in args:
        i = args.index("--fixups")
        fixups = Path(args[i + 1])
        del args[i : i + 2]
    if len(args) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    img = Image(Path(args[0]), fixups)

    metas: dict[int, Meta] = {}
    for loc in img.rebase:
        m = Meta(img, loc - 8)
        if m.valid():
            metas[m.addr] = m

    classes = []
    by_addr: dict[int, str] = {}
    for addr in sorted(metas):
        try:
            c = metas[addr].parse()
        except (IndexError, struct.error) as err:
            print(f"skip {hex(addr)}: {err}", file=sys.stderr)
            continue
        by_addr[addr] = c["class"]
        classes.append(c)

    externals = demangle(sorted({c["_super_sym"] for c in classes if c["_super_sym"]}))
    for c in classes:
        sva, ssym = c.pop("_super_va"), c.pop("_super_sym")
        if sva in by_addr:
            c["superclass"] = by_addr[sva]
        elif ssym:
            c["superclass"] = externals.get(ssym, ssym).replace("::staticMetaObject", "")

    Path(args[1]).write_text(json.dumps(classes, indent=1))
    print(f"{len(classes)} metaobjects -> {args[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
