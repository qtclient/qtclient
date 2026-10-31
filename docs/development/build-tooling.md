# Build tooling (tibia client)

Blank-slate stack for `apps/qtclient`. Desktop and **wasm are equal target OSes**.

## Stack

| Layer | Choice |
|-------|--------|
| Build | CMake + Ninja + CMakePresets |
| Qt / emsdk | CipSoft `third_party/qt5-cip` via `make qt` / `make qt WASM=1` (+ emsdk 4.0.7); warm GHCR prefixes |
| LegacyControls | `third_party/Qt5LegacyControls` (LGPL submodule) |
| C++ deps | vcpkg **manifest mode** (`vcpkg.json` + baseline) |
| Dep cache | `VCPKG_BINARY_SOURCES` under monorepo `.cache/vcpkg/` |
| Compile cache | ccache (preferred in CI) or sccache |

CrystalServer also uses vcpkg; share baseline pins where versions overlap. Qt stays outside vcpkg and tracks CipSoft-Tibia/Qt5 (not stock aqt).

`qt5-cip` is `update = none` in `.gitmodules` so umbrella `git submodule update --recursive` does not clone ~multi-GB sources. `make qt` initializes it when a local build is required (after GHCR miss).

## Prerequisites

```bash
# vcpkg (same convention as Crystal)
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
# Optional: oras for GHCR warm Qt pulls (https://oras.land)
# Qt — desktop kit, then WASM (builds host kit first)
make qt          # desktop CIP Qt → ~/Qt/cip-<sha>/…
make qt WASM=1   # + wasm_multithread + emsdk
# CI publishers: PUBLISH=1 make qt   # pushes ghcr.io/qtclient/qt:<sha>-<kit>
```

## Build

```bash
make client-build          # umbrella: disk QML / hot reload
make -C apps/qtclient client-build   # production embedded QML
make tibia-wasm            # browser (needs make qt WASM=1)
```

Presets (`macos-dev`, `linux-release`, `windows-release`, `wasm-tibia`, …) all use the vcpkg toolchain. Wasm chainloads Qt’s `qt.toolchain.cmake` and the `wasm32-emscripten` triplet (SJLJ / pthread ABI aligned with Qt wasm).

## Dependencies

Declared in [`vcpkg.json`](../../vcpkg.json). Product needs APNG → `libpng` feature `apng`. No host zlib/protobuf/fmt.

## Binary cache

Local presets use monorepo `.cache/vcpkg/archives`. CI uses Soft composites
[`.github/actions/cache-restore`](../../.github/actions/cache-restore/action.yml) /
[`cache-save`](../../.github/actions/cache-save/action.yml):

| Layer | Path | Persistence |
|-------|------|-------------|
| vcpkg archives | `workspace/.cache/vcpkg` | Actions `vcpkg-{triplet}-{baseline12}-{sha}` (+ restore-keys) and public GHCR `ghcr.io/qtclient/vcpkg-cache:{triplet}-{baseline12}` |
| ccache | `workspace/.cache/ccache` | Actions `ccache-{scope}-{os}-{sha}` only (no GHCR; high churn) |

Primary keys include `{sha}` so each run can **save** a new tip (Actions keys are immutable). Restore-keys warm from the previous tip. Do not use `save-always` on `actions/cache@v4` — it is broken; use restore + save.

Release pack and Crystal call the same Soft actions (`uses: qtclient/qtclient/.github/actions/...` or `./_qtclient/.github/actions/...`). Qt prefixes stay on `ghcr.io/qtclient/qt:<sha>-<kit>` (not Actions).
