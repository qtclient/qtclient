#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
export DEV="${DEV:-}" DEBUG="${DEBUG:-}"
bash "$HERE/../build/launcher.sh"
