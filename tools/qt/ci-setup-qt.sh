#!/usr/bin/env bash
# Backward-compatible forwarder → tools/qt/ci-setup.sh
# Prefer: tools/qt/ci-setup.sh [--wasm] [--pull-only] [--with-vcpkg] [--with-deps]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
exec bash "$HERE/ci-setup.sh" "$@"
