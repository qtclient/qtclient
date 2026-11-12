#!/usr/bin/env python3
"""Build the typed C++ contract CIP QML expects, from qt_moc.py output.

Usage: qml_contract.py MOC.json QML_DIR OUT_DIR

Writes:
  OUT_DIR/elements.json     QML.Element/Singleton name -> class + flattened API
  OUT_DIR/classes/<ns>.json typed API per tibia:: namespace (inherited members merged)
  OUT_DIR/qml_files.json    per QML file: elements used, controller classes matched
                            by member overlap, unresolved member names
"""

from __future__ import annotations
import argparse
import collections
import json
import math
import os
import re

QT_BASES = {"QObject", "QQuickItem", "QQuickPaintedItem", "QAbstractListModel",
            "QAbstractItemModel", "QAbstractTableModel", "QSortFilterProxyModel",
            "QAbstractProxyModel", "QIdentityProxyModel", "QQuickImageProvider"}


def short(name):
    return name.split("::")[-1]


def load_classes(path):
    raw = json.load(open(path))
    by_full = {c["class"]: c for c in raw}
    by_short = collections.defaultdict(list)
    for c in raw:
        by_short[short(c["class"])].append(c)
    return raw, by_full, by_short


def resolve_super(c, by_full, by_short):
    s = c.get("superclass")
    if not s or s in QT_BASES:
        return None
    if s in by_full:
        return by_full[s]
    cands = by_short.get(short(s), [])
    return cands[0] if len(cands) == 1 else None


def flatten(c, by_full, by_short):
    api = {"class": c["class"], "chain": [], "classinfo": {}, "properties": {},
           "signals": {}, "slots": {}, "invokables": {}, "enums": {}}
    seen = set()
    cur = c
    while cur and cur["class"] not in seen:
        seen.add(cur["class"])
        api["chain"].append(cur["class"])
        for k, v in cur.get("classinfo", {}).items():
            api["classinfo"].setdefault(k, v)
        for p in cur.get("properties", []):
            api["properties"].setdefault(p["name"], {
                "type": p["type"], "flags": p.get("flags", []), "notify": p.get("notify"),
                "from": short(cur["class"])})
        for kind in ("signals", "slots", "invokables"):
            for m in cur.get(kind, []):
                sig = "%s %s(%s)" % (m.get("return", "void"), m["name"], ", ".join(
                    ("%s %s" % (a.get("type", "?"), a.get("name", ""))).strip() for a in m.get("args", [])))
                api[kind].setdefault(m["name"], []).append(sig)
        for e in cur.get("enums", []):
            api["enums"].setdefault(e["name"], e.get("values") or e.get("keys"))
        last = cur
        cur = resolve_super(cur, by_full, by_short)
        if cur is None and last.get("superclass"):
            api["chain"].append(last["superclass"])
    return api


def member_names(api):
    names = set(api["properties"]) | set(api["signals"]) | set(api["slots"]) | set(api["invokables"])
    for vals in api["enums"].values():
        if isinstance(vals, dict):
            names |= set(vals)
        elif isinstance(vals, list):
            names |= set(vals)
    return names


