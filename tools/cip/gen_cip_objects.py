#!/usr/bin/env python3
"""Generate the pack ABI's CipSoft object delegates (host API v9 cip_object table).

  gen_cip_objects.py CONTRACT_DIR OUT_DIR [--chunks N]

For EVERY class tools/cip/gen_cpp.py generates from the vendored contract
(client/upstream/contract), this emits a delegate subclass of the generated base
`cip::<CIP name>` that:

- overrides each public slot, Q_INVOKABLE and virtual property setter of the class
  and its generated ancestors whose argument and return types travel as JSON: the
  override asks the pack's QtClientCipObjectDelegateV1.invoke first and runs the
  generated (CipSoft default) implementation when the pack returns 0;
- writes a property's storage (m_<name>, read-only CipSoft properties included)
  for cip_object->set_value / set_object;
- runs the base implementation by name for cip_object->call_base.

Every class goes into one table (cip_objects_table.cpp) keyed by its CipSoft name;
the host side (client/tibia/mods/cip_object_abi.cpp) reads only that table. A new
CipSoft release regenerates it with cip_gen (make tibia-sync / make cip-absorb ->
the build reruns this with gen_cpp.py): no hand-written host code per class.

Host-implemented CipSoft classes (class_supported == 2) come from the same table:
cip_objects_host.cpp asks the trait `cip::HostImpl<Base>` (client/tibia/mods/
cip_host_classes.h, defaulting to cip_impl.h's CIP_IMPLEMENT bindings) for a host
implementation constructible from a Qt parent.

Outputs (OUT_DIR):
  cip_objects_NN.cpp     delegates + per-class functions, NN = 00..N-1
  cip_objects_table.cpp  the class table (name -> functions)
  cip_objects_host.cpp   host implementation factories (one per class, null when none)
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_cpp  # noqa: E402
from gen_cpp import CPP_KEYWORDS, HEADER, MACROS, QT_ENUMS, VALUE_TYPES, split_ns  # noqa: E402

HEADER = HEADER.replace("gen_cpp.py", "gen_cip_objects.py")

# Types the JSON bridge carries (cip_object_support.h): plain values, Qt value classes,
# CipSoft enums and Qt object pointers (as null).
JSON_CLASSES = {
    "QString", "QStringList", "QVariant", "QVariantMap", "QVariantList", "QUrl", "QColor", "QJsonObject",
    "QJsonArray", "QJsonValue", "QByteArray", "QList<int>", "QList<bool>", "QList<QString>", "QList<QUrl>",
    "QDate", "QDateTime", "QTime", "QPoint", "QPointF", "QSize", "QSizeF", "QRect", "QRectF", "QKeySequence",
    "QList<QObject*>",
}


def bare(t: str) -> str:
    t = t.strip()
    if t.startswith("const "):
        t = t[6:]
    return t.rstrip("&").strip()


def json_type(t: str) -> bool:
    t = bare(t)
    if t == "void" or t in VALUE_TYPES or t in JSON_CLASSES or t in QT_ENUMS:
        return True
    if t.endswith("*") and "<" not in t:
        return True
    # CipSoft enums resolve to ::cip::...::EName (or int when emitted earlier).
    return bool(re.fullmatch(r"(::)?[\w:]+", t)) and t.startswith("::cip::")


def ident(name: str) -> str:
    return re.sub(r"\W", "_", name)


class ObjGen:
    def __init__(self, contract: Path):
        self.g = gen_cpp.Gen(contract)
        order = self.g.order()
        emitted = {n["class"] for n in self.g.namespaces}
        self.members = {}
        for name in order:
            self.members[name] = self.g.class_members(name, emitted)
            emitted.add(name)
        self.order = order

    def chain(self, name):
        """The class and its generated ancestors, nearest first."""
        out = [name]
        out += [a for a in self.g.ancestors(name) if a in self.members]
        return out

    def collect(self, name):
        """Overridable methods and properties of the class and its ancestors. The nearest
        declaration wins; a name a nearer class declares (with any arguments, or as a
        getter / signal) hides the ancestor's overloads, as C++ name lookup does."""
        methods, props, seen_props = [], [], set()
        declared: dict[str, set] = {}
        for level in self.chain(name):
            m = self.members[level]
            for p, t in m["props"]:
                if p["name"] not in seen_props:
                    seen_props.add(p["name"])
                    props.append((level, p, t))
            entries = [(n, r, a, "method") for n, r, a in m["slots"] + m["invokables"]]
            entries += [(s_, "void", [(self.g.param(t_), "v")], "setter") for s_, t_ in m["setters"].items()]
            here: dict[str, set] = {}
            for n, r, a, kind in entries:
                sig = tuple(t for t, _ in a)
                here.setdefault(n, set()).add(sig)
                if n in declared:
                    continue
                methods.append((level, n, r, a, kind))
            for n in list(m["getters"]) + [s_ for s_, _, _ in m["signals"]]:
                here.setdefault(n, set()).add(None)
            for n, sigs in here.items():
                declared.setdefault(n, set()).update(sigs)
        return methods, props

    def emit(self, name, idx):
        g = self.g
        m = self.members[name]
        base = g.ref(name)
        dname = f"CipDelegate_{ident(g.cpp_path(name))}"
        ctor_parent = m["ctor_parent"]
        methods, props = self.collect(name)
        lines = [f"// {name}", f"class {dname} final : public {base}, public CipDelegateMixin", "{", "public:"]
        lines.append(f"    using Base = {base};")
        lines.append(f"    explicit {dname}(const QtClientCipObjectDelegateV1* d) : {base}(static_cast<{ctor_parent}*>(nullptr)), "
                     "CipDelegateMixin(d) {}")
        lines.append(f"    ~{dname}() override {{ cipDelegateDestroyed(this); }}")
        callbase = []
        for level, n, ret, args, kind in methods:
            types = [bare(t) for t, _ in args]
            if not all(json_type(t) for t in types) or not json_type(ret):
                continue
            params = ", ".join(f"{t} a{i}" for i, (t, _) in enumerate(args))
            names = ", ".join(f"a{i}" for i in range(len(args)))
            to_json = ", ".join(f"cipToJson(a{i})" for i in range(len(args)))
            lines.append(f"    {ret} {n}({params}) override")
            lines.append("    {")
            lines.append("        QJsonValue r;")
            if ret == "void":
                lines.append(f'        if (cipAskDelegate(this, this, "{n}", QJsonArray{{{to_json}}}, r)) return;')
                lines.append(f"        Base::{n}({names});")
            else:
                lines.append(f'        if (cipAskDelegate(this, this, "{n}", QJsonArray{{{to_json}}}, r)) return cipFromJson<{bare(ret)}>(r);')
                lines.append(f"        return Base::{n}({names});")
            lines.append("    }")
            conv = ", ".join(f"cipFromJson<{t}>(args.at({i}))" for i, t in enumerate(types))
            call = f"self->Base::{n}({conv})"
            body = f"{call}; ret = QJsonValue();" if ret == "void" else f"ret = cipToJson({call});"
            callbase.append(f'        if (name == "{n}" && args.size() == {len(args)}) {{ {body} return 0; }}')
        # call_base
        lines.append("    static int callBase(QObject* o, QByteArrayView name, const QJsonArray& args, QJsonValue& ret)")
        lines.append("    {")
        lines.append(f"        auto* self = dynamic_cast<{dname}*>(o);")
        lines.append("        if (!self) return -1;")
        lines += callbase
        lines.append("        Q_UNUSED(args); Q_UNUSED(ret); Q_UNUSED(name);")
        lines.append("        return -1;")
        lines.append("    }")
        # set_value / set_object: the property's storage
        sv, so = [], []
        for level, p, t in props:
            pname = p["name"]
            if t.startswith("QQmlListProperty"):
                continue
            member = f"self->m_{pname}"
            if t.endswith("*"):
                so.append(f'        if (name == "{pname}") {{ auto* v = qobject_cast<{t}>(value); if (value && !v) return -1; '
                          f'const bool changed = {member} != v; {member} = v; if (changed) cipEmitNotify(self, "{pname}"); return 0; }}')
                continue
            if not json_type(t):
                continue
            sv.append(f'        if (name == "{pname}") {{ bool ok = cipJsonFits<{t}>(value); if (!ok) return -1; auto v = cipFromJson<{t}>(value); '
                      f'const bool changed = !::cip::unchanged({member}, v); {member} = v; if (changed) cipEmitNotify(self, "{pname}"); return 0; }}')
        for fname, sig, body in (("setValue", "const QJsonValue& value", sv), ("setObject", "QObject* value", so)):
            lines.append(f"    static int {fname}(QObject* o, QByteArrayView name, {sig})")
            lines.append("    {")
            lines.append(f"        auto* self = dynamic_cast<{dname}*>(o);")
            lines.append("        if (!self) return -1;")
            lines += body
            lines.append("        Q_UNUSED(value); Q_UNUSED(name);")
            lines.append("        return -1;")
            lines.append("    }")
        lines.append(f"    static QObject* create(QObject* parent, const QtClientCipObjectDelegateV1* d)")
        lines.append("    {")
        lines.append(f"        auto* o = new {dname}(d);")
        lines.append("        o->QObject::setParent(parent);")
        lines.append("        return o;")
        lines.append("    }")
        lines.append("};")
        lines.append("")
        entry = (f'    {{ "{name}", "{split_ns(name)[1]}", &{base}::staticMetaObject, &{dname}::create, '
                 f"&{dname}::callBase, &{dname}::setValue, &{dname}::setObject }},")
        return "\n".join(lines), entry

    def write(self, out: Path, chunks: int):
        out.mkdir(parents=True, exist_ok=True)
        files = {}
        per = max(1, -(-len(self.order) // chunks))
        table_decls = []
        table_rows = []
        for i in range(chunks):
            part = self.order[i * per:(i + 1) * per]
            body = [HEADER, '#include "mods/cip_object_support.h"', "", "#include <cip_gen.h>", "",
                    "namespace tibia::mods::cipobjects {", "namespace {", ""]
            entries = []
            for name in part:
                text, entry = self.emit(name, i)
                body.append(text)
                entries.append(entry)
            body += ["} // namespace", ""]
            body.append(f"extern const CipClassInfo kChunk{i:02d}[] = {{")
            body += entries or ['    { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },']
            body.append("};")
            body.append(f"extern const size_t kChunk{i:02d}Size = {len(entries)};")
            body += ["", "} // namespace tibia::mods::cipobjects", ""]
            files[f"cip_objects_{i:02d}.cpp"] = "\n".join(body)
            table_decls.append(f"extern const CipClassInfo kChunk{i:02d}[];\nextern const size_t kChunk{i:02d}Size;")
            table_rows.append(f"    {{ kChunk{i:02d}, &kChunk{i:02d}Size }},")
        files["cip_objects_table.cpp"] = "\n".join([
            HEADER, '#include "mods/cip_object_support.h"', "", "namespace tibia::mods::cipobjects {", "",
            *table_decls, "", "const CipClassChunk kChunks[] = {", *table_rows, "};",
            f"const size_t kChunkCount = {chunks};", "", "} // namespace tibia::mods::cipobjects", ""])
        host = [HEADER, '#include "mods/cip_host_classes.h"', '#include "mods/cip_object_support.h"', "",
                "#include <cip_gen.h>", "", "namespace tibia::mods::cipobjects {", "",
                "const CipHostClass kHostClasses[] = {"]
        for name in self.order:
            host.append(f'    {{ "{name}", &cipMakeHost<{self.g.ref(name)}>, cipHasHost<{self.g.ref(name)}>() }},')
        host += ["};", f"const size_t kHostClassCount = {len(self.order)};", "",
                 "} // namespace tibia::mods::cipobjects", ""]
        files["cip_objects_host.cpp"] = "\n".join(host)
        for fname, text in files.items():
            path = out / fname
            if not path.is_file() or path.read_text() != text:
                path.write_text(text)
        print(f"[gen_cip_objects] {len(self.order)} CipSoft classes in {chunks} chunks")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("contract", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--chunks", type=int, default=12)
    args = ap.parse_args()
    ObjGen(args.contract).write(args.out, args.chunks)


if __name__ == "__main__":
    main()
