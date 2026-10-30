#!/usr/bin/env python3
"""Build CIP class + QML obligation ledgers for the tibia / qtclient CIP clone.

Reads:
  - tmp/re/tibia/index/names.txt (RTTI/moc labels) OR --names
  - apps/qtclient/upstream/contract/qml_files.json
  - apps/qtclient/client/tibia/client/** handwritten subclasses
  - docs/reverse-engineering/qml-controllers-allowlist.txt

Writes (default under docs/reverse-engineering/ledgers/):
  - classes.json / classes.md
  - qml_controllers.json / qml_controllers.md

Status values for classes:
  ported | stub | missing | qtclient-only | out-of-scope

Exit 0 always when generating; use --check to fail CI on missing QML controllers
(same bar as check-qml-controllers.py), on the committed missing-class ceiling
(docs/reverse-engineering/ledgers/missing-ceiling.txt vs classes.json), on the
wired.json real-incomplete ceiling (entries with depth!=real), on the
behavioral-gaps.json open-count ceiling
(behavioral-incomplete-ceiling.txt), or on --fail-missing-classes
(strict missing==0).

--check is CI-safe for QML (no docs/ writes). The class ceiling is read from the
committed classes.json + missing-ceiling.txt (no names.txt required). The real-wire
ceiling is read from committed wired.json + real-incomplete-ceiling.txt. The
behavioral ceiling is read from committed behavioral-gaps.json +
behavioral-incomplete-ceiling.txt.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_NAMES = ROOT / "tmp/re/tibia/index/names.txt"
DEFAULT_QML = ROOT / "apps/qtclient/client/upstream/contract/qml_files.json"
DEFAULT_SRC = ROOT / "apps/qtclient/client/tibia"
DEFAULT_ALLOW = ROOT / "docs/reverse-engineering/qml-controllers-allowlist.txt"
DEFAULT_OUT = ROOT / "docs/reverse-engineering/ledgers"

# names.txt: "0xaddr ns::Class::method" or "0xaddr ns::…::TClass::vN"
NAME_LINE = re.compile(
    r"^0x[0-9a-fA-F]+\s+((?:[A-Za-z_][A-Za-z0-9_]*::)+)(T[A-Za-z0-9_]+)::"
)
OUT_OF_SCOPE = re.compile(
    r"^(tibia::)?(battleye|emac|anticheat|protobuf::|reporting::TBattl)",
    re.I,
)
TIBIA_NS = re.compile(r"qtclient", re.I)

# Only inheritance counts toward a declaration (`class TFoo :`). Brace-only
# `class TFoo {` is not a port. Prefer `public cip::…` bases when codegen exists.
CLASS_DECL = re.compile(
    r"\bclass\s+(T[A-Za-z0-9_]+)\s+(?:final\s*)?:"
)
CIP_BASE = re.compile(
    r"public\s+cip::((?:[A-Za-z0-9_]+::)*)(T[A-Za-z0-9_]+)\b"
)
DIALOG_BASE = re.compile(
    r"TDialogController\s*<\s*cip::((?:[A-Za-z0-9_]+::)*)(T[A-Za-z0-9_]+)\s*>"
)
# Thin placeholders under cip_ports/ are never "ported".
STUB_PATH = re.compile(r"(^|/)cip_ports/")
DEFAULT_CEILING = DEFAULT_OUT / "missing-ceiling.txt"
DEFAULT_WIRED = DEFAULT_OUT / "wired.json"
DEFAULT_REAL_CEILING = DEFAULT_OUT / "real-incomplete-ceiling.txt"
DEFAULT_BEHAVIORAL_GAPS = DEFAULT_OUT / "behavioral-gaps.json"
DEFAULT_BEHAVIORAL_CEILING = DEFAULT_OUT / "behavioral-incomplete-ceiling.txt"


def wired_structural_count(wired_path: Path) -> int:
    """Entries in wired.json that are not yet depth=real."""
    data = json.loads(wired_path.read_text(encoding="utf-8"))
    entries = data.get("wired") or []
    return sum(1 for e in entries if e.get("depth", "structural") != "real")


def behavioral_open_count(gaps_path: Path) -> int:
    """Count behavioral-gaps.json entries with status=open."""
    data = json.loads(gaps_path.read_text(encoding="utf-8"))
    gaps = data.get("gaps") or []
    return sum(1 for g in gaps if g.get("status") == "open")


def read_ceiling_int(path: Path) -> int:
    """First integer token before optional # comment."""
    return int(path.read_text(encoding="utf-8").split("#", 1)[0].strip().split()[0])


