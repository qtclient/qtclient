#!/usr/bin/env python3
"""Stage the Qt CIP launcher with its runtime and write a downloadable archive.

Output (under --out):
  installers/<platform>/          staged tree (QtClient.app / qtclient.exe+DLLs / qtclient-launcher)
  installers/<platform>.zip       archive for Windows/Linux /download
  installers/<platform>.dmg       macOS disk image for /download (preferred)

Python zipfile follows symlinks and drops directory links, which destroys Qt
framework layout inside QtClient.app and makes Gatekeeper report the app as
"damaged". macOS packaging uses ditto + hdiutil so symlinks stay intact.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_desktop import (  # noqa: E402
    APP_NAME,
    BUNDLE_ID,
    OTC,
    PLATFORMS,
    brand_icns,
    ldd_qt_libs,
    qt_prefix,
    set_rpath,
    without_qt_sqldrivers,
)

LAUNCHER_BUILD = {
    "macos-arm64": "launcher.app",
    "windows-x64": "launcher.exe",
    "linux-x64": "launcher",
}
# Widgets + Network plugins the launcher may load.
LINUX_LAUNCHER_PLUGINS = (
    "platforms",
    "imageformats",
    "iconengines",
    "tls",
    "networkinformation",
    "styles",
)


def find_launcher(bin_dir: Path, platform: str) -> Path:
    """qtclient launcher at <bin>/launcher[.app|/exe] (TOGGLE_BIN_FOLDER=ON)."""
    name = LAUNCHER_BUILD[platform]
    path = bin_dir / name
    want_dir = name.endswith(".app")
    if path.exists() and (path.is_dir() if want_dir else path.is_file()):
        return path
    raise SystemExit(
        f"[launcher] missing {path} (build: scripts/lifecycle/build/entrypoint.sh --launcher)"
    )


def brand_launcher_macos(app: Path) -> Path:
    """Rename launcher.app → APP_NAME.app (IdleOT.app when product-branded)."""
    target = app.parent / f"{APP_NAME}.app"
    if app != target:
        shutil.rmtree(target, ignore_errors=True)
        app.rename(target)
        app = target
    contents = app / "Contents"
    plist_path = contents / "Info.plist"
    import plistlib

    with open(plist_path, "rb") as fh:
        plist = plistlib.load(fh)
    old_exe = plist.get("CFBundleExecutable", "launcher")
    macos = contents / "MacOS"
    if old_exe != APP_NAME and (macos / old_exe).is_file():
        (macos / old_exe).rename(macos / APP_NAME)
    plist.update(
        CFBundleExecutable=APP_NAME,
        CFBundleName=APP_NAME,
        CFBundleDisplayName=APP_NAME,
        CFBundleIdentifier=BUNDLE_ID,
        CFBundleIconFile=APP_NAME,
    )
    with open(plist_path, "wb") as fh:
        plistlib.dump(plist, fh)
    resources = contents / "Resources"
    resources.mkdir(parents=True, exist_ok=True)
    icns = brand_icns()
    if icns is not None:
        shutil.copy2(icns, resources / f"{APP_NAME}.icns")
    return app


def adhoc_sign_macos(app: Path) -> None:
    subprocess.run(["xattr", "-cr", str(app)], check=False)
    subprocess.run(
        ["codesign", "--force", "--deep", "--sign", "-", str(app)],
        check=True,
    )


def stage_macos(build: Path, dest: Path, qt: Path) -> Path:
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    staged = dest / build.name
    subprocess.run(["ditto", str(build), str(staged)], check=True)
    deploy = [
        str(qt / "bin" / "macdeployqt"),
        str(staged),
        "-always-overwrite",
        "-verbose=1",
        "-appstore-compliant",
    ]
    with without_qt_sqldrivers(qt):
        subprocess.run(deploy, check=True)
    shutil.rmtree(staged / "Contents" / "PlugIns" / "sqldrivers", ignore_errors=True)
    app = brand_launcher_macos(staged)
    # Ad-hoc so the local tree is sealed; CI re-signs with Developer ID when secrets exist.
    adhoc_sign_macos(app)
    return app


def stage_windows(build: Path, dest: Path, qt: Path) -> Path:
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    exe = dest / f"{APP_NAME}.exe"
    shutil.copy2(build, exe)
    for dll in build.parent.glob("*.dll"):
        shutil.copy2(dll, dest / dll.name)
    deploy = [
        str(qt / "bin" / "windeployqt.exe"),
        "--release",
        "--no-translations",
        "--no-compiler-runtime",
        "--no-system-d3d-compiler",
        "--no-opengl-sw",
        "--no-quick",
        "--no-quick-import",
        "--dir",
        str(dest),
        str(exe),
    ]
    subprocess.run(deploy, check=True)
    return dest


def stage_linux(build: Path, dest: Path, qt: Path) -> Path:
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    # CIP package.json executable: idleot-launcher (IdleOT) / qtclient-launcher (stock).
    binary = dest / ("idleot-launcher" if APP_NAME == "IdleOT" else "qtclient-launcher")
    shutil.copy2(build, binary)
    binary.chmod(0o755)

    elves = [binary]
    for category in LINUX_LAUNCHER_PLUGINS:
        src = qt / "plugins" / category
        if src.is_dir():
            shutil.copytree(src, dest / "plugins" / category, symlinks=False)
            elves += sorted((dest / "plugins" / category).glob("*.so"))

    libs: dict[str, Path] = {}
    for elf in elves:
        libs.update(ldd_qt_libs(elf, qt / "lib"))
    (dest / "lib").mkdir()
    for name, src in sorted(libs.items()):
        shutil.copy2(src.resolve(), dest / "lib" / name)
    (dest / "qt.conf").write_text(
        "[Paths]\nPrefix = .\nLibraries = lib\nPlugins = plugins\n"
    )

    set_rpath(binary, "$ORIGIN/lib")
    for lib in (dest / "lib").iterdir():
        set_rpath(lib, "$ORIGIN")
    for elf in elves[1:]:
        depth = len(elf.relative_to(dest).parts) - 1
        set_rpath(elf, "$ORIGIN/" + "/".join([".."] * depth) + "/lib")
    return dest


def write_zip(src: Path, zip_path: Path, arcroot: str | None = None) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    zip_path.unlink(missing_ok=True)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        if src.is_file():
            zf.write(src, src.name)
            return
        root_name = arcroot or src.name
        for path in sorted(src.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(src).as_posix()
            zf.write(path, f"{root_name}/{rel}" if root_name else rel)


def write_macos_dmg(app: Path, dmg_path: Path) -> None:
    """Create a UDZO DMG that preserves .app framework symlinks."""
    dmg_path.parent.mkdir(parents=True, exist_ok=True)
    dmg_path.unlink(missing_ok=True)
    # Also keep a symlink-preserving zip next to the DMG for assemble fallbacks.
    zip_path = dmg_path.with_suffix(".zip")
    zip_path.unlink(missing_ok=True)
    subprocess.run(
        ["ditto", "-c", "-k", "--keepParent", str(app), str(zip_path)],
        check=True,
    )
    # Stage a clean folder so the DMG root is QtClient.app (drag-to-Applications).
    stage = dmg_path.parent / f".dmg-stage-{app.stem}"
    shutil.rmtree(stage, ignore_errors=True)
    stage.mkdir(parents=True)
    subprocess.run(["ditto", str(app), str(stage / app.name)], check=True)
    # Applications symlink for the usual drag-install UX.
    applications = stage / "Applications"
    if applications.exists() or applications.is_symlink():
        applications.unlink()
    applications.symlink_to("/Applications")
    subprocess.run(
        [
            "hdiutil",
            "create",
            "-volname",
            APP_NAME,
            "-srcfolder",
            str(stage),
            "-ov",
            "-format",
            "UDZO",
            str(dmg_path),
        ],
        check=True,
    )
    shutil.rmtree(stage, ignore_errors=True)


def stage_launcher(platform: str, bin_dir: Path, dest: Path) -> Path:
    build, qt = find_launcher(bin_dir, platform), qt_prefix()
    if platform == "macos-arm64":
        return stage_macos(build, dest, qt)
    if platform == "windows-x64":
        return stage_windows(build, dest, qt)
    return stage_linux(build, dest, qt)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--platform", choices=PLATFORMS, required=True)
    ap.add_argument("--bin", type=Path, required=True, help="cmake build dir holding the launcher binary")
    ap.add_argument("--out", type=Path, required=True, help="desktop slice root (installers/ written here)")
    args = ap.parse_args()

    out = args.out.resolve()
    platform = args.platform
    staged_dir = out / "installers" / platform
    staged = stage_launcher(platform, args.bin.resolve(), staged_dir)
    if platform == "macos-arm64":
        dmg_path = out / "installers" / f"{platform}.dmg"
        write_macos_dmg(staged, dmg_path)
        print(
            f"[launcher] {platform} -> {staged} + {dmg_path} "
            f"({dmg_path.stat().st_size} bytes) + {dmg_path.with_suffix('.zip').name}"
        )
    else:
        zip_path = out / "installers" / f"{platform}.zip"
        arcroot = APP_NAME if platform == "windows-x64" else (
            "idleot" if APP_NAME == "IdleOT" else "qtclient"
        )
        write_zip(staged_dir, zip_path, arcroot=arcroot)
        print(f"[launcher] {platform} -> {staged} + {zip_path} ({zip_path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
