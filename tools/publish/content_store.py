"""Content-addressed CDN store shared by the desktop, browser and assets slices.

Blobs live at <out>/content/<sha[0:2]>/<sha256>, zip packs of the same files at
<out>/packs/<sha[0:2]>/<sha256>.zip, file lists at <out>/.qtclient/v3/<name>.json:

  {"schema": 3, "revision", "engine", "entry"?,
   "files": {"/<path>": {"sha256", "size", "exec"?}},
   "packs": [{"sha256", "size", "files": ["/<path>", ...]}]}

Packs make cold installs a few large downloads instead of thousands of small
ones; blobs stay for deltas and repair. Pack bytes are deterministic, so an
unchanged group of files keeps its pack across releases.

qtclient/release (scripts/assemble.py) copies the lists to releases/<revision>/
and points latest/release.json at them; packages/client-sync downloads by hash.
"""
from __future__ import annotations

import hashlib
import io
import json
import os
import stat
import zipfile
from pathlib import Path

SCHEMA = 3
PACK_TARGET = 16 * 1024 * 1024
# Already compressed: deflate would only burn CPU on both ends.
STORED_SUFFIXES = {".lzma", ".png", ".jpg", ".jpeg", ".webp", ".gif", ".wasm", ".gz", ".zip", ".ogg", ".mp3"}


def blob_path(store: Path, digest: str) -> Path:
    return store / digest[:2] / digest


def put_blob(store: Path, data: bytes) -> tuple[str, int]:
    digest = hashlib.sha256(data).hexdigest()
    dest = blob_path(store, digest)
    if not dest.is_file():
        dest.parent.mkdir(parents=True, exist_ok=True)
        tmp = dest.with_suffix(".tmp")
        tmp.write_bytes(data)
        os.replace(tmp, dest)
    return digest, len(data)


def entry(store: Path, data: bytes, executable: bool = False) -> dict:
    digest, size = put_blob(store, data)
    item: dict = {"sha256": digest, "size": size}
    if executable:
        item["exec"] = True
    return item


def tree_entries(store: Path, root: Path, prefix: str = "", include: list[str] | None = None,
                 skip_hidden: bool = False, skip_symlinks: bool = False) -> dict[str, dict]:
    """Store every regular file under root; keys are "/<prefix>/<relative path>".

    include limits the walk to those top-level names; skip_hidden drops dot-files/dirs.
    skip_symlinks drops symlinks (the launcher only writes files), e.g. the
    Current/top-level aliases of a macOS framework, which loads by Versions/A.
    """
    files: dict[str, dict] = {}
    tops = [root / name for name in include] if include else [root]
    paths = sorted(p for top in tops if top.exists() for p in ([top] if top.is_file() else top.rglob("*")))
    for path in paths:
        rel_parts = path.relative_to(root).parts
        if skip_hidden and any(part.startswith(".") for part in rel_parts):
            continue
        if path.is_symlink():
            if skip_symlinks:
                continue
            raise SystemExit(f"[store] symlinks are not supported: {path}")
        if not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        key = "/" + "/".join(p for p in (prefix.strip("/"), rel) if p)
        executable = os.name != "nt" and bool(path.stat().st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))
        files[key] = entry(store, path.read_bytes(), executable)
    return files


def files_revision(files: dict[str, dict]) -> str:
    """Stable revision for a list whose files fully determine it (e.g. sprites)."""
    canon = json.dumps(dict(sorted(files.items())), sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canon.encode()).hexdigest()[:16]


def pack_groups(files: dict[str, dict], target: int = PACK_TARGET) -> list[list[str]]:
    """Paths sorted and cut at ~target bytes, never mixing top-level directories."""
    groups: list[list[str]] = []
    current: list[str] = []
    size = 0
    top = None
    for path in sorted(files):
        parts = path.strip("/").split("/")
        path_top = parts[0] if len(parts) > 1 else ""
        item = files[path]["size"]
        if current and (path_top != top or size + item > target):
            groups.append(current)
            current, size = [], 0
        current.append(path)
        size += item
        top = path_top
    if current:
        groups.append(current)
    return groups


def build_pack(blobs: Path, files: dict[str, dict], paths: list[str]) -> bytes:
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        for path in paths:
            info = zipfile.ZipInfo(path.lstrip("/"), date_time=(1980, 1, 1, 0, 0, 0))
            info.external_attr = 0o644 << 16
            stored = Path(path).suffix.lower() in STORED_SUFFIXES
            info.compress_type = zipfile.ZIP_STORED if stored else zipfile.ZIP_DEFLATED
            zf.writestr(info, blob_path(blobs, files[path]["sha256"]).read_bytes(), compresslevel=9)
    return buf.getvalue()


def write_packs(out: Path, files: dict[str, dict]) -> list[dict]:
    """Zip packs over a list's blobs (already in <out>/content) into <out>/packs."""
    blobs, packs = out / "content", []
    for paths in pack_groups(files):
        data = build_pack(blobs, files, paths)
        digest = hashlib.sha256(data).hexdigest()
        dest = out / "packs" / digest[:2] / f"{digest}.zip"
        if not dest.is_file():
            dest.parent.mkdir(parents=True, exist_ok=True)
            tmp = dest.with_suffix(".tmp")
            tmp.write_bytes(data)
            os.replace(tmp, dest)
        packs.append({"sha256": digest, "size": len(data), "files": paths})
    return packs


def write_list(out: Path, name: str, revision: str, engine: str, files: dict[str, dict],
               entry_point: str | None = None, dest: Path | None = None, packs: bool = True) -> Path:
    doc: dict = {"schema": SCHEMA, "revision": revision, "engine": engine}
    if entry_point:
        if entry_point not in files:
            raise SystemExit(f"[store] entry {entry_point} not in {name}")
        doc["entry"] = entry_point
    doc["files"] = dict(sorted(files.items()))
    if packs and files:
        doc["packs"] = write_packs(out, files)
    dest = dest or out / ".qtclient" / "v3" / f"{name}.json"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(doc, indent=1) + "\n")
    total = sum(f["size"] for f in files.values())
    packed = sum(p["size"] for p in doc.get("packs", []))
    print(f"[store] {name}: {len(files)} files, {total} bytes ({len(doc.get('packs', []))} packs, "
          f"{packed} bytes) -> {dest}")
    return dest


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Store a directory tree and write its v3 file list.")
    ap.add_argument("--root", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--name", required=True, help="list name, e.g. content-web")
    ap.add_argument("--revision", required=True, help="list revision, or 'auto' for a hash of its files")
    ap.add_argument("--engine", required=True)
    ap.add_argument("--prefix", default="", help="path prefix inside the install, e.g. data/things/1530")
    ap.add_argument("--include", nargs="+", help="top-level names under --root to store (default: all)")
    ap.add_argument("--list", type=Path, help="write the list here instead of <out>/.qtclient/v3/<name>.json")
    ap.add_argument("--no-packs", action="store_true", help="blobs only")
    args = ap.parse_args()
    out = args.out.resolve()
    files = tree_entries(out / "content", args.root.resolve(), prefix=args.prefix, include=args.include,
                         skip_hidden=True)
    revision = files_revision(files) if args.revision == "auto" else args.revision
    write_list(out, args.name, revision, args.engine, files, dest=args.list, packs=not args.no_packs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
