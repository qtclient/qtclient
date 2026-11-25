#!/usr/bin/env bash
set -euo pipefail
QTCLIENT="$(cd "$(dirname "$0")/../../.." && pwd)"
rm -rf "$QTCLIENT/build"/*-dev "$QTCLIENT/build"/macos "$QTCLIENT/build"/linux "$QTCLIENT/build"/windows 2>/dev/null || true
echo "[qtclient] cleaned desktop client build trees"
