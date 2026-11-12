#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../../.." && pwd)"
rm -rf "$OTC/build"/*-dev "$OTC/build"/macos "$OTC/build"/linux "$OTC/build"/windows 2>/dev/null || true
echo "[qtclient] cleaned desktop client build trees"
