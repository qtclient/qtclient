#!/usr/bin/env python3
"""Populate CIP-shaped assets next to an qtclient client from a local Tibia install.

BYO license — copies from the official client on this machine; never downloads
CIP blobs. Intended for release zips:

  ./qtclient-fetch-assets
  ./client

Also usable from a source tree:

  python3 tools/fetch-assets/fetch_assets.py [--tibia PATH] [--out DIR]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def _layout() -> tuple[Path, Path]:
    """Return (tools_dir, package_root) for source tree or release zip."""
    # Release zip: fetch_assets.py next to tools/tibia_paths.py + client
    if (HERE / "tools" / "tibia_paths.py").is_file():
        return HERE / "tools", HERE
    # Source: tools/fetch-assets/fetch_assets.py
    if (HERE.parent / "tibia_paths.py").is_file():
        return HERE.parent, HERE.parent.parent
    # Monorepo umbrella tools/
    umbrella = HERE.parents[3] / "tools" if len(HERE.parents) > 3 else None
    if umbrella and (umbrella / "tibia_paths.py").is_file():
        return umbrella, HERE.parents[2]
    raise SystemExit("[fetch-assets] tools/tibia_paths.py not found")


def _tools_dir() -> Path:
    return _layout()[0]


def _find_extract() -> Path:
    tools, root = _layout()
    for cand in (
        tools / "extract" / "cli.py",
        root / "tools" / "extract" / "cli.py",
    ):
        if cand.is_file():
            return cand
    raise SystemExit(
        "[fetch-assets] tools/extract/cli.py not found next to this helper"
    )


def _detect_out(explicit: Path | None) -> Path:
    if explicit:
        return explicit.expanduser().resolve()
    _, package_root = _layout()
    for root in (Path.cwd(), package_root, HERE):
        if (root / "client").exists() or (root / "client.exe").exists():
            return root.resolve()
        if (root / "data").is_dir() or (root / "src" / "tibia").is_dir():
            return root.resolve()
    return Path.cwd().resolve()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Copy CIP assets from a local official Tibia install for qtclient."
    )
    ap.add_argument(
        "--tibia",
        default=os.environ.get("TIBIA"),
        help="Official Tibia.app / package (default: OS probe / $TIBIA)",
    )
    ap.add_argument(
        "--out",
        type=Path,
        help="Install root to populate (default: dir with client, else cwd). "
        "Writes data/assets + data/sounds (+ graphics_resources.rcc).",
    )
    ap.add_argument(
        "--link-resources",
        action="store_true",
        help="Also symlink Resources → data next to client (macOS Contents or bare).",
    )
    args = ap.parse_args(argv)

    tools = _tools_dir()
    sys.path.insert(0, str(tools))
    from tibia_paths import resolve_tibia, usage_hint  # noqa: E402

    try:
        tibia = resolve_tibia(args.tibia)
    except FileNotFoundError:
        print(usage_hint(), file=sys.stderr)
        return 1

    out = _detect_out(args.out)
    assets_dir = out / "data" / "assets"
    print(f"[fetch-assets] Tibia: {tibia}")
    print(f"[fetch-assets] out:   {out}")
    print(f"[fetch-assets] assets → {assets_dir}")

    extract_py = _find_extract()
    cmd = [
        sys.executable,
        str(extract_py),
        f"--tibia={tibia}",
        f"--assets-dir={assets_dir}",
        "--skip-cip",
        "--skip-game-tables",
    ]
    env = os.environ.copy()
    # Ensure tibia_paths import works for extract.
    env["PYTHONPATH"] = os.pathsep.join(
        [str(tools), env.get("PYTHONPATH", "")]
    ).rstrip(os.pathsep)
    rc = subprocess.call(cmd, env=env)
    if rc != 0:
        return rc

    if args.link_resources:
        _, package_root = _layout()
        client = next(
            (
                out / name
                for name in ("client", "client.exe", "tibia")
                if (out / name).exists()
            ),
            None,
        )
        if client is None:
            apps = list(out.glob("*.app/Contents/MacOS/*"))
            client = apps[0] if apps else None
        if client is not None:
            link_sh = package_root / "deploy" / "link-cip-resources.sh"
            if link_sh.is_file():
                subprocess.call(
                    ["bash", str(link_sh), str(client), str(out / "data")]
                )
            else:
                if "Contents/MacOS" in str(client):
                    res = client.parent.parent / "Resources"
                else:
                    res = client.parent / "Resources"
                if not res.exists():
                    res.symlink_to(out / "data")
                    print(f"[fetch-assets] Resources → {out / 'data'}")

    print("[fetch-assets] done — run ./client (or make client-start)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
