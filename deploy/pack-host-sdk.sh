#!/usr/bin/env bash
# Pack the qtclient host SDK for out-of-tree MODULE/SIDE_MODULE builds.
# Usage: pack-host-sdk.sh <dest-dir>
# Env: QTCLIENT_HOST_BUILD_ID (full SHA), SOURCE_COMMIT, GITHUB_SHA
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/.." && pwd)"
DEST="${1:?dest dir}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo unknown)"
fi

rm -rf "$DEST"
mkdir -p "$DEST/include/qtclient" "$DEST/cmake" "$DEST/client/tibia"

# Public Mod API + client/tibia headers (IdleOT QTCLIENT_MOD_INTERNAL still needs
# deep headers during migration; new packs should use mod_api_v1.h only).
cp -a "$OTC/include/qtclient/." "$DEST/include/qtclient/"
rsync -a --include '*/' --include '*.h' --include '*.hpp' --exclude '*' \
  "$OTC/client/tibia/" "$DEST/client/tibia/"
# Mirror under include/ for -I${SDK}/include convenience.
mkdir -p "$DEST/include"
rsync -a --include '*/' --include '*.h' --include '*.hpp' --exclude '*' \
  "$OTC/client/tibia/" "$DEST/include/"

cp -a "$OTC/cmake/QtClientMod.cmake" "$DEST/cmake/"
cp -a "$OTC/cmake/QtClientHost.cmake" "$DEST/cmake/"
cp -a "$OTC/cmake/GeneratePackQmldir.cmake" "$DEST/cmake/"
cp -a "$OTC/cmake/QtClientConfig.cmake.in" "$DEST/cmake/" 2>/dev/null || true

cat >"$DEST/cmake/QtClientConfig.cmake" <<EOF
# qtclient host-sdk (hostBuildId=${HOST_ID})
set(QTCLIENT_ROOT "\${CMAKE_CURRENT_LIST_DIR}/..")
set(QTCLIENT_HOST_BUILD_ID "${HOST_ID}")
set(TIBIA_ROOT "\${QTCLIENT_ROOT}" CACHE PATH "qtclient SDK root" FORCE)
list(APPEND CMAKE_MODULE_PATH "\${QTCLIENT_ROOT}/cmake")
include("\${QTCLIENT_ROOT}/cmake/QtClientMod.cmake")
if(NOT TARGET QtClient::Host)
  add_library(QtClient::Host INTERFACE IMPORTED)
  set_target_properties(QtClient::Host PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "\${QTCLIENT_ROOT}/include;\${QTCLIENT_ROOT}/client/tibia")
endif()
set(QtClient_FOUND TRUE)
EOF

cat >"$DEST/host-sdk.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

echo "[pack-host-sdk] hostBuildId=${HOST_ID} → ${DEST}"
