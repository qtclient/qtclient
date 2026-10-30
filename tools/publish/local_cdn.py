#!/usr/bin/env python3
"""Assemble the local CDN bundle (dist/cdn) from whatever this checkout has built.

Dev /play and a dev launcher read /cdn/latest/release.json exactly like
production; the portal serves dist/cdn at /cdn. Slices land in dist/slices/:
  wasm/     build/tibia-wasm via pack-tibia-wasm.sh
  desktop/  tibia engine-<platform> + content-desktop (make client-build)
  assets/   content-things + store/outfits/items                     (make publish-assets)
then qtclient/release's scripts/assemble.py builds dist/cdn from them.

Usage:
  python3 tools/publish/local_cdn.py [--wasm] [--desktop]   # rebuild those slices, then assemble
"""
from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import package_desktop  # noqa: E402

OTC = HERE.parents[1]
UMBRELLA = OTC.parent.parent if OTC.parent.name == "apps" else OTC.parent
DIST = UMBRELLA / "dist"
SLICES = DIST / "slices"
ASSEMBLE = UMBRELLA / "apps" / "release" / "scripts" / "assemble.py"


def host_platform() -> str | None:
    system, machine = platform.system(), platform.machine().lower()
    if system == "Darwin" and machine in ("arm64", "aarch64"):
        return "macos-arm64"
    if system == "Linux" and machine in ("x86_64", "amd64"):
        return "linux-x64"
    if system == "Windows":
        return "windows-x64"
    return None


def wasm_slice() -> bool:
    out = SLICES / "wasm"
    tibia_dir = Path(os.environ.get("TIBIA_WASM_DIR", OTC / "build" / "tibia-wasm"))
    if not ((tibia_dir / "tibia.html").is_file() and (tibia_dir / "tibia.wasm").is_file()):
        print("[local-cdn] no tibia-wasm build — skipping wasm (make tibia-wasm)")
        return False
    shutil.rmtree(out, ignore_errors=True)
    subprocess.run(
        ["bash", str(OTC / "deploy" / "pack-tibia-wasm.sh"), str(tibia_dir), str(out)],
        check=True,
    )
    return True


def tibia_build_dir(plat: str) -> Path | None:
    """Directory that contains qtclient client.app / client (…/build/<preset>/bin)."""
    import os
    import platform as py_platform

    name = package_desktop.TIBIA_BUILD[plat]
    system = py_platform.system()
    os_name = "macos" if system == "Darwin" else "linux" if system == "Linux" else "windows"
    base = "release"
    if os.environ.get("DEV", "1").lower() not in ("0", "false", "no", "off"):
        base = "dev"
    if os.environ.get("DEBUG", "").lower() in ("1", "true", "yes", "on"):
        base = "debug"
    preset = os.environ.get("TIBIA_PRESET", f"{os_name}-{base}")
    bin_dir = OTC / "build" / preset / "bin"
    path = bin_dir / name
    if path.exists():
        return bin_dir
    return None


def desktop_slice() -> bool:
    plat = host_platform()
    build_dir = tibia_build_dir(plat) if plat else None
    if build_dir is None:
        print("[local-cdn] no tibia build for this OS (make client-build) — skipping desktop")
        return False
    out = SLICES / "desktop"
    shutil.rmtree(out, ignore_errors=True)
    revision = f"{package_desktop.default_revision()}-local"
    engine = package_desktop.engine_id()
    with tempfile.TemporaryDirectory() as tmp:
        staged = package_desktop.tibia_engine_bundle(plat, build_dir, Path(tmp) / "engine")
        package_desktop.write_tibia_engine_files(out, plat, revision, engine, staged)
    package_desktop.write_tibia_content_files(out, revision, engine, package_desktop.tibia_runtime_entries())
    return True


def assemble() -> int:
    if not ASSEMBLE.is_file():
        print(f"[local-cdn] {ASSEMBLE} missing (monorepo only) — dist/cdn not assembled")
        return 0
    args = [sys.executable, str(ASSEMBLE), "--out", str(DIST / "cdn.next")]
    for name in ("wasm", "desktop", "assets"):
        if (SLICES / name).is_dir():
            args += [f"--{name}", str(SLICES / name)]
    subprocess.run(args, check=True)
    old = DIST / "cdn.old"
    shutil.rmtree(old, ignore_errors=True)
    if (DIST / "cdn").exists():
        (DIST / "cdn").rename(old)
    (DIST / "cdn.next").rename(DIST / "cdn")
    shutil.rmtree(old, ignore_errors=True)
    print(f"[local-cdn] {DIST / 'cdn'} (served by the portal at /cdn)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--wasm", action="store_true", help="re-pack the browser build")
    ap.add_argument("--desktop", action="store_true", help="re-store the desktop build")
    args = ap.parse_args()
    if args.wasm:
        wasm_slice()
    if args.desktop:
        desktop_slice()
    return assemble()


if __name__ == "__main__":
    raise SystemExit(main())
