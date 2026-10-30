# CipSoft Qt toolchain

Product Qt is built from [CipSoft-Tibia/Qt5](https://github.com/CipSoft-Tibia/Qt5)
(`third_party/qt5-cip`, patched 6.10.3). LegacyControls is
[CipSoft-Tibia/Qt5LegacyControls](https://github.com/CipSoft-Tibia/Qt5LegacyControls).

Warm prefixes are published by the public [qtclient](https://github.com/qtclient/qtclient) CI:

`ghcr.io/qtclient/qt:<sha12>-<kit>`

## `make qt`

1. Use local `$QT_ROOT/cip-<sha>/<kit>` if already installed.
2. Else pull via [oras](https://oras.land) (`QT_GHCR_IMAGE`, default above).
3. Else **fail** (no silent compile — that hid Soft Client:ci GHCR misses).

Compile only with Soft **Qt: build**, or locally `make qt BUILD=1` /
`install-qt.sh --build` (then `PUBLISH=1` to push). Soft Client:ci /
`ci-setup.sh` always pull-only.

`WASM=1` pulls `wasm_multithread` after the host kit; emsdk pin is in
`qt-version.sh`. `ensure_emsdk` must not use a local named `EMSDK` (it would
swallow `emsdk_env.sh`’s export and break `-platform wasm-emscripten`).

**Windows:** build with MSVC (`-platform win32-msvc`). Git Bash often prefers
`C:/mingw64`; `install-qt.sh` strips MinGW from `PATH` and requires `cl.exe`
(CI uses `ilammy/msvc-dev-cmd`). A MinGW-built `qsb.exe` crashes while baking
`.qsb` shaders.

**macOS arm64:** `qtmultimedia` is skipped (not used by tibia). Otherwise
configure can pick Intel FFmpeg from `/usr/local/lib` and fail linking
`libffmpegmediaplugin.dylib`. After changing skip lists, wipe
`$QT_ROOT/build/cip-*-host` so configure re-runs.

`PUBLISH=1` pushes the installed prefix to GHCR after a successful build (CI publisher workflow).

Do **not** commit Qt binaries into git — only GHCR / Actions cache.

## Submodule notes

- `qt5-cip` has `update = none` so recursive umbrella clones stay small.
- After init, `install-qt.sh` applies a cone sparse-checkout (`QT_SPARSE_MODULES`)
  so skipped modules (e.g. `qtwebengine`) never sit in the worktree — otherwise
  IDE git status hits “too many active changes”.
- Init sources only when compiling: `make qt BUILD=1` / Soft Qt: build.

