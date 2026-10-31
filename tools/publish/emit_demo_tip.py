#!/usr/bin/env python3
"""Emit a demo CIP tip: our client package + CipSoft assetsurl (no CIP blobs).

Writes under --out:
  tibiametadata.json
  <os>-package.json
  <os>-package.json.version
  <os>-package/          CIP package tree (localfile.lzma + optional unpacked)
  mods/default           urls-only myAAC login pack (staged into the client tree)
  release_env.lua        revision only (login URLs from mods/default)

Usage:
  emit_demo_tip.py --platform macos-arm64 --tree build/engine/QtClient.app \\
      --out dist/demo --release-base-url https://github.com/qtclient/qtclient/releases/latest/download
"""
from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import os
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_desktop import (  # noqa: E402
    APP_NAME,
    GAME_EXE,
    PLATFORMS,
)

OTC = Path(__file__).resolve().parents[2]
DEMO_TEMPLATE = OTC / "docs" / "public" / "tibiametadata.demo.json"

# Tip package.json.version / Release asset names.
PLATFORM_TIP = {
    "macos-arm64": ("macos", "macos-package"),
    "linux-x64": ("linux", "linux-package"),
    "windows-x64": ("windows", "windows-package"),
}

CIP_ASSETS = {
    "macos-arm64": (
        "https://static.tibia.com/launcher/assets-current/assets-macos.json",
        "https://static.tibia.com/launcher/assets-current/assets.json.sha256",
    ),
    "linux-x64": (
        "https://static.tibia.com/launcher/assets-current/assets.json",
        "https://static.tibia.com/launcher/assets-current/assets.json.sha256",
    ),
    "windows-x64": (
        "https://static.tibia.com/launcher/assets-current/assets.json",
        "https://static.tibia.com/launcher/assets-current/assets.json.sha256",
    ),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cip_stamp(revision: str) -> str:
    """Short CIP-shaped package version stamp."""
    rev = (revision or "0").strip()
    if len(rev) >= 12 and rev[:4].isdigit():
        return rev[:12]
    digest = sha256_bytes(rev.encode())[:6]
    return f"15.00.{digest}"


def pack_file(package_dir: Path, blob: Path, localfile: str) -> dict:
    data = blob.read_bytes()
    packed = lzma.compress(data, format=lzma.FORMAT_ALONE)
    packed_hash = sha256_bytes(packed)
    rel = localfile.lstrip("/") + ".lzma"
    dest = package_dir / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(packed)
    return {
        "url": rel.replace("\\", "/"),
        "localfile": localfile.lstrip("/").replace("\\", "/"),
        "packedhash": packed_hash,
        "packedsize": len(packed),
        "unpackedhash": sha256_bytes(data),
        "unpackedsize": len(data),
    }


def pack_tree(package_dir: Path, tree: Path) -> list[dict]:
    records: list[dict] = []
    files = sorted(p for p in tree.rglob("*") if p.is_file() and not p.is_symlink())
    for index, path in enumerate(files, start=1):
        rel = path.relative_to(tree).as_posix()
        if index == 1 or index % 200 == 0 or index == len(files):
            print(f"[demo] pack {index}/{len(files)} {rel}")
        records.append(pack_file(package_dir, path, rel))
    return records


def executable_for(platform: str, tree: Path) -> str:
    if platform == "macos-arm64":
        # Prefer Contents/MacOS/client (CipSoft Play basename).
        mac = tree / "Contents" / "MacOS" / GAME_EXE
        if mac.is_file():
            return f"Contents/MacOS/{GAME_EXE}"
        # Tree may be the .app itself or parent of APP_NAME.app.
        nested = tree / f"{APP_NAME}.app" / "Contents" / "MacOS" / GAME_EXE
        if nested.is_file():
            return f"{APP_NAME}.app/Contents/MacOS/{GAME_EXE}"
        return f"Contents/MacOS/{GAME_EXE}"
    if platform == "windows-x64":
        return f"{GAME_EXE}.exe"
    return GAME_EXE


def stage_default_mod(tree: Path, platform: str) -> None:
    """Install urls-only mods/default (myAAC) next to the Play binary."""
    src = OTC / "client" / "mods" / "default"
    if not (src / "mod.json").is_file():
        print(f"[demo] warning: missing {src}/mod.json")
        return
    if platform == "macos-arm64":
        dest = tree / "Contents" / "MacOS" / "mods" / "default"
    else:
        dest = tree / "mods" / "default"
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest)
    print(f"[demo] staged {dest}")


