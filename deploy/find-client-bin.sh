#!/usr/bin/env bash
# Resolve the Soft Play desktop binary under a build preset root.
# CipSoft OUTPUT_NAME is `client` → client.app / client; product pack brands the .app (QtClient.app when qtclient is linked).
# Prints one absolute path or exits 1. Usage: find-client-bin.sh <build-root>
set -euo pipefail

root=${1:?build root}
[[ -d "$root" ]] || exit 1

for p in \
    '*QtClient.app/Contents/MacOS/client' \
    '*client.app/Contents/MacOS/client' \
    '*/client/tibia/client' \
    '*/client/tibia/tibia' \
    '*/bin/client' \
    '*/bin/tibia' \
    '*/client.exe' \
    '*/tibia.exe'; do
  hit="$(find "$root" -path "$p" -type f 2>/dev/null | head -1 || true)"
  if [[ -n "$hit" ]]; then
    printf '%s\n' "$hit"
    exit 0
  fi
done
# Fallback: any executable named client/tibia under the build root
hit="$(find "$root" -type f \( -name client -o -name tibia -o -name client.exe -o -name tibia.exe \) 2>/dev/null | head -1 || true)"
[[ -n "$hit" ]] || exit 1
printf '%s\n' "$hit"
