#!/usr/bin/env python3
"""Emit a local CIP-shaped multi-client tip for launcher-first play.

Mirrors CipSoft's tibiametadata.json contract (live tip: Tibia + Tibia External
Test): tip-driven `packages[]`, no client-count cap, separate package vs assets
version URLs, bare `directoryname`. See CIP package layout (packages/<name>/)
and the launcher::TPackage unit (variant / directoryname / installautomatically /
builds).

Local convenience (still CIP tip shape):
  - Discover sibling git worktrees with a built client.
  - Discover product pack trees (QTCLIENT_PACK / apps/idleot-client), including
    pack-only sources that reuse another worktree's binary.
  - Share one assetsurl/assetsversionurl across packages (code-only bumps only
    package.json.version). Prefer local CDN assets-current; else CipSoft static.
  - Stage engine files uncompressed with \"unpack\": false so Update is a local
    copy (CIP sprites already use unpack:false; release/demo tips stay lzma).

Usage:
  emit_play_tip.py --out dist/play/tip
  emit_play_tip.py --out dist/play/tip --current-only
  emit_play_tip.py --out dist/play/tip --pack /path/to/idleot-client/client
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
OTC = HERE.parents[1]
UMBRELLA = OTC.parent.parent if OTC.parent.name == "apps" else OTC.parent

# CipSoft tip variants (TPackage::EVariant) — art only, not a count limit.
VARIANTS = ("regular", "test", "beta", "preview")

CIP_ASSETS = {
    "macos": (
        "https://static.tibia.com/launcher/assets-current/assets-macos.json",
        "https://static.tibia.com/launcher/assets-current/assets.json.sha256",
    ),
    "linux": (
        "https://static.tibia.com/launcher/assets-current/assets.json",
        "https://static.tibia.com/launcher/assets-current/assets.json.sha256",
    ),
    "windows": (
        "https://static.tibia.com/launcher/assets-current/assets.json",
        "https://static.tibia.com/launcher/assets-current/assets.json.sha256",
    ),
}

# Soft UI comfort (launcher wraps at 4/row); tip itself is unbounded like CIP.
SOFT_TILE_WARN = 8


@dataclass
class PlaySource:
    """One tip packages[] entry source (worktree binary and/or pack FS)."""

    key: str
    name: str
    directory_name: str
    kind: str  # worktree | pack | current
    root: Path
    binary_root: Path  # checkout that owns the client binary
    pack: Path | None = None
    branch: str = ""
    extra: dict = field(default_factory=dict)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def host_os_arch() -> tuple[str, str, str]:
    """Return (tip_os, architecture_field, platform_key)."""
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Darwin":
        return "macos", "arm64;x86_64", "macos-arm64"
    if system == "Linux":
        return "linux", "x86_64", "linux-x64"
    if system == "Windows":
        return "windows", "x86_64", "windows-x64"
    raise SystemExit(f"[play-tip] unsupported host: {system}")


def desktop_preset(otc: Path | None = None) -> str:
    """Match scripts/lifecycle/lib/desktop-paths.sh preset selection."""
    del otc  # reserved for future .env reads beside a worktree
    if os.environ.get("TIBIA_PRESET"):
        return os.environ["TIBIA_PRESET"]
    system = platform.system()
    if system == "Darwin":
        os_name = "macos"
    elif system == "Linux":
        os_name = "linux"
    else:
        os_name = "windows"
    base = "release"
    if os_name != "windows" and os.environ.get("DEV", "1").lower() not in (
        "0",
        "false",
        "no",
        "off",
    ):
        base = "dev"
    if os.environ.get("DEBUG", "").lower() in ("1", "true", "yes", "on"):
        base = "debug"
    return f"{os_name}-{base}"


def client_tree_for(checkout: Path) -> Path | None:
    """Return the Play tree (macOS .app or bin dir) when a client binary exists."""
    preset = desktop_preset(checkout)
    bin_dir = checkout / "apps" / "qtclient" / "build" / preset / "bin"
    if not bin_dir.is_dir():
        # Standalone qtclient checkout.
        alt = checkout / "build" / preset / "bin"
        bin_dir = alt if alt.is_dir() else bin_dir
    system = platform.system()
    if system == "Darwin":
        for name in ("client.app", "qtclient.app", "IdleOT.app"):
            app = bin_dir / name
            if (app / "Contents" / "MacOS" / "client").is_file():
                return app
        return None
    if system == "Windows":
        exe = bin_dir / "client.exe"
        return bin_dir if exe.is_file() else None
    exe = bin_dir / "client"
    return bin_dir if exe.is_file() else None


def git_worktrees(repo: Path) -> list[tuple[Path, str]]:
    """Return (path, branch-or-detached) for each worktree of repo."""
    try:
        out = subprocess.check_output(
            ["git", "-C", str(repo), "worktree", "list", "--porcelain"],
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError):
        return [(repo.resolve(), _git_branch(repo))]
    entries: list[tuple[Path, str]] = []
    path: Path | None = None
    branch = ""
    for line in out.splitlines():
        if line.startswith("worktree "):
            if path is not None:
                entries.append((path, branch or _git_branch(path)))
            path = Path(line[len("worktree ") :]).resolve()
            branch = ""
        elif line.startswith("branch "):
            ref = line[len("branch ") :].strip()
            # refs/heads/session/all → session/all (not only the last segment).
            if ref.startswith("refs/heads/"):
                branch = ref[len("refs/heads/") :]
            else:
                branch = ref.rsplit("/", 1)[-1] if ref else ""
        elif line.startswith("detached"):
            branch = "detached"
    if path is not None:
        entries.append((path, branch or _git_branch(path)))
    return entries or [(repo.resolve(), _git_branch(repo))]


def _git_branch(repo: Path) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(repo), "rev-parse", "--abbrev-ref", "HEAD"],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return repo.name


def sanitize_directory_name(raw: str, *, prefix: str = "IdleOT") -> str:
    """CIP-safe bare directoryname (launcher adds .app on macOS)."""
    text = re.sub(r"[^A-Za-z0-9]+", "-", raw.strip()).strip("-")
    if not text:
        text = "dev"
    if text.lower().startswith(prefix.lower()):
        name = text
    else:
        name = f"{prefix}-{text}"
    # Keep AppData paths short; CIP uses Tibia / TibiaExternal.
    return name[:48].rstrip("-")


def pack_root_candidates(checkout: Path) -> list[Path]:
    """Product pack FS roots under a checkout (mod.json required)."""
    found: list[Path] = []
    env_pack = os.environ.get("QTCLIENT_PACK", "").strip()
    if env_pack:
        p = Path(env_pack)
        if not p.is_absolute():
            p = (checkout / p).resolve()
        # QTCLIENT_PACK may be pack root or …/client.
        for cand in (p, p / "client"):
            if (cand / "mod.json").is_file():
                found.append(cand.resolve())
                break
    idleot = checkout / "apps" / "idleot-client" / "client"
    if (idleot / "mod.json").is_file():
        found.append(idleot.resolve())
    # Dedup while preserving order.
    seen: set[Path] = set()
    out: list[Path] = []
    for path in found:
        if path not in seen:
            seen.add(path)
            out.append(path)
    return out


def pack_fingerprint(pack: Path) -> str:
    h = hashlib.sha256()
    for path in sorted(pack.rglob("*")):
        if not path.is_file():
            continue
        rel = path.relative_to(pack).as_posix()
        if rel in {".pack.stamp", ".files.manifest"} or rel.endswith(".files"):
            continue
        if "/lib/" in f"/{rel}/" or rel.startswith("lib/"):
            continue
        h.update(rel.encode())
        h.update(b"\0")
        h.update(path.read_bytes())
        h.update(b"\0")
    return h.hexdigest()


def discover_sources(
    umbrella: Path,
    *,
    current_only: bool = False,
    extra_packs: list[Path] | None = None,
) -> list[PlaySource]:
    """Worktree binaries + pack-only overlays (CIP packages[] sources)."""
    current = umbrella.resolve()
    worktrees = git_worktrees(current) if not current_only else [(current, _git_branch(current))]
    sources: list[PlaySource] = []
    seen_dirs: set[str] = set()
    binary_by_root: dict[Path, Path] = {}

    for root, branch in worktrees:
        tree = client_tree_for(root)
        if tree is None:
            continue
        binary_by_root[root] = tree
        slug = sanitize_directory_name(branch or root.name)
        if slug in seen_dirs:
            slug = sanitize_directory_name(f"{branch}-{root.name[-6:]}")
        seen_dirs.add(slug)
        packs = pack_root_candidates(root)
        pack = packs[0] if packs else None
        kind = "current" if root == current else "worktree"
        name = branch or root.name
        if root == current:
            name = f"{name} (this)"
        sources.append(
            PlaySource(
                key=slug.lower(),
                name=name,
                directory_name=slug,
                kind=kind,
                root=root,
                binary_root=root,
                pack=pack,
                branch=branch,
            )
        )

    # Pack-only tiles: CipSoft-shaped extra packages when a product pack differs
    # but there is no separate client binary (reuse primary engine). Same tip
    # contract as Tibia vs TibiaExternal — distinct directoryname + builds.
    primary_root = current if current in binary_by_root else next(iter(binary_by_root), None)
    if primary_root is not None:
        represented_fps = {
            pack_fingerprint(s.pack) for s in sources if s.pack and (s.pack / "mod.json").is_file()
        }
        # Explicit --pack only by default. Sibling pack-only tiles (no client
        # binary) are opt-in via QTCLIENT_PLAY_PACK_WORKTREES=1 — otherwise every
        # idleot-only worktree would flood the CIP tile grid.
        pack_paths: list[Path] = list(extra_packs or [])
        if not current_only and os.environ.get("QTCLIENT_PLAY_PACK_WORKTREES", "").lower() in (
            "1",
            "true",
            "yes",
            "on",
        ):
            for root, _branch in worktrees:
                if root in binary_by_root:
                    continue
                pack_paths.extend(pack_root_candidates(root))
        for pack in pack_paths:
            pack = pack.resolve()
            if not (pack / "mod.json").is_file():
                continue
            fp_full = pack_fingerprint(pack)
            if fp_full in represented_fps:
                continue
            represented_fps.add(fp_full)
            fp = fp_full[:8]
            try:
                mod = json.loads((pack / "mod.json").read_text())
                mod_id = str(mod.get("id") or pack.parent.name)
            except (OSError, ValueError):
                mod_id = pack.parent.name
            slug = sanitize_directory_name(f"{mod_id}-{fp}")
            if slug in seen_dirs:
                slug = sanitize_directory_name(f"{mod_id}-pack-{fp}")
            if slug in seen_dirs:
                continue
            seen_dirs.add(slug)
            sources.append(
                PlaySource(
                    key=slug.lower(),
                    name=f"{mod_id} pack",
                    directory_name=slug,
                    kind="pack",
                    root=pack,
                    binary_root=primary_root,
                    pack=pack,
                    branch=mod_id,
                    extra={"packFingerprint": fp},
                )
            )

    # Current checkout first (installautomatically), then others.
    sources.sort(key=lambda s: (0 if s.kind == "current" else 1, s.name.lower()))
    return sources


def place_file(src: Path | str, dest: Path | str) -> None:
    # shutil.copytree passes str paths into copy_function.
    src_path = Path(src)
    dest_path = Path(dest)
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    if dest_path.exists() or dest_path.is_symlink():
        try:
            if src_path.exists() and src_path.samefile(dest_path):
                return
        except OSError:
            pass
        dest_path.unlink()
    try:
        os.link(src_path, dest_path)
    except OSError:
        shutil.copy2(src_path, dest_path)


def stage_tree(src_tree: Path, dest_tree: Path, tip_os: str) -> Path:
    """Copy/hardlink the Play tree into dest; return the staged root to pack."""
    if dest_tree.exists():
        shutil.rmtree(dest_tree)
    dest_tree.mkdir(parents=True)
    if tip_os == "macos":
        # Stage the .app bundle as the package root (CIP: packages/Tibia.app).
        if src_tree.name.endswith(".app"):
            staged = dest_tree / src_tree.name
            shutil.copytree(src_tree, staged, symlinks=True, copy_function=place_file)
            return staged
        staged = dest_tree / "client.app"
        shutil.copytree(src_tree, staged, symlinks=True, copy_function=place_file)
        return staged
    # Linux/Windows: CIP lays the engine under bin/.
    staged_bin = dest_tree / "bin"
    staged_bin.mkdir(parents=True)
    if src_tree.is_dir():
        for child in src_tree.iterdir():
            target = staged_bin / child.name
            if child.is_dir():
                shutil.copytree(child, target, symlinks=True, copy_function=place_file)
            else:
                place_file(child, target)
    return dest_tree


def stage_pack_into_tree(tree: Path, pack: Path, tip_os: str) -> None:
    """FS chrome next to the Play binary (merge-pack-into-tip layout)."""
    if tip_os == "macos":
        mods_root = tree / "Contents" / "MacOS" / "mods"
        if not mods_root.parent.is_dir():
            # Staged as client.app
            apps = list(tree.parent.glob("*.app")) if tree.is_dir() else []
            app = tree if (tree / "Contents" / "MacOS").is_dir() else (apps[0] if apps else tree)
            mods_root = app / "Contents" / "MacOS" / "mods"
    else:
        mods_root = tree / "bin" / "mods" if (tree / "bin").is_dir() else tree / "mods"
    try:
        mod = json.loads((pack / "mod.json").read_text())
        pack_id = str(mod.get("id") or "pack")
    except (OSError, ValueError):
        pack_id = pack.name
    dest = mods_root / pack_id
    # The binary's build staged the installed pack (-DQTCLIENT_PACKS: mod.json with the
    # SDK's hostBuildId, native/mod.* on desktop): that is the pack this binary runs.
    # Keep it and lay the pack's filesystem chrome over it; never ship pack sources.
    installed = (dest / "mod.json").is_file()
    if dest.exists() and not installed:
        shutil.rmtree(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    source_ignore = shutil.ignore_patterns(
        "lib", "native", "*.a", ".git", "*.cpp", "*.c", "*.cc", "*.h", "*.hpp", "*.mm",
        "*.cmake", "CMakeLists.txt", "*.icns", "*.frag", "*.vert", "*.comp",
    )
    if installed:
        for name in ("qml", "overrides", "images", "resources"):
            src = pack / name
            if src.is_dir():
                shutil.copytree(src, dest / name, ignore=source_ignore, symlinks=True, dirs_exist_ok=True)
    else:
        shutil.copytree(pack, dest, ignore=source_ignore, symlinks=True)
    # Directories left empty by the source filter (e.g. idleot/ with only C++).
    for path in sorted(dest.rglob("*"), reverse=True):
        if path.is_dir() and not any(path.iterdir()):
            path.rmdir()
    # Drop stock portal when a product pack supplies urls (merge-pack policy).
    sibling = mods_root / "portal"
    if sibling.is_dir() and pack_id != "portal":
        shutil.rmtree(sibling)
    # The pack's launcher config (download channels), next to its client/ dir.
    launcher_json = pack.parent / "launcher" / "launcher.json"
    if launcher_json.is_file():
        shutil.copy2(launcher_json, dest / "launcher.json")


def iter_pack_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            # Follow only if target is a regular file inside the tree.
            try:
                if path.resolve().is_file() and root in path.resolve().parents:
                    files.append(path)
            except OSError:
                continue
            continue
        if path.is_file():
            files.append(path)
    return files


def emit_package_files(package_dir: Path, tree: Path, tip_os: str) -> tuple[list[dict], str, str]:
    """Write raw file records (unpack:false) + package.json; return files, version, exe."""
    if package_dir.exists():
        shutil.rmtree(package_dir)
    files_root = package_dir / "files"
    files_root.mkdir(parents=True)

    records: list[dict] = []
    digest = hashlib.sha256()
    for path in iter_pack_files(tree):
        rel = path.relative_to(tree).as_posix()
        if tip_os == "macos":
            # Package root is the bundle; localfiles are Contents/…
            localfile = rel
        else:
            # tree may already be dest with bin/; keep relative path
            localfile = rel if rel.startswith("bin/") else f"bin/{rel}"
        dest = files_root / localfile
        place_file(path.resolve() if path.is_symlink() else path, dest)
        data_hash = sha256_file(dest)
        size = dest.stat().st_size
        digest.update(localfile.encode())
        digest.update(b"\0")
        digest.update(data_hash.encode())
        digest.update(b"\0")
        records.append(
            {
                "url": f"files/{localfile}",
                "localfile": localfile,
                "packedhash": data_hash,
                "packedsize": size,
                "unpackedhash": data_hash,
                "unpackedsize": size,
                # Local play tip: raw bytes (CIP sprites already use unpack:false).
                "unpack": False,
            }
        )

    version = f"15.00.{digest.hexdigest()[:6]}"
    if tip_os == "macos":
        exe = "Contents/MacOS/client"
        if not any(r["localfile"] == exe for r in records):
            nested = next(
                (r["localfile"] for r in records if r["localfile"].endswith("Contents/MacOS/client")),
                None,
            )
            if nested:
                exe = nested
    elif tip_os == "windows":
        exe = "bin/client.exe"
    else:
        exe = "bin/client"
    for record in records:
        if record["localfile"] == exe:
            record["executable"] = True

    pkg = {
        "version": version,
        "creation_time": int(time.time()),
        "files": records,
        "executable": exe,
        "generation": "second",
    }
    (package_dir / "package.json").write_text(json.dumps(pkg, indent=2) + "\n")
    (package_dir / "package.json.version").write_text(version + "\n")
    return records, version, exe


def resolve_assets(tip_os: str, cdn_url: str | None) -> tuple[str, str]:
    """Shared assetsurl across packages — CIP package/assets split."""
    assets_name = "assets-macos.json" if tip_os == "macos" else "assets.json"
    local_assets = UMBRELLA / "dist" / "cdn" / "launcher" / "assets-current"
    if cdn_url and (local_assets / assets_name).is_file() and (local_assets / "assets.json.sha256").is_file():
        base = cdn_url.rstrip("/")
        return (
            f"{base}/launcher/assets-current/{assets_name}",
            f"{base}/launcher/assets-current/assets.json.sha256",
        )
    # Fallback: CipSoft assets-current (demo tip behaviour).
    return CIP_ASSETS[tip_os]


def emit_tip(
    out: Path,
    sources: list[PlaySource],
    *,
    cdn_url: str | None,
    release_base: str | None = None,
) -> Path:
    tip_os, arch, _plat = host_os_arch()
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    assets_url, assets_version_url = resolve_assets(tip_os, cdn_url)
    base = (release_base or out.as_uri()).rstrip("/")

    packages: list[dict] = []
    if len(sources) > SOFT_TILE_WARN:
        print(
            f"[play-tip] warning: {len(sources)} packages (CIP UI wraps at 4/row; "
            f"soft comfort ~{SOFT_TILE_WARN})",
            file=sys.stderr,
        )

    for index, source in enumerate(sources):
        variant = VARIANTS[min(index, len(VARIANTS) - 1)] if index < len(VARIANTS) else "test"
        # Beyond four tiles, keep using test art (CIP has only four logos).
        if index >= len(VARIANTS):
            variant = "test"
        binary_tree = client_tree_for(source.binary_root)
        if binary_tree is None:
            print(f"[play-tip] skip {source.name}: no client binary", file=sys.stderr)
            continue
        pkg_dir = out / source.directory_name
        stage = out / ".stage" / source.directory_name
        staged = stage_tree(binary_tree, stage, tip_os)
        if source.pack and (source.pack / "mod.json").is_file():
            stage_pack_into_tree(staged if tip_os == "macos" else stage, source.pack, tip_os)
        elif tip_os != "macos":
            # Stock login pack when no product pack.
            qtc = OTC / "client" / "mods" / "portal"
            if (qtc / "mod.json").is_file():
                stage_pack_into_tree(stage, qtc, tip_os)
        else:
            qtc = OTC / "client" / "mods" / "portal"
            if source.pack is None and (qtc / "mod.json").is_file():
                stage_pack_into_tree(staged, qtc, tip_os)

        pack_root = staged if tip_os == "macos" else stage
        _records, version, _exe = emit_package_files(pkg_dir, pack_root, tip_os)
        # Drop staging tree to save disk; package files/ holds the tip payload.
        shutil.rmtree(stage, ignore_errors=True)

        # CipSoft gives each tip package a distinct assets host (static vs
        # static.test). Local tips share one assets-current; fragment-tag the
        # urls so the download pool treats them as separate requests while the
        # HTTP path stays identical (fragments are not sent on the wire).
        asset_tag = source.directory_name
        build = {
            "os": tip_os,
            "architecture": arch,
            "url": f"{base}/{source.directory_name}/package.json",
            "versionurl": f"{base}/{source.directory_name}/package.json.version",
            "assetsurl": f"{assets_url}#{asset_tag}",
            "assetsversionurl": f"{assets_version_url}#{asset_tag}",
        }
        packages.append(
            {
                "name": source.name,
                "variant": variant,
                "directoryname": source.directory_name,
                "installautomatically": index == 0,
                "logolinktarget": "",
                "builds": [build],
            }
        )
        print(
            f"[play-tip] package {source.directory_name} variant={variant} "
            f"kind={source.kind} version={version}"
        )

    if not packages:
        raise SystemExit(
            "[play-tip] no packages — build a client first "
            "(make qtclient desktop build / make stock desktop build)"
        )

    metadata = {
        "launcher": {"name": "LAUNCHER", "builds": []},
        "packages": packages,
        # CipSoft field: show name under each tile when several clients exist.
        "showpackagenames": len(packages) > 1,
        "hints": "https://static.tibia.com/hints.json",
    }
    meta_path = out / "tibiametadata.json"
    meta_path.write_text(json.dumps(metadata, indent=2) + "\n")
    manifest = {
        "schema": 1,
        "generatedAt": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "assetsurl": assets_url,
        "packages": [
            {
                "name": p["name"],
                "directoryname": p["directoryname"],
                "variant": p["variant"],
                "kind": next(
                    (s.kind for s in sources if s.directory_name == p["directoryname"]),
                    "",
                ),
            }
            for p in packages
        ],
    }
    (out / "play-sources.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"[play-tip] tip → {meta_path} ({len(packages)} packages, shared assets)")
    return meta_path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "--out",
        type=Path,
        default=UMBRELLA / "dist" / "play" / "tip",
        help="tip output directory (tibiametadata.json + per-package dirs)",
    )
    ap.add_argument(
        "--umbrella",
        type=Path,
        default=UMBRELLA,
        help="monorepo root used for worktree discovery",
    )
    ap.add_argument(
        "--current-only",
        action="store_true",
        help="only the current checkout (no sibling worktrees)",
    )
    ap.add_argument(
        "--pack",
        type=Path,
        action="append",
        default=[],
        help="extra product pack client/ dir (repeatable)",
    )
    ap.add_argument(
        "--cdn-url",
        default=os.environ.get("IDLEOT_CDN_URL")
        or os.environ.get("QTCLIENT_CDN_URL")
        or os.environ.get("IDLEOT_ASSET_BASE")
        or "http://127.0.0.1:9080",
        help="local CDN base for shared assets-current (empty → CipSoft static)",
    )
    ap.add_argument(
        "--cip-assets",
        action="store_true",
        help="force CipSoft static.tibia.com assetsurl (ignore local CDN)",
    )
    ap.add_argument(
        "--list",
        action="store_true",
        help="print discovered sources and exit",
    )
    args = ap.parse_args()
    umbrella = args.umbrella.resolve()
    sources = discover_sources(
        umbrella,
        current_only=args.current_only,
        extra_packs=list(args.pack or []),
    )
    if args.list:
        for source in sources:
            print(
                f"{source.kind:8} {source.directory_name:40} {source.name} "
                f"pack={source.pack or '-'}"
            )
        print(f"[play-tip] {len(sources)} source(s)")
        return 0
    cdn = None if args.cip_assets else (args.cdn_url or None)
    # file:// tip: package urls must be absolute file URLs so the launcher
    # resolves them the CipSoft way (resolveConfigurationUrl keeps absolutes).
    out = args.out.resolve()
    emit_tip(out, sources, cdn_url=cdn, release_base=out.as_uri())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
