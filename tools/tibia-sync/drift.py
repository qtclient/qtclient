"""Compare two vendored upstream/ trees and write DRIFT.md."""

from __future__ import annotations

import difflib
import hashlib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "re"))
import qt_qm  # noqa: E402

OVERRIDE_BASE = re.compile(r"^//\s*cip-base:\s*([0-9a-f]{64})", re.M)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree(root: Path, sub: str, pattern="*") -> dict[str, str]:
    base = root / sub
    if not base.is_dir():
        return {}
    return {p.relative_to(base).as_posix(): sha256(p) for p in sorted(base.rglob(pattern)) if p.is_file()}


def load_json(path: Path, default):
    return json.loads(path.read_text()) if path.is_file() else default


def members(cls: dict) -> set[str]:
    out = set()
    for name, p in cls.get("properties", {}).items():
        out.add(f"property {p.get('type', '?')} {name}")
    for kind in ("signals", "slots", "invokables"):
        for sigs in cls.get(kind, {}).values():
            out.update(f"{kind[:-1]} {s}" for s in sigs)
    enums = cls.get("enums", {})
    if isinstance(enums, list):
        enums = {e["name"]: e["values"] for e in enums}
    for name, values in enums.items():
        out.update(f"enum {name}::{k}={v}" for k, v in values.items())
    return out


def contract_classes(root: Path) -> dict[str, dict]:
    classes = {}
    for f in sorted((root / "contract/classes").glob("*.json")):
        classes.update(load_json(f, {}))
    return classes


