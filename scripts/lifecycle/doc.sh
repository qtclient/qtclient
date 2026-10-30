#!/usr/bin/env bash
# qtclient doc
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
COMP="${COMP:-client}"
echo "docs: $OTC/docs/mods.md $OTC/README.md (comp=$COMP)"
