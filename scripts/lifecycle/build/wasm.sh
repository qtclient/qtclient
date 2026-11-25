#!/usr/bin/env bash
# Build qtclient wasm tip (optional QTCLIENT_PACKS → static-ABI pack archives).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
exec bash "$HERE/desktop.sh" --wasm "$@"