def cip_fqn(ns: str, cls: str) -> str:
    """cip::tibia::gamewindow::TFoo → tibia::gamewindow::TFoo (no double tibia::)."""
    ns = ns.rstrip(":")
    if ns.startswith("tibia::"):
        return f"{ns}::{cls}"
    if ns:
        return f"tibia::{ns}::{cls}"
    return f"tibia::{cls}"


def load_names(path: Path, *, required: bool = True) -> set[str] | None:
    """Return tibia::<ns>::<TClass> FQNs from Ghidra label dump.

    When required=False and the file is missing, returns None (CI --check path).
    """
    if not path.is_file():
        if required:
            raise SystemExit(f"missing names file: {path}")
        return None
    out: set[str] = set()
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = NAME_LINE.match(raw)
        if not m:
            continue
        ns = m.group(1).rstrip(":")
        cls = m.group(2)
        # Skip nested nonsense like config::TConfigBackupReader::Something
        if "::" in cls:
            continue
        fqn = f"tibia::{ns}::{cls}" if not ns.startswith("tibia::") else f"{ns}::{cls}"
        # Normalize double tibia::
        fqn = fqn.replace("tibia::tibia::", "tibia::")
        out.add(fqn)
    return out


def load_handwritten(src: Path) -> dict:
    """Map short class name -> path; also cip-base FQNs found via public cip::."""
    by_short: dict[str, str] = {}
    cip_bases: set[str] = set()
    for path in src.rglob("*"):
        if path.suffix not in {".h", ".hpp", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        rel = str(path.relative_to(src))
        for m in CLASS_DECL.finditer(text):
            by_short[m.group(1)] = rel
        for m in CIP_BASE.finditer(text):
            cip_bases.add(cip_fqn(m.group(1), m.group(2)))
        for m in DIALOG_BASE.finditer(text):
            cip_bases.add(cip_fqn(m.group(1), m.group(2)))
    return {"by_short": by_short, "cip_bases": cip_bases}


def classify_class(fqn: str, handwritten: dict) -> str:
    if OUT_OF_SCOPE.search(fqn):
        return "out-of-scope"
    short = fqn.rsplit("::", 1)[-1]
    by_short: dict[str, str] = handwritten["by_short"]  # type: ignore[assignment]
    cip_bases: set[str] = handwritten["cip_bases"]  # type: ignore[assignment]
    path = by_short.get(short, "")
    # Thin cip_ports/ placeholders stay visible as stub — not ported.
    if path and STUB_PATH.search(path.replace("\\", "/")):
        return "stub"
    if fqn in cip_bases or short in by_short:
        # qtclient/ path → product island even if named like CIP
        if TIBIA_NS.search(path) or TIBIA_NS.search(fqn):
            return "qtclient-only" if fqn not in cip_bases else "ported"
        return "ported"
    return "missing"


def incomplete_count(counts: dict) -> int:
    """Classes not yet done: missing + stub (ceiling tracks this)."""
    return int(counts.get("missing", 0)) + int(counts.get("stub", 0))


def load_qml_controllers(path: Path) -> set[str]:
    data = json.loads(path.read_text(encoding="utf-8"))
    out: set[str] = set()
    for meta in data.values():
        for entry in meta.get("controllers") or []:
            if isinstance(entry, dict):
                cls = entry.get("class") or ""
                if cls.startswith("tibia::") and cls.endswith("Controller"):
                    out.add(cls)
    return out


def load_allowlist(path: Path) -> set[str]:
    if not path.is_file():
        return set()
    out: set[str] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line.startswith("tibia::"):
            out.add(line.split()[0])
    return out


def write_class_ledger(classes: set[str], handwritten: dict, out_dir: Path) -> dict:
    rows = []
    counts: dict[str, int] = defaultdict(int)
    for fqn in sorted(classes):
        status = classify_class(fqn, handwritten)
        counts[status] += 1
        short = fqn.rsplit("::", 1)[-1]
        rows.append(
            {
                "class": fqn,
                "status": status,
                "source": handwritten["by_short"].get(short),  # type: ignore[index]
            }
        )
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "total": len(rows),
        "counts": dict(counts),
        "classes": rows,
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "classes.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# CIP class obligation ledger",
        "",
        f"Generated `{payload['generated_at']}`. Total **{payload['total']}** classes from RTTI labels.",
        "",
        "| Status | Count |",
        "|--------|------:|",
    ]
    for k in ("ported", "stub", "missing", "qtclient-only", "out-of-scope"):
        lines.append(f"| {k} | {counts.get(k, 0)} |")
    lines += [
        "",
        f"Incomplete (missing+stub, CI ceiling): **{incomplete_count(counts)}**",
        "",
        "## Missing (sample first 40)",
        "",
    ]
    missing = [r["class"] for r in rows if r["status"] == "missing"]
    for name in missing[:40]:
        lines.append(f"- `{name}`")
    if len(missing) > 40:
        lines.append(f"- … +{len(missing) - 40} more (see classes.json)")
    stubs = [r["class"] for r in rows if r["status"] == "stub"]
    lines += ["", "## Stub / thin placeholders (sample first 40)", ""]
    for name in stubs[:40]:
        lines.append(f"- `{name}`")
    if len(stubs) > 40:
        lines.append(f"- … +{len(stubs) - 40} more (see classes.json / cip_ports/)")
    lines.append("")
    (out_dir / "classes.md").write_text("\n".join(lines), encoding="utf-8")
    return payload


