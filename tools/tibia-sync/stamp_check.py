"""Compare CIP stamp headers (h) to tip/launcher implementation stamps (cpp).

  stamp_check.py --report   # matched / diverged / missing / unknown
  stamp_check.py --check    # fail on diverged or unknown tip stamps

Missing catalog entries (declared in h, no cpp stamp) are the decompile backlog:
printed on --report, not a --check failure (incremental ports).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_PARENT = HERE.parents[1]
if (_PARENT / "apps" / "qtclient" / "client" / "tibia").is_dir():
    MONOREPO_ROOT = _PARENT
    QTCLIENT_ROOT = _PARENT / "apps" / "qtclient"
elif (_PARENT / "client" / "tibia").is_dir():
    QTCLIENT_ROOT = _PARENT
    _mono = _PARENT.parent.parent
    MONOREPO_ROOT = _mono if (_mono / "apps" / "qtclient").resolve() == QTCLIENT_ROOT.resolve() else QTCLIENT_ROOT
else:
    raise SystemExit(f"cannot locate qtclient from {HERE}")

sys.path.insert(0, str(HERE))
import funcmap  # noqa: E402

SCHEME = funcmap.SCHEME
STAMP_RE = funcmap.STAMP_RE

# void Class::method(  OR  Retns::Class::method(  OR  Class::method(
IMPL_RE = re.compile(
    r"^(?:[\w:<>,\s\*\&]+?\s+)?((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)::([A-Za-z_]\w*)\s*\("
)

SKIP_DIR_PARTS = {
    "idleot",
    "cip_ports",
    "third_party",
    "build",
    "cipgen",
    "upstream",
}


def log(msg: str) -> None:
    print(f"[stamp_check] {msg}", flush=True)


def parse_catalog(stamps_dir: Path) -> dict[str, str]:
    """Class::method → b1/hash from generated headers."""
    out: dict[str, str] = {}
    if not stamps_dir.is_dir():
        return out
    class_re = re.compile(r"^\s*class\s+(\w+)\b")
    # moc decls are not void-only (QString/bool/int getters, etc.).
    method_re = re.compile(
        r"(?:^|\s)(?:[\w:<>,\s\*\&]+?\s+)?(\w+)\s*\([^;]*\)\s*(?:const\s*)?;?\s*$"
    )
    for path in sorted(stamps_dir.rglob("*.h")):
        if path.name == "cip_stamps.h":
            continue
        class_name: str | None = None
        pending: str | None = None
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            stripped = line.strip()
            m = STAMP_RE.match(stripped)
            if m:
                pending = f"{SCHEME}/{m.group(1)}"
                continue
            cm = class_re.match(stripped)
            if cm:
                class_name = cm.group(1)
            if pending and class_name:
                mm = method_re.search(stripped)
                if mm and mm.group(1) not in {"if", "while", "for", "switch", "return", "class"}:
                    out[f"{class_name}::{mm.group(1)}"] = pending
                    pending = None
    return out


def parse_impl_stamps(roots: list[Path]) -> dict[str, tuple[str, Path]]:
    """Class::method → (b1/hash, path) from tip/launcher sources."""
    out: dict[str, tuple[str, Path]] = {}
    exts = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".mm"}
    for root in roots:
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if path.suffix.lower() not in exts or not path.is_file():
                continue
            parts = set(path.parts)
            if parts & SKIP_DIR_PARTS:
                continue
            pending: str | None = None
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for line in text.splitlines():
                stripped = line.strip()
                m = STAMP_RE.match(stripped)
                if m:
                    pending = f"{SCHEME}/{m.group(1)}"
                    continue
                if pending:
                    im = IMPL_RE.match(stripped)
                    if im:
                        cls = im.group(1).split("::")[-1]
                        method = im.group(2)
                        out[f"{cls}::{method}"] = (pending, path)
                        pending = None
                    elif stripped and not stripped.startswith("//") and not stripped.startswith("#"):
                        # Stamp not followed by a Class::method definition.
                        pending = None
    return out


def default_stamp_dirs() -> list[Path]:
    return [
        QTCLIENT_ROOT / "client" / "upstream" / "cip_stamps",
        QTCLIENT_ROOT / "launcher" / "upstream" / "cip_stamps",
    ]


def default_impl_roots() -> list[Path]:
    return [
        QTCLIENT_ROOT / "client" / "tibia",
        QTCLIENT_ROOT / "launcher" / "src",
    ]


def compare(
    catalog: dict[str, str],
    impl: dict[str, tuple[str, Path]],
) -> tuple[list[str], list[str], list[str], list[str]]:
    matched: list[str] = []
    diverged: list[str] = []
    missing: list[str] = []
    unknown: list[str] = []
    for key, h_hash in sorted(catalog.items()):
        if key not in impl:
            missing.append(f"{key}  {h_hash}")
            continue
        c_hash, path = impl[key]
        if c_hash == h_hash:
            matched.append(f"{key}  {h_hash}")
        else:
            diverged.append(f"{key}  h={h_hash}  cpp={c_hash}  ({path})")
    for key, (c_hash, path) in sorted(impl.items()):
        if key not in catalog:
            unknown.append(f"{key}  {c_hash}  ({path})")
    return matched, diverged, missing, unknown


def run_report(check: bool) -> int:
    catalog: dict[str, str] = {}
    for d in default_stamp_dirs():
        catalog.update(parse_catalog(d))
    impl = parse_impl_stamps(default_impl_roots())

    if not catalog and not impl:
        log("no cip_stamps/ and no tip stamps yet (run sync with --funcmap)")
        return 0

    matched, diverged, missing, unknown = compare(catalog, impl)
    log(f"catalog={len(catalog)} tip_stamps={len(impl)} "
        f"matched={len(matched)} diverged={len(diverged)} "
        f"missing={len(missing)} unknown={len(unknown)}")
    for label, rows in (
        ("diverged", diverged),
        ("unknown", unknown),
        ("missing", missing[:50]),  # backlog sample
    ):
        for row in rows:
            log(f"{label}: {row}")
    if len(missing) > 50:
        log(f"missing: … {len(missing) - 50} more (decompile backlog)")

    if check and (diverged or unknown):
        log("FAIL: tip stamps must match generated headers (fix diverged / drop unknown)")
        return 1
    if check:
        log("ok")
    return 0


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", action="store_true", help="print matched/diverged/missing/unknown")
    ap.add_argument("--check", action="store_true", help="CI: fail on diverged or unknown tip stamps")
    args = ap.parse_args()
    if not args.report and not args.check:
        args.report = True
    raise SystemExit(run_report(check=args.check))


if __name__ == "__main__":
    main()
