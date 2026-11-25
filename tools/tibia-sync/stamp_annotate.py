"""Insert or refresh // b1/<hash> comments on tip Class::method definitions.

Does not change function bodies. Catalog = upstream/cip_stamps/.

  python3 tools/tibia-sync/stamp_annotate.py           # dry-run
  python3 tools/tibia-sync/stamp_annotate.py --apply
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_PARENT = HERE.parents[1]
if (_PARENT / "apps" / "qtclient" / "client" / "tibia").is_dir():
    QTCLIENT_ROOT = _PARENT / "apps" / "qtclient"
elif (_PARENT / "client" / "tibia").is_dir():
    QTCLIENT_ROOT = _PARENT
else:
    raise SystemExit(f"cannot locate qtclient from {HERE}")

sys.path.insert(0, str(HERE))
import stamp_check  # noqa: E402

STAMP_RE = stamp_check.STAMP_RE
IMPL_RE = stamp_check.IMPL_RE
SCHEME = stamp_check.SCHEME


def log(msg: str) -> None:
    print(f"[stamp_annotate] {msg}", flush=True)


def annotate_file(path: Path, catalog: dict[str, str], apply: bool) -> tuple[int, int, int]:
    """Returns (inserted, updated, already_ok)."""
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines(keepends=True)
    inserted = updated = already = 0
    out: list[str] = []
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        # Existing stamp + following impl
        m = STAMP_RE.match(stripped)
        if m and i + 1 < len(lines):
            im = IMPL_RE.match(lines[i + 1].strip())
            if im:
                cls = im.group(1).split("::")[-1]
                method = im.group(2)
                key = f"{cls}::{method}"
                want = catalog.get(key)
                have = f"{SCHEME}/{m.group(1)}"
                if want is None:
                    out.append(line)
                    i += 1
                    continue
                if have == want:
                    already += 1
                    out.append(line)
                    i += 1
                    continue
                updated += 1
                indent = line[: len(line) - len(line.lstrip())]
                nl = "\n" if line.endswith("\n") else ""
                if apply:
                    out.append(f"{indent}// {want}{nl}")
                else:
                    out.append(line)
                i += 1
                continue

        im = IMPL_RE.match(stripped)
        if im:
            cls = im.group(1).split("::")[-1]
            method = im.group(2)
            key = f"{cls}::{method}"
            want = catalog.get(key)
            if want:
                # Skip if previous non-empty line is already the right stamp
                prev = out[-1].strip() if out else ""
                if STAMP_RE.match(prev):
                    out.append(line)
                    i += 1
                    continue
                inserted += 1
                indent = line[: len(line) - len(line.lstrip())]
                nl = "\n" if line.endswith("\n") else ""
                if apply:
                    out.append(f"{indent}// {want}{nl}")
                out.append(line)
                i += 1
                continue

        out.append(line)
        i += 1

    if apply and (inserted or updated):
        path.write_text("".join(out), encoding="utf-8")
    return inserted, updated, already


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--apply", action="store_true", help="write tip files")
    ap.add_argument(
        "--roots",
        nargs="*",
        type=Path,
        default=None,
        help="tip roots (default: client/tibia)",
    )
    args = ap.parse_args()

    catalog: dict[str, str] = {}
    for d in stamp_check.default_stamp_dirs():
        catalog.update(stamp_check.parse_catalog(d))
    if not catalog:
        log("no cip_stamps/ catalog — run tibia-sync with --funcmap first")
        raise SystemExit(1)

    roots = args.roots or [QTCLIENT_ROOT / "client" / "tibia"]
    tot_i = tot_u = tot_a = 0
    for root in roots:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*.cpp")):
            if "idleot" in path.parts:
                continue
            i, u, a = annotate_file(path, catalog, apply=args.apply)
            if i or u or a:
                log(f"{path.relative_to(QTCLIENT_ROOT)}: insert={i} update={u} ok={a}")
            tot_i += i
            tot_u += u
            tot_a += a
    log(
        f"total insert={tot_i} update={tot_u} ok={tot_a} "
        f"({'applied' if args.apply else 'dry-run'})"
    )


if __name__ == "__main__":
    main()