DOT_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\.\s*([A-Za-z_]\w*)")
TYPE_RE = re.compile(r"^\s*([A-Z]\w*)\s*\{", re.M)
JS_NOISE = {"length", "push", "x", "y", "width", "height", "visible", "text", "model", "count",
            "index", "parent", "left", "right", "top", "bottom", "fill", "anchors", "color",
            "source", "enabled", "checked", "checked", "modelData", "font", "pixelSize",
            "bold", "family", "horizontalCenter", "verticalCenter", "centerIn", "connect",
            "disconnect", "toString", "indexOf", "forEach", "map", "slice", "max", "min",
            "floor", "ceil", "round", "abs", "arg", "replace", "split", "join", "contentItem",
            "implicitWidth", "implicitHeight", "opacity", "z", "item", "active", "status",
            "running", "start", "stop", "restart", "forceActiveFocus", "focus", "hovered",
            "pressed", "containsMouse", "mouseX", "mouseY", "currentIndex", "name", "value",
            "id", "key", "data", "type", "state", "size", "title", "Ready", "Error"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("moc")
    ap.add_argument("qml_dir")
    ap.add_argument("out_dir")
    a = ap.parse_args()

    raw, by_full, by_short = load_classes(a.moc)
    flat = {c["class"]: flatten(c, by_full, by_short) for c in raw}
    os.makedirs(os.path.join(a.out_dir, "classes"), exist_ok=True)

    elements = {}
    for full, api in flat.items():
        ci = api["classinfo"]
        own = by_full[full].get("classinfo", {})
        if "QML.Element" in own:
            elements[own["QML.Element"]] = dict(api, singleton="QML.Singleton" in own,
                                               creatable=own.get("QML.Creatable", "true") != "false",
                                               uncreatable_reason=own.get("QML.UncreatableReason"))
    json.dump(elements, open(os.path.join(a.out_dir, "elements.json"), "w"), indent=1, sort_keys=True)

    per_ns = collections.defaultdict(dict)
    for full, api in flat.items():
        parts = full.split("::")
        ns = parts[1] if len(parts) >= 3 and parts[0] == "tibia" else (parts[0] if len(parts) > 1 else "_global")
        per_ns[ns][full] = api
    for ns, classes in per_ns.items():
        json.dump(classes, open(os.path.join(a.out_dir, "classes", ns + ".json"), "w"), indent=1, sort_keys=True)

    names = {full: member_names(api) for full, api in flat.items()}
    df = collections.Counter(n for s in names.values() for n in s)
    total = len(names)

    def idf(n):
        return math.log((total + 1) / (df[n] + 1)) + 0.1

    def match(members, limit=4):
        scored = []
        for full, s in names.items():
            hit = members & s
            if len(hit) < 2:
                continue
            score = sum(idf(n) for n in hit)
            scored.append((score, full, sorted(hit)))
        scored.sort(reverse=True)
        out, covered = [], set()
        for score, full, hit in scored:
            new = set(hit) - covered
            if sum(idf(n) for n in new) < 3.0:
                continue
            out.append({"class": full, "score": round(score, 1), "matched": len(hit)})
            covered |= set(hit)
            if len(out) >= limit:
                break
        return out, covered

    all_member_names = set(df)
    element_names = set(elements)
    qml_files = {}
    for root, _, files in os.walk(a.qml_dir):
        for f in sorted(files):
            if not f.endswith(".qml"):
                continue
            src = open(os.path.join(root, f), encoding="utf-8", errors="replace").read()
            dotted = collections.defaultdict(set)
            for obj, mem in DOT_RE.findall(src):
                dotted[obj].add(mem)
            used_types = sorted((set(TYPE_RE.findall(src)) | set(dotted)) & element_names)
            members = {m for obj, ms in dotted.items() for m in ms} - JS_NOISE
            cpp_members = members & all_member_names
            matches, covered = match(cpp_members)
            qml_files[os.path.relpath(os.path.join(root, f), a.qml_dir)] = {
                "elements": used_types,
                "controllers": matches,
                "type_refs": sorted(o for o in dotted if o[0].isupper() and o not in element_names
                                    and (o.endswith("Controller") or o.endswith("Model"))),
                "uncovered_cpp_members": sorted(cpp_members - covered),
            }
    json.dump(qml_files, open(os.path.join(a.out_dir, "qml_files.json"), "w"), indent=1, sort_keys=True)

    summary = {"classes": len(raw), "elements": len(elements), "qml_files": len(qml_files),
               "qml_files_with_controller": sum(1 for v in qml_files.values() if v["controllers"])}

    print(json.dumps(summary))


if __name__ == "__main__":
    main()
