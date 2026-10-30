#!/usr/bin/env bash
# Pack the qtclient SDK for out-of-tree MODULE/SIDE_MODULE builds.
# Usage: pack-client-sdk.sh <dest-dir>
# Env: QTCLIENT_HOST_BUILD_ID (full SHA), SOURCE_COMMIT, GITHUB_SHA
#
# Includes generated cipgen/ so IdleOT QTCLIENT_MOD_INTERNAL packs compile
# without a Soft host build tree.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/.." && pwd)"
DEST="${1:?dest dir}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo unknown)"
fi

rm -rf "$DEST"
mkdir -p "$DEST/include/qtclient" "$DEST/cmake" "$DEST/client/tibia" "$DEST/cipgen"

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

# Generate CIP QObject bases into the SDK (cip_gen.h on the include path).
python3 "$OTC/tools/cip/gen_cpp.py" \
  "$OTC/client/upstream/contract" \
  "$DEST/cipgen" \
  --chunks 12 \
  --qml "$OTC/client/upstream/qml/qmlcomponents"
test -f "$DEST/cipgen/cip_gen.h"

cat >"$DEST/cmake/QtClientConfig.cmake" <<EOF
# qtclient client-sdk (hostBuildId=${HOST_ID})
set(QTCLIENT_ROOT "\${CMAKE_CURRENT_LIST_DIR}/..")
set(QTCLIENT_HOST_BUILD_ID "${HOST_ID}")
set(TIBIA_ROOT "\${QTCLIENT_ROOT}" CACHE PATH "qtclient SDK root" FORCE)
list(APPEND CMAKE_MODULE_PATH "\${QTCLIENT_ROOT}/cmake")
include("\${QTCLIENT_ROOT}/cmake/QtClientMod.cmake")
if(NOT TARGET QtClient::Host)
  add_library(QtClient::Host INTERFACE IMPORTED)
  set_target_properties(QtClient::Host PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "\${QTCLIENT_ROOT}/include;\${QTCLIENT_ROOT}/include/engine;\${QTCLIENT_ROOT}/cipgen;\${QTCLIENT_ROOT}/client/tibia;\${QTCLIENT_ROOT}/client/tibia/engine")
endif()
set(QtClient_FOUND TRUE)
EOF

# Canonical name + deprecated Soft host-sdk alias.
cat >"$DEST/client-sdk.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
cp -f "$DEST/client-sdk.json" "$DEST/host-sdk.json"

echo "[pack-client-sdk] hostBuildId=${HOST_ID} → ${DEST}"
