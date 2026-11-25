#!/usr/bin/env bash
set -euo pipefail
QTCLIENT="$(cd "$(dirname "$0")/../../.." && pwd)"
rm -rf "$QTCLIENT/build"/*/launcher 2>/dev/null || true
echo "[qtclient] cleaned launcher"
