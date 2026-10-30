#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/.." && pwd)"
export PACKAGE_KIND=desktop
while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform) export PLATFORM="${2:?}"; shift 2 ;;
    --bin) export BIN_DIR="${2:?}"; shift 2 ;;
    --out) export PACKAGE_OUT="${2:?}"; shift 2 ;;
    *) echo "unknown: $1" >&2; exit 1 ;;
  esac
done
exec bash "$OTC/scripts/lifecycle/package.sh"
