#!/usr/bin/env bash
# shellcheck shell=bash
# Shared pack_* helpers (HOST_ID, OTC must be set).
pack_sdk() {
  # QtClientSdk: include/qtclient/abi + lib/cmake/QtClientSdk (QtClientMod.cmake, config
  # recording ABI version / hostBuildId / Qt kit) + share/qtclient-sdk. A pack builds
  # with this tree and its Qt kit alone (find_package(QtClientSdk)); no tip closure,
  # no cipgen, no protobuf, no host sources.
  local DEST="$1"
  local args=()
  case "${COMP:-}" in browser|wasm) args=(--wasm) ;; esac
  QTCLIENT_SDK_DIR="$DEST" QTCLIENT_HOST_BUILD_ID="$HOST_ID" \
    bash "$QTCLIENT/scripts/lifecycle/build/sdk.sh" ${args[@]+"${args[@]}"} \
    || { echo "[package] error: sdk install failed" >&2; return 1; }
  test -f "$DEST/include/qtclient/abi/host_api_v1.h" \
    || { echo "[package] error: SDK missing abi headers" >&2; return 1; }
  if find "$DEST/include/qtclient" -name '*.h' ! -path '*/qtclient/abi/*' | grep -q .; then
    echo "[package] error: SDK must ship include/qtclient/abi only" >&2
    return 1
  fi
  # Legacy names read by release scripts (ensure-qtclient-host.sh / pull-client-sdk.sh).
  cp -f "$DEST/share/qtclient-sdk/sdk.json" "$DEST/client-sdk.json"
  cp -f "$DEST/share/qtclient-sdk/sdk.json" "$DEST/host-sdk.json"
  echo "[package] sdk hostBuildId=${HOST_ID} → ${DEST}"
}

pack_desktop() {
  local PLATFORM="$1" BIN="$2" DEST="$3"
  export QTCLIENT_APP_NAME="${QTCLIENT_APP_NAME:-qtclient}"
  export QTCLIENT_BUNDLE_ID="${QTCLIENT_BUNDLE_ID:-com.qtclient.client}"
  export QTCLIENT_STORE_META="${QTCLIENT_STORE_META:-.qtclient}"

  rm -rf "$DEST"
  mkdir -p "$DEST/bin" "$DEST/engine"

  python3 "$QTCLIENT/tools/publish/package_desktop.py" \
    --platform "$PLATFORM" \
    --bin "$BIN" \
    --engine-bundle "$DEST/engine"

  for name in client launcher; do
    if [[ -f "$BIN/$name" ]]; then
      cp -a "$BIN/$name" "$DEST/bin/$name"
    elif [[ -f "$BIN/$name.exe" ]]; then
      cp -a "$BIN/$name.exe" "$DEST/bin/$name.exe"
    elif [[ -d "$BIN/$name.app" ]]; then
      cp -a "$BIN/$name.app" "$DEST/bin/$name.app"
    fi
  done

  cat >"$DEST/host.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "${PLATFORM}",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
  echo "[package] desktop ${PLATFORM} hostBuildId=${HOST_ID} → ${DEST}"
}

pack_wasm() {
  local SRC="$1" DEST="$2"
  export TIBIA_ENGINE_SHA="$HOST_ID"
  rm -rf "$DEST"
  mkdir -p "$DEST"
  bash "$QTCLIENT/scripts/lifecycle/package/pack-qtclient-wasm.sh" "$SRC" "$DEST"
  cat >"$DEST/host.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "wasm",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
  echo "[package] wasm hostBuildId=${HOST_ID} → ${DEST}"
}