def write_qml_ledger(
    controllers: set[str], handwritten: dict, allow: set[str], out_dir: Path
) -> dict:
    by_short: dict[str, str] = handwritten["by_short"]  # type: ignore[assignment]
    cip_bases: set[str] = handwritten["cip_bases"]  # type: ignore[assignment]
    rows = []
    counts: dict[str, int] = defaultdict(int)
    for cls in sorted(controllers):
        short = cls.rsplit("::", 1)[-1]
        path = by_short.get(short, "")
        if path and STUB_PATH.search(path.replace("\\", "/")):
            status = "allowlisted" if cls in allow else "missing"
        elif cls in cip_bases or short in by_short:
            status = "ported"
        elif cls in allow:
            status = "allowlisted"
        else:
            status = "missing"
        counts[status] += 1
        rows.append({"class": cls, "status": status, "source": by_short.get(short)})
    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "total": len(rows),
        "counts": dict(counts),
        "controllers": rows,
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "qml_controllers.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8"
    )
    lines = [
        "# CIP QML controller obligation ledger",
        "",
        f"Generated `{payload['generated_at']}`. Controllers referenced by vendored QML: **{payload['total']}**.",
        "",
        "| Status | Count |",
        "|--------|------:|",
    ]
    for k in ("ported", "allowlisted", "missing"):
        lines.append(f"| {k} | {counts.get(k, 0)} |")
    missing = [r["class"] for r in rows if r["status"] == "missing"]
    if missing:
        lines += ["", "## Missing", ""]
        for name in missing:
            lines.append(f"- `{name}`")
    lines.append("")
    (out_dir / "qml_controllers.md").write_text("\n".join(lines), encoding="utf-8")
    return payload


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--names", type=Path, default=DEFAULT_NAMES)
    p.add_argument("--qml-files", type=Path, default=DEFAULT_QML)
    p.add_argument("--src", type=Path, default=DEFAULT_SRC)
    p.add_argument("--allowlist", type=Path, default=DEFAULT_ALLOW)
    p.add_argument("--out", type=Path, default=DEFAULT_OUT)
    p.add_argument(
        "--check",
        action="store_true",
        help="exit 1 if any QML controller is missing (not allowlisted), or if "
        "committed classes.json incomplete (missing+stub) exceeds missing-ceiling.txt, "
        "wired structural exceeds real-incomplete-ceiling, or open behavioral gaps "
        "exceed behavioral-incomplete-ceiling; does not write docs/",
    )
    p.add_argument(
        "--fail-missing-classes",
        action="store_true",
        help="exit 1 if class ledger has incomplete entries (missing+stub != 0)",
    )
    p.add_argument(
        "--ceiling",
        type=Path,
        default=DEFAULT_CEILING,
        help="committed max allowed missing classes (default: ledgers/missing-ceiling.txt)",
    )
    p.add_argument(
        "--real-ceiling",
        type=Path,
        default=DEFAULT_REAL_CEILING,
        help="committed max allowed wired.json structural entries "
        "(default: ledgers/real-incomplete-ceiling.txt)",
    )
    p.add_argument(
        "--wired",
        type=Path,
        default=DEFAULT_WIRED,
        help="committed wired.json (depth tracking)",
    )
    p.add_argument(
        "--behavioral-gaps",
        type=Path,
        default=DEFAULT_BEHAVIORAL_GAPS,
        help="committed behavioral-gaps.json",
    )
    p.add_argument(
        "--behavioral-ceiling",
        type=Path,
        default=DEFAULT_BEHAVIORAL_CEILING,
        help="committed max allowed open behavioral gaps "
        "(default: ledgers/behavioral-incomplete-ceiling.txt)",
    )
    args = p.parse_args()

    handwritten = load_handwritten(args.src)
    controllers = load_qml_controllers(args.qml_files)
    allow = load_allowlist(args.allowlist)

    # --check must not dirty the working tree; write under a temp dir if needed.
    out_dir = args.out
    tmp_ctx = None
    if args.check and not args.fail_missing_classes:
        tmp_ctx = tempfile.TemporaryDirectory(prefix="cip-ledger-")
        out_dir = Path(tmp_ctx.name)

    # Regenerate class ledger when names.txt is available (generate path, or
    # strict fail-missing). --check ceiling uses committed classes.json.
    need_classes = (not args.check) or args.fail_missing_classes
    class_payload: dict | None = None
    if need_classes:
        classes = load_names(args.names, required=True)
        assert classes is not None
        class_payload = write_class_ledger(classes, handwritten, out_dir)
        print(
            f"classes: total={class_payload['total']} "
            + " ".join(f"{k}={v}" for k, v in sorted(class_payload["counts"].items()))
        )
    else:
        print("classes: skipped regenerate (--check uses committed classes.json for ceiling)")

    qml_payload = write_qml_ledger(controllers, handwritten, allow, out_dir)
    print(
        f"qml-controllers: total={qml_payload['total']} "
        + " ".join(f"{k}={v}" for k, v in sorted(qml_payload["counts"].items()))
    )
    if tmp_ctx is None:
        print(f"wrote {out_dir}")
    else:
        print("check: no docs/ writes")

    rc = 0
    if args.check and qml_payload["counts"].get("missing", 0):
        print("error: missing QML controllers (see qml_controllers.md)", file=sys.stderr)
        rc = 1
    if args.check:
        committed = args.out / "classes.json"
        incomplete = -1
        if class_payload is not None:
            incomplete = incomplete_count(class_payload["counts"])
        elif committed.is_file():
            try:
                incomplete = incomplete_count(
                    json.loads(committed.read_text(encoding="utf-8")).get("counts", {})
                )
            except (json.JSONDecodeError, TypeError, ValueError) as exc:
                print(f"error: cannot read {committed}: {exc}", file=sys.stderr)
                rc = 1
        else:
            print(f"error: missing committed class ledger {committed}", file=sys.stderr)
            rc = 1
        if incomplete >= 0:
            if not args.ceiling.is_file():
                print(f"error: missing class ceiling file {args.ceiling}", file=sys.stderr)
                rc = 1
            else:
                try:
                    ceiling = read_ceiling_int(args.ceiling)
                except (ValueError, IndexError) as exc:
                    print(f"error: bad ceiling in {args.ceiling}: {exc}", file=sys.stderr)
                    rc = 1
                    ceiling = -1
                if ceiling >= 0 and incomplete > ceiling:
                    print(
                        f"error: incomplete CIP classes {incomplete} > ceiling {ceiling} "
                        f"(missing+stub; lower missing-ceiling.txt only after real ports land)",
                        file=sys.stderr,
                    )
                    rc = 1
                else:
                    print(f"classes-ceiling: incomplete={incomplete} ceiling={ceiling}")
        # Real-wire depth ratchet (structural keep-alives must not grow).
        if not args.wired.is_file():
            print(f"error: missing wired ledger {args.wired}", file=sys.stderr)
            rc = 1
        elif not args.real_ceiling.is_file():
            print(f"error: missing real-wire ceiling {args.real_ceiling}", file=sys.stderr)
            rc = 1
        else:
            try:
                structural = wired_structural_count(args.wired)
                real_ceiling = read_ceiling_int(args.real_ceiling)
            except (json.JSONDecodeError, TypeError, ValueError, IndexError) as exc:
                print(f"error: cannot read wired/real ceiling: {exc}", file=sys.stderr)
                rc = 1
            else:
                if structural > real_ceiling:
                    print(
                        f"error: structural wires {structural} > real-incomplete ceiling "
                        f"{real_ceiling} (lower real-incomplete-ceiling.txt only after "
                        f"depth=real promotions)",
                        file=sys.stderr,
                    )
                    rc = 1
                else:
                    print(f"real-wire-ceiling: structural={structural} ceiling={real_ceiling}")
        # Behavioral / multi-surface gap ratchet.
        if not args.behavioral_gaps.is_file():
            print(f"error: missing behavioral gaps {args.behavioral_gaps}", file=sys.stderr)
            rc = 1
        elif not args.behavioral_ceiling.is_file():
            print(
                f"error: missing behavioral ceiling {args.behavioral_ceiling}",
                file=sys.stderr,
            )
            rc = 1
        else:
            try:
                open_gaps = behavioral_open_count(args.behavioral_gaps)
                behavioral_ceiling = read_ceiling_int(args.behavioral_ceiling)
            except (json.JSONDecodeError, TypeError, ValueError, IndexError) as exc:
                print(f"error: cannot read behavioral gaps/ceiling: {exc}", file=sys.stderr)
                rc = 1
            else:
                if open_gaps > behavioral_ceiling:
                    print(
                        f"error: open behavioral gaps {open_gaps} > ceiling "
                        f"{behavioral_ceiling} (lower behavioral-incomplete-ceiling.txt "
                        f"only after gaps are fixed or marked wontfix/out-of-scope)",
                        file=sys.stderr,
                    )
                    rc = 1
                else:
                    print(
                        f"behavioral-ceiling: open={open_gaps} ceiling={behavioral_ceiling}"
                    )
    if args.fail_missing_classes:
        if class_payload is None:
            print("error: --fail-missing-classes needs names.txt", file=sys.stderr)
            rc = 1
        elif incomplete_count(class_payload["counts"]):
            print("error: incomplete CIP classes (missing+stub; see classes.md)", file=sys.stderr)
            rc = 1
    if tmp_ctx is not None:
        tmp_ctx.cleanup()
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
