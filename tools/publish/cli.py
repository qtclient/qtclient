#!/usr/bin/env python3
"""Publish qtclient CDN bundle slices.

Usage:
  python3 tools/publish/cli.py assets [--push] [things|store|outfits|items]   # --push → ghcr cdn-assets
  python3 tools/publish/cli.py desktop --platform <linux-x64|windows-x64|macos-arm64> --bin <dir> --engine-files [--content-files]
  python3 tools/publish/cli.py cdn [--wasm] [--desktop]   # local dist/cdn from dist/slices
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OTC = HERE.parents[1]
UMBRELLA = OTC.parent.parent if OTC.parent.name == "apps" else OTC.parent


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] in {"-h", "--help"}:
        print(__doc__)
        return 0
    cmd = sys.argv[1]
    rest = sys.argv[2:]

    if cmd == "assets":
        return subprocess.call(
            ["bash", str(HERE / "publish_assets.sh"), *rest],
            cwd=str(UMBRELLA),
        )
    if cmd == "desktop":
        return subprocess.call(
            [sys.executable, str(HERE / "package_desktop.py"), *rest],
            cwd=str(UMBRELLA),
        )
    if cmd == "cdn":
        return subprocess.call([sys.executable, str(HERE / "local_cdn.py"), *rest], cwd=str(UMBRELLA))
    print(f"unknown command: {cmd}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
