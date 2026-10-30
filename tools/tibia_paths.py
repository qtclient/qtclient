"""Locate an official CipSoft Tibia install (BYO license).

Used by extract, tibia-sync, qtclient-fetch-assets, and resolve-tibia.sh.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path


def _candidates() -> list[Path]:
    home = Path.home()
    env = os.environ.get("TIBIA", "").strip()
    out: list[Path] = []
    if env:
        out.append(Path(env).expanduser())

    # macOS — CipSoft launcher package, then Applications
    out.append(
        home
        / "Library/Application Support/CipSoft GmbH/Tibia/packages/Tibia.app"
    )
    out.append(Path("/Applications/Tibia.app"))

    # Windows — CipSoft package under LocalAppData, then common installs
    local = os.environ.get("LOCALAPPDATA") or str(home / "AppData/Local")
    out.append(Path(local) / "Tibia" / "packages" / "Tibia")
    out.append(Path(local) / "Tibia" / "packages" / "Tibia.app")
    for pf in (
        os.environ.get("ProgramFiles"),
        os.environ.get("ProgramFiles(x86)"),
        r"C:\Program Files",
        r"C:\Program Files (x86)",
    ):
        if pf:
            out.append(Path(pf) / "Tibia" / "Tibia.exe")
            out.append(Path(pf) / "Tibia")

    # Linux / Wine
    xdg = os.environ.get("XDG_DATA_HOME") or str(home / ".local/share")
    out.append(Path(xdg) / "CipSoft GmbH" / "Tibia" / "packages" / "Tibia")
    out.append(
        home
        / ".wine/drive_c/users"
        / os.environ.get("USER", "user")
        / "AppData/Local/Tibia/packages/Tibia"
    )
    return out


def looks_like_tibia(path: Path) -> bool:
    """True if path is a usable Tibia.app / Contents / package root."""
    p = path.expanduser()
    if not p.exists():
        return False
    # File: treat as Tibia.exe → parent package
    if p.is_file():
        p = p.parent
    checks = [
        p / "Contents" / "MacOS" / "client",
        p / "MacOS" / "client",
        p / "Contents" / "Resources" / "assets",
        p / "Resources" / "assets",
        p / "assets",
        p / "bin" / "client.exe",
        p / "client.exe",
    ]
    return any(c.exists() for c in checks)


def resolve_tibia(explicit: str | Path | None = None) -> Path:
    """Return the first existing Tibia install, or raise FileNotFoundError."""
    ordered: list[Path] = []
    if explicit:
        ordered.append(Path(explicit).expanduser())
    ordered.extend(_candidates())

    seen: set[Path] = set()
    for raw in ordered:
        try:
            path = raw.expanduser().resolve()
        except OSError:
            path = raw.expanduser()
        if path in seen:
            continue
        seen.add(path)
        if looks_like_tibia(path):
            return path if path.is_dir() or not path.is_file() else path.parent

    raise FileNotFoundError(
        "Tibia install not found. Install the official CipSoft client, or set:\n"
        "  export TIBIA=/path/to/Tibia.app   # or Tibia package / Contents\n"
        "  make sync TIBIA=/path/to/Tibia.app\n"
        "  make extract TIBIA=/path/to/Tibia.app\n"
        "  qtclient-fetch-assets --tibia /path/to/Tibia.app"
    )


def usage_hint() -> str:
    return (
        "[tibia] install not found. Install the official client, or:\n"
        "  make sync TIBIA=/path/to/Tibia.app\n"
        "  make extract TIBIA=/path/to/Tibia.app\n"
        "  qtclient-fetch-assets --tibia /path/to/Tibia.app"
    )


def main(argv: list[str] | None = None) -> int:
    args = argv if argv is not None else sys.argv[1:]
    explicit = args[0] if args else os.environ.get("TIBIA")
    try:
        path = resolve_tibia(explicit)
    except FileNotFoundError as e:
        print(str(e), file=sys.stderr)
        return 1
    print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
