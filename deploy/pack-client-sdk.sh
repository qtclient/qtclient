#!/usr/bin/env bash
# Compat — prefer: PACKAGE_KIND=sdk PACKAGE_OUT=… scripts/lifecycle/package.sh
set -euo pipefail
OTC="$(cd "$(dirname "$0")/.." && pwd)"
export PACKAGE_KIND=sdk PACKAGE_OUT="${1:?dest dir}"
exec bash "$OTC/scripts/lifecycle/package.sh"