def write_revision_release_env(tree: Path, revision: str) -> None:
    """Drop release_env.lua with revision only (login URLs live in mods/default)."""
    body = (
        "return {\n"
        f'    TIBIA_REVISION = {json.dumps(revision)},\n'
        f'    QTCLIENT_REVISION = {json.dumps(revision)},\n'
        "}\n"
    )
    for dest_dir in (
        tree / "Contents" / "Resources",
        tree / f"{APP_NAME}.app" / "Contents" / "Resources",
        tree,
    ):
        if dest_dir == tree or dest_dir.is_dir() or (dest_dir.parent / "MacOS").is_dir():
            dest_dir.mkdir(parents=True, exist_ok=True)
            path = dest_dir / "release_env.lua"
            path.write_text(body)
            print(f"[demo] wrote {path}")
            return
    (tree / "release_env.lua").write_text(body)


def resolve_tree(platform: str, tree: Path) -> Path:
    tree = tree.resolve()
    if platform == "macos-arm64":
        if tree.name.endswith(".app") and (tree / "Contents" / "MacOS").is_dir():
            return tree
        nested = tree / f"{APP_NAME}.app"
        if nested.is_dir():
            return nested
        client_app = tree / "client.app"
        if client_app.is_dir():
            return client_app
    return tree


def emit_package(
    platform: str,
    tree: Path,
    out: Path,
    revision: str,
    release_base: str,
) -> tuple[Path, Path, str]:
    tip_os, tip_name = PLATFORM_TIP[platform]
    package_dir = out / f"{tip_name}-files"
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(parents=True)

    tree = resolve_tree(platform, tree)
    stage_default_mod(tree, platform)
    write_revision_release_env(tree, revision)

    files = pack_tree(package_dir, tree)
    version = cip_stamp(revision)
    exe = executable_for(platform, tree)
    pkg = {
        "version": version,
        "creation_time": int(time.time()),
        "files": files,
        "executable": exe,
        "generation": "second",
    }
    pkg_path = out / f"{tip_name}.json"
    ver_path = out / f"{tip_name}.json.version"
    pkg_path.write_text(json.dumps(pkg, indent=2) + "\n")
    ver_path.write_text(version + "\n")

    # Also place package.json next to lzma tree for local file:// tips.
    (package_dir / "package.json").write_text(json.dumps(pkg, indent=2) + "\n")
    (package_dir / "package.json.version").write_text(version + "\n")

    print(f"[demo] {tip_os} package {version} ({len(files)} files) -> {pkg_path}")
    return pkg_path, ver_path, version


def emit_metadata(out: Path, release_base: str, platforms: list[str]) -> Path:
    release_base = release_base.rstrip("/")
    template = json.loads(DEMO_TEMPLATE.read_text()) if DEMO_TEMPLATE.is_file() else {
        "launcher": {"builds": []},
        "packages": [{"name": "qtclient", "variant": "regular", "directoryname": "QtClient.app",
                      "installautomatically": True, "logolinktarget": "", "builds": []}],
        "hints": "https://static.tibia.com/hints.json",
    }
    builds = []
    for platform in platforms:
        tip_os, tip_name = PLATFORM_TIP[platform]
        assets_url, assets_version = CIP_ASSETS[platform]
        arch = "arm64;x86_64" if platform == "macos-arm64" else "x86_64"
        builds.append({
            "os": tip_os,
            "architecture": arch,
            "url": f"{release_base}/{tip_name}.json",
            "versionurl": f"{release_base}/{tip_name}.json.version",
            "assetsurl": assets_url,
            "assetsversionurl": assets_version,
        })
    template["packages"][0]["builds"] = builds
    template["packages"][0]["directoryname"] = "QtClient.app"
    template["packages"][0]["name"] = "qtclient"
    meta = out / "tibiametadata.json"
    meta.write_text(json.dumps(template, indent=2) + "\n")
    print(f"[demo] tip -> {meta}")
    return meta


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--platform", choices=PLATFORMS, action="append", dest="platforms")
    ap.add_argument("--tree", type=Path, help="staged client tree (.app / dir with client binary)")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument(
        "--release-base-url",
        default=os.environ.get(
            "QTCLIENT_DEMO_RELEASE_URL",
            "https://github.com/qtclient/qtclient/releases/latest/download",
        ),
    )
    ap.add_argument(
        "--revision",
        default=os.environ.get("QTCLIENT_HOST_BUILD_ID")
        or os.environ.get("SOURCE_COMMIT")
        or "dev",
    )
    ap.add_argument(
        "--metadata-only",
        action="store_true",
        help="only write tibiametadata.json for the given --platform list",
    )
    args = ap.parse_args()
    platforms = args.platforms or list(PLATFORMS)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)

    if not args.metadata_only:
        if args.tree is None:
            ap.error("--tree is required unless --metadata-only")
        if len(platforms) != 1:
            ap.error("pass exactly one --platform with --tree")
        emit_package(platforms[0], args.tree, out, args.revision, args.release_base_url)

    emit_metadata(out, args.release_base_url, platforms)
    # Sidecar tip next to out for local launcher tests.
    print("[demo] CipSoft asset blobs are NOT included (assetsurl -> static.tibia.com)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