def qm_strings(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    buf = path.read_bytes()
    pos, sections = 16, {}
    while pos < len(buf):
        tag = buf[pos]
        n = int.from_bytes(buf[pos + 1 : pos + 5], "big")
        sections[tag] = buf[pos + 5 : pos + 5 + n]
        pos += 5 + n
    out = {}
    for m in qt_qm.parse_messages(sections.get(qt_qm.TAG_MESSAGES, b"")):
        tr = m.get("translation") or []
        out[m.get("source", "")] = tr[0] if len(tr) == 1 else json.dumps(tr)
    return out


def keyed_diff(old: dict, new: dict):
    added = sorted(set(new) - set(old))
    removed = sorted(set(old) - set(new))
    changed = sorted(k for k in set(old) & set(new) if old[k] != new[k])
    return added, removed, changed


def section(lines, title, added, removed, changed=(), limit=60):
    if not (added or removed or changed):
        return
    lines.append(f"## {title}\n")
    for label, items in (("Added", added), ("Removed", removed), ("Changed", changed)):
        if not items:
            continue
        lines.append(f"**{label} ({len(items)})**\n")
        lines += [f"- `{i}`" for i in list(items)[:limit]]
        if len(items) > limit:
            lines.append(f"- … {len(items) - limit} more")
        lines.append("")


def _override_base_file(upstream: Path, rel: str) -> Path | None:
    """Resolve the file an override's cip-base must match.

    CIP stock forks live under upstream/qml/. Engine Qt forks under
    QtQuick/LegacyControls/ are based on third_party/Qt5LegacyControls
    (not carved into upstream/).
    """
    if rel.startswith("QtQuick/LegacyControls/"):
        # upstream = …/client/upstream → repo root is parents[1]
        qt5lc = upstream.parent.parent / "third_party" / "Qt5LegacyControls" / rel
        return qt5lc if qt5lc.is_file() else None
    target = upstream / "qml" / rel
    return target if target.is_file() else None


def overrides_report(new: Path, overrides: Path | list[Path]):
    """Check cip-base shas for engine + pack override trees.

    Each overrides root mirrors paths under upstream/qml/ (e.g.
    qmlcomponents/qml/Foo.qml) or QtQuick/LegacyControls/ (Qt5LegacyControls).
    """
    stale, orphan = [], []
    roots = overrides if isinstance(overrides, list) else [overrides]
    for overrides_root in roots:
        if not overrides_root.is_dir():
            continue
        for f in sorted(overrides_root.rglob("*.qml")):
            rel = f.relative_to(overrides_root).as_posix()
            label = f"{overrides_root.name}/{rel}" if len(roots) > 1 else rel
            # Prefer path under overrides_root parent for pack roots named overrides/
            if overrides_root.name == "overrides":
                pack = overrides_root.parent.name
                label = f"{pack}/overrides/{rel}"
            m = OVERRIDE_BASE.search(f.read_text(errors="replace"))
            target = _override_base_file(new, rel)
            if target is None:
                orphan.append(label)
            elif not m or m.group(1) != sha256(target):
                stale.append(label)
    return stale, orphan


def report(old: Path, new: Path, overrides: Path) -> tuple[str, bool]:
    old_v = load_json(old / "VERSION.json", {})
    new_v = load_json(new / "VERSION.json", {})
    lines = [
        "# CIP drift",
        "",
        f"From `{old_v.get('cip_version', 'none')}` ({old_v.get('client_sha256', '-')[:12]}) "
        f"to `{new_v.get('cip_version', '?')}` ({new_v.get('client_sha256', '-')[:12]}).",
        "",
    ]
    start = len(lines)

    for sub, title in (("qml", "QML files"), ("shaders", "Shaders"), ("conf", "Default configuration")):
        section(lines, title, *keyed_diff(tree(old, sub), tree(new, sub)))

    old_cls, new_cls = contract_classes(old), contract_classes(new)
    added_cls, removed_cls, _ = keyed_diff(old_cls, new_cls)
    section(lines, "Contract classes", added_cls, removed_cls)
    member_changes = []
    for name in sorted(set(old_cls) & set(new_cls)):
        a, b = members(old_cls[name]), members(new_cls[name])
        member_changes += [f"{name}: + {m}" for m in sorted(b - a)]
        member_changes += [f"{name}: - {m}" for m in sorted(a - b)]
    section(lines, "Contract members", [], [], member_changes, limit=200)

    old_el, new_el = load_json(old / "contract/elements.json", {}), load_json(new / "contract/elements.json", {})
    section(lines, "QML elements", *keyed_diff({k: v.get("class") for k, v in old_el.items()},
                                              {k: v.get("class") for k, v in new_el.items()}))

    section(lines, "QML enum scopes (qmlenumvalues)",
            *keyed_diff(load_json(old / "contract/qml_registrations.json", {}).get("enum_scopes", {}),
                        load_json(new / "contract/qml_registrations.json", {}).get("enum_scopes", {})))

    old_opt = load_json(old / "contract/option_ids.json", {})
    new_opt = load_json(new / "contract/option_ids.json", {})
    added, removed, changed = keyed_diff(old_opt, new_opt)
    section(lines, "Option ids", [f"{k} {new_opt[k]}" for k in added], [f"{k} {old_opt[k]}" for k in removed],
            [f"{k} {old_opt[k]} -> {new_opt[k]}" for k in changed])

    def messages(root):
        return {f"{m['dir']} {m['opcode']:#04x}": "/".join(m.get("tibia", [])) for m in load_json(root / "protocol/messages.json", [])}

    if (new / "protocol/messages.json").is_file():
        section(lines, "Protocol messages", *keyed_diff(messages(old), messages(new)))

    proto_old, proto_new = tree(old, "proto"), tree(new, "proto")
    added, removed, changed = keyed_diff(proto_old, proto_new)
    section(lines, "Protobuf schemas", added, removed, changed)
    for name in changed:
        diff = difflib.unified_diff((old / "proto" / name).read_text().splitlines(),
                                    (new / "proto" / name).read_text().splitlines(), name, name, lineterm="", n=1)
        lines += ["```diff", *list(diff)[:120], "```", ""]

    section(lines, "Strings (client.en.qm)", *keyed_diff(qm_strings(old / "strings/client.en.qm"),
                                                         qm_strings(new / "strings/client.en.qm")), limit=40)

    stale, orphan = overrides_report(new, overrides)
    if stale or orphan:
        lines.append("## QML overrides to rebase\n")
        lines += [f"- `{o}`: CIP base changed (update the copy, then its `cip-base` sha)" for o in stale]
        lines += [f"- `{o}`: CIP file no longer exists" for o in orphan]
        lines.append("")

    changed_any = len(lines) > start
    if not changed_any:
        lines.append("No changes.")
    return "\n".join(lines).rstrip() + "\n", changed_any
