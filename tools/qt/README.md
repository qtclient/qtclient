# Qt toolchain

Product Qt is built from [qtclient/Qt5](https://github.com/qtclient/Qt5), our fork
of [CipSoft-Tibia/Qt5](https://github.com/CipSoft-Tibia/Qt5) (`third_party/qt5-cip`,
patched 6.10.3). `master` = CipSoft's tree plus JSPI-safe wasm
`QNetworkReply` and `QWebSocket` (browser fetch / WebSocket through
`QWasmSuspendResumeControl`). Rebase it when CipSoft moves Qt.

### LegacyControls

| Part | Source |
|------|--------|
| **QML** | `make tibia-sync` — carved from the install plugin (`libQt5LegacyControls.dylib` / `.dll`) into `client/upstream/qml/QtQuick/LegacyControls/` |
| **C++** | LGPL submodule [`CipSoft-Tibia/Qt5LegacyControls`](https://github.com/CipSoft-Tibia/Qt5LegacyControls) on branch `main` (not hash-pinned) |

The install tip owns QML. The GitHub repo is the **C++ reference** (`abstractstyle`, `calendarmodel`, `rangemodel`, `wheelarea`, …) compiled into `tibia_legacycontrols`. When sync QML drifts ahead of the submodule tip, bump `third_party/Qt5LegacyControls` to current `main` and adjust C++ if CipSoft changed native types. Do not treat submodule QML as source of truth.

## Kits

Kits are built and published by [qtclient/Qt5](https://github.com/qtclient/Qt5)
itself ("qtclient kits" workflow, `qtclient/build-kit.sh` there) for each commit
of its `master`: `ghcr.io/qtclient/qt5:<sha12>-<kit>` and the GitHub Release
`qt-<sha12>` of that repository. This repository only pulls them; Qt build
flags, the module skip list and the Emscripten pin live there (see its
`qtclient/README.md`).

### `make qt`

1. Use local `$QT_ROOT/cip-<sha12>/<kit>` if already installed.
2. Else pull via [oras](https://oras.land) (`QT_GHCR_IMAGE`, default above).
3. Else the qtclient/Qt5 release asset.
4. Else **fail**. `--pull-only` / `QT_PULL_ONLY=1` (CI) waits for a kit that is
   still being built first.

`<sha12>` is the `third_party/qt5-cip` commit. To change Qt: commit to
qtclient/Qt5 `master`, wait for its kits, then bump the submodule here.

`WASM=1` pulls `wasm_multithread` after the host kit and sets up its emsdk
(`QT_EMSDK_VERSION` in `qt-version.sh`, which must equal the kit's recorded
`emsdk-version`). `ensure_emsdk` must not use a local named `EMSDK` (it would
swallow `emsdk_env.sh`’s export and break `-platform wasm-emscripten`). The
wasm kit has `-feature-wasm-exceptions` and `-feature-wasm-jspi` (the /play
file system suspends main); the pull checks both.

### Desktop WebEngine (Phase E)

The kits skip `qtwebengine`. The qtclient tip links real **Qt WebEngineQuick +
WebChannel** on desktop when those targets exist under `CMAKE_PREFIX_PATH`;
wasm/android/ios keep inert `client/tibia/qmlstubs`. For CIP parity, build a
desktop kit with `qtwebengine` from a qtclient/Qt5 checkout (drop it from the
skip list in `qtclient/build-kit.sh`) and reconfigure the client after the
prefix includes `lib/cmake/Qt6WebEngineQuick`.

**MoltenVK:** macOS Vulkan (`rendererIndex` 7) expects `Contents/Frameworks/libMoltenVK.dylib`
and `MoltenVK_icd.json`. CMake POST_BUILD and `package_desktop.py` copy from
`TIBIA_MOLTENVK_DIR`, `VULKAN_SDK`, or Homebrew `molten-vk` when present.

Do **not** commit Qt binaries into git — kits live on GHCR / releases.

## Submodule notes

- `qt5-cip` has `update = none` so recursive umbrella clones stay small.
- Check it out only to work on Qt itself (qtclient/Qt5 has its own build
  scripts); qtclient never builds Qt.
