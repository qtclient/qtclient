#!/usr/bin/env bash
# Build stock CIP tip launcher (Qt Widgets).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
exec bash "$HERE/desktop.sh" --launcher "$@"
